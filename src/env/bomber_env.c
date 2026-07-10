#include "env/env.h"
#include "env/bomber_map.h"
#include "env/bomber_bombs.h"
#include "env/bomber_blast.h"
#include "env/bomber_danger.h"
#include "env/bomber_observation.h"
#include "env/bomber_reward.h"
#include "env/bomber_rules.h"
#include "core/math_util.h"
#include "agents/agent.h"
#include <string.h>
#include <stdio.h>

#define OPPONENT_WIRING_MAGIC UINT32_C(0xA17E0F05)

void env_init(BomberEnv* env, const BomberConfig* config) {
    Agent* opponent = env->opponent_wiring_magic == OPPONENT_WIRING_MAGIC
        ? env->opponent : NULL;
    memset(env, 0, sizeof(BomberEnv));
    env->opponent = opponent;
    env->opponent_wiring_magic = OPPONENT_WIRING_MAGIC;
    env->config = *config;
    config_normalize(&env->config);
    rng_init(&env->rng, (uint64_t)env->config.seed);
    env_reset(env, (uint64_t)env->config.seed);
}

void env_reset(BomberEnv* env, uint64_t seed) {
    config_normalize(&env->config);
    rng_init(&env->rng, seed);
    map_generate(&env->state, &env->config, &env->rng);
    rb_init(&env->action_history);
    env->prev_agent_x = env->state.agents[0].x;
    env->prev_agent_y = env->state.agents[0].y;
    env->steps_since_progress = 0;
    memset(&env->last_reward, 0, sizeof(RewardBreakdown));
    danger_compute(&env->danger, &env->state);
    danger_compute_escape(&env->danger, &env->state, 0);
}

StepResult env_step(BomberEnv* env, Action action) {
    Action actions[MAX_AGENTS] = {ACTION_WAIT};
    actions[0] = action;
    for (int a = 1; a < env->state.agent_count; a++) {
        if (!env->state.agents[a].alive) continue;
        if (env->opponent) {
            Observation obs;
            DebugSnapshot snap;
            env_observe(env, a, &obs);
            env_get_debug_snapshot(env, &snap);
            actions[a] = agent_act(env->opponent, &obs, &snap);
        } else {
            Action legal[ACTION_COUNT];
            int count = 0;
            env_legal_actions(env, a, legal, &count);
            int movable = 0;
            while (movable < count && legal[movable] <= ACTION_RIGHT) movable++;
            actions[a] = movable > 0 ? legal[rng_range(&env->rng, 0, movable)] : ACTION_WAIT;
        }
    }
    return env_step_joint(env, actions, env->state.agent_count);
}

void env_copy(BomberEnv* dst, const BomberEnv* src) {
    if (dst && src && dst != src) memcpy(dst, src, sizeof(*dst));
}

void env_legal_actions(const BomberEnv* env, int agent_id, Action* out, int* count) {
    int n = 0;
    if (!count) return;
    *count = 0;
    if (!env || !out || agent_id < 0 || agent_id >= env->state.agent_count ||
        !env->state.agents[agent_id].alive) return;
    const BomberAgentState* agent = &env->state.agents[agent_id];
    static const Action moves[] = {ACTION_UP, ACTION_DOWN, ACTION_LEFT, ACTION_RIGHT};
    static const int dx[] = {0, 0, -1, 1};
    static const int dy[] = {-1, 1, 0, 0};
    for (int i = 0; i < 4; i++) {
        if (map_is_walkable(&env->state, agent->x + dx[i], agent->y + dy[i])) out[n++] = moves[i];
    }
    if (agent->bomb_ammo > 0) {
        int occupied = 0;
        for (int i = 0; i < MAX_BOMBS; i++)
            if (env->state.bombs[i].active && env->state.bombs[i].x == agent->x && env->state.bombs[i].y == agent->y)
                occupied = 1;
        if (!occupied) out[n++] = ACTION_PLACE_BOMB;
    }
    out[n++] = ACTION_WAIT;
    *count = n;
}

static int movement_delta(Action action, int* dx, int* dy) {
    *dx = 0; *dy = 0;
    switch (action) {
        case ACTION_UP: *dy = -1; return 1;
        case ACTION_DOWN: *dy = 1; return 1;
        case ACTION_LEFT: *dx = -1; return 1;
        case ACTION_RIGHT: *dx = 1; return 1;
        default: return 0;
    }
}

StepResult env_step_joint(BomberEnv* env, const Action* actions, int action_count) {
    StepResult result = {0.0f, 0, TERMINAL_NONE};
    env->last_rng_before_joint = env->rng;
    BomberState* state = &env->state;
    BomberAgentState* agent = &state->agents[0];
    const BomberConfig* cfg = &env->config;
    env->last_joint_action_count = state->agent_count;
    for (int a = 0; a < state->agent_count; a++)
        env->last_joint_actions[a] = actions && a < action_count ? actions[a] : ACTION_WAIT;

    int prev_owned_crates = agent->crates_destroyed;
    int prev_enemies_alive = 0;
    for (int a = 1; a < state->agent_count; a++) {
        if (state->agents[a].alive) prev_enemies_alive++;
    }
    int was_in_danger = (env->danger.time_to_blast[agent->y][agent->x] >= 0) ? 1 : 0;

    Action action = actions && action_count > 0 ? actions[0] : ACTION_WAIT;
    rb_push(&env->action_history, (int)action);

    int action_valid = 1;
    env->prev_agent_x = agent->x;
    env->prev_agent_y = agent->y;

    int desired_x[MAX_AGENTS], desired_y[MAX_AGENTS], moving[MAX_AGENTS], valid[MAX_AGENTS];
    for (int a = 0; a < state->agent_count; a++) {
        desired_x[a] = state->agents[a].x;
        desired_y[a] = state->agents[a].y;
        moving[a] = 0;
        valid[a] = state->agents[a].alive ? 1 : 0;
        Action current = actions && a < action_count ? actions[a] : ACTION_WAIT;
        int dx, dy;
        if (movement_delta(current, &dx, &dy)) {
            moving[a] = 1;
            desired_x[a] += dx;
            desired_y[a] += dy;
            if (!map_is_walkable(state, desired_x[a], desired_y[a])) valid[a] = 0;
        } else if (current != ACTION_WAIT && current != ACTION_PLACE_BOMB) {
            valid[a] = 0;
        }
    }
    /* Resolve moves atomically. Contested destinations are rejected, while a
       destination vacated in the same tick (including a two-agent swap) is legal. */
    for (int a = 0; a < state->agent_count; a++) if (moving[a] && valid[a]) {
        for (int b = 0; b < state->agent_count; b++) if (a != b && state->agents[b].alive) {
            if (moving[b] && valid[b] && desired_x[a] == desired_x[b] && desired_y[a] == desired_y[b]) {
                valid[a] = 0;
                valid[b] = 0;
            }
        }
    }
    int changed;
    do {
        changed = 0;
        for (int a = 0; a < state->agent_count; a++) if (moving[a] && valid[a]) {
            for (int b = 0; b < state->agent_count; b++) if (a != b && state->agents[b].alive &&
                desired_x[a] == state->agents[b].x && desired_y[a] == state->agents[b].y &&
                (!moving[b] || !valid[b])) {
                valid[a] = 0; changed = 1; break;
            }
        }
    } while (changed);
    for (int a = 0; a < state->agent_count; a++) if (moving[a] && valid[a]) {
        state->agents[a].x = desired_x[a];
        state->agents[a].y = desired_y[a];
    }
    for (int a = 0; a < state->agent_count; a++) if (state->agents[a].alive) {
        Action current = actions && a < action_count ? actions[a] : ACTION_WAIT;
        if (current == ACTION_PLACE_BOMB && !rules_try_place_bomb(state, a, cfg->bomb_timer)) valid[a] = 0;
    }
    action_valid = valid[0];

    if (agent->x != env->prev_agent_x || agent->y != env->prev_agent_y) {
        env->steps_since_progress = 0;
    } else {
        env->steps_since_progress++;
    }

    int powerups_collected = rules_pickup_powerup(state, 0);

    for (int a = 1; a < state->agent_count; a++) if (state->agents[a].alive)
        (void)rules_pickup_powerup(state, a);

    tick_bombs(state, &env->rng, cfg->powerup_rate);
    /* Sudden death closes the arena inward once the clock passes the threshold, forcing a
       decisive result rather than a stalled timeout draw. */
    map_apply_sudden_death(state, cfg->sudden_death_start, cfg->shrink_interval);
    /* Detonations only lay flame; a single post-move pass applies lethal damage so the
       fire kills across its whole lifetime (canonical area denial), then it ages one tick. */
    apply_flame_damage(state);
    decay_flame(state);

    danger_compute(&env->danger, state);
    danger_compute_escape(&env->danger, state, 0);

    state->step++;

    float reward = reward_compute(&env->last_reward, env, action, 0,
                                   prev_owned_crates, powerups_collected, prev_enemies_alive, was_in_danger);

    if (!action_valid) {
        env->last_reward.invalid_action_penalty = cfg->invalid_action_penalty;
        reward += cfg->invalid_action_penalty;
        env->last_reward.total = reward;
    }

    state->total_reward += reward;

    TerminalReason terminal = rules_check_terminal(state, 0, cfg->max_steps);
    result.reward = reward;
    result.terminal_reason = terminal;
    result.done = (terminal != TERMINAL_NONE) ? 1 : 0;

    return result;
}

uint64_t env_state_hash(const BomberEnv* env) {
    const unsigned char* bytes = (const unsigned char*)&env->state;
    uint64_t hash = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < sizeof(env->state); i++) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    bytes = (const unsigned char*)&env->rng;
    for (size_t i = 0; i < sizeof(env->rng); i++) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

void env_observe(const BomberEnv* env, int agent_id, Observation* obs) {
    DangerMap perspective_danger;
    danger_compute(&perspective_danger, &env->state);
    danger_compute_escape(&perspective_danger, &env->state, agent_id);
    obs_compute(&env->state, &perspective_danger, agent_id,
                rb_get(&env->action_history, rb_size(&env->action_history) - 1), obs);
}

void env_set_opponent(BomberEnv* env, Agent* opponent) {
    env->opponent = opponent;
    env->opponent_wiring_magic = OPPONENT_WIRING_MAGIC;
}

void env_get_debug_snapshot(const BomberEnv* env, DebugSnapshot* out) {
    memset(out, 0, sizeof(DebugSnapshot));
    out->state = env->state;
    out->config = env->config;
    out->rng = env->rng;
    out->danger = env->danger;
    out->last_reward = env->last_reward;
    out->last_action = (Action)rb_get(&env->action_history, rb_size(&env->action_history) - 1);
    out->cumulative_reward = env->state.total_reward;
    out->determinism_ok = 1;

    const char* action_names[] = {"UP", "DOWN", "LEFT", "RIGHT", "BOMB", "WAIT"};
    if (out->last_action >= 0 && out->last_action < ACTION_COUNT) {
        snprintf(out->decision_text, sizeof(out->decision_text),
                 "Action: %s | Reward: %.3f | Step: %d",
                 action_names[out->last_action], out->last_reward.total, env->state.step);
    }
}
