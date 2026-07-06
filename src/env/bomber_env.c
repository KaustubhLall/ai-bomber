#include "env/env.h"
#include "env/bomber_map.h"
#include "env/bomber_bombs.h"
#include "env/bomber_blast.h"
#include "env/bomber_danger.h"
#include "env/bomber_observation.h"
#include "env/bomber_reward.h"
#include "env/bomber_rules.h"
#include "core/math_util.h"
#include <string.h>
#include <stdio.h>

void env_init(BomberEnv* env, const BomberConfig* config) {
    memset(env, 0, sizeof(BomberEnv));
    env->config = *config;
    rng_init(&env->rng, (uint64_t)config->seed);
    env_reset(env, (uint64_t)config->seed);
}

void env_reset(BomberEnv* env, uint64_t seed) {
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
    StepResult result = {0.0f, 0, TERMINAL_NONE};
    BomberState* state = &env->state;
    BomberAgentState* agent = &state->agents[0];
    const BomberConfig* cfg = &env->config;

    /* Track pre-step state for reward computation */
    int prev_crates = map_count_crates(state);
    int prev_enemies_alive = 0;
    for (int a = 1; a < state->agent_count; a++) {
        if (state->agents[a].alive) prev_enemies_alive++;
    }
    int was_in_danger = (env->danger.time_to_blast[agent->y][agent->x] >= 0) ? 1 : 0;

    /* Record action */
    rb_push(&env->action_history, (int)action);

    /* Execute action for agent 0 */
    int action_valid = 1;
    env->prev_agent_x = agent->x;
    env->prev_agent_y = agent->y;

    switch (action) {
        case ACTION_UP:
        case ACTION_DOWN:
        case ACTION_LEFT:
        case ACTION_RIGHT:
            if (!rules_try_move(state, 0, action)) {
                action_valid = 0;
            }
            break;
        case ACTION_PLACE_BOMB:
            if (!rules_try_place_bomb(state, 0, cfg->bomb_timer)) {
                action_valid = 0;
            }
            break;
        case ACTION_WAIT:
            break;
        default:
            action_valid = 0;
            break;
    }

    /* Track progress for stall detection */
    if (agent->x != env->prev_agent_x || agent->y != env->prev_agent_y) {
        env->steps_since_progress = 0;
    } else {
        env->steps_since_progress++;
    }

    /* Pick up powerups */
    rules_pickup_powerup(state, 0);

    /* Simple enemy AI: move randomly (placeholder for scripted enemy bot) */
    for (int a = 1; a < state->agent_count; a++) {
        if (!state->agents[a].alive) continue;
        /* Random walk: try a random direction */
        int dirs[4] = {ACTION_UP, ACTION_DOWN, ACTION_LEFT, ACTION_RIGHT};
        int shuffled[4];
        for (int i = 0; i < 4; i++) shuffled[i] = dirs[i];
        /* Fisher-Yates with rng */
        for (int i = 3; i > 0; i--) {
            int j = rng_range(&env->rng, 0, i + 1);
            int tmp = shuffled[i]; shuffled[i] = shuffled[j]; shuffled[j] = tmp;
        }
        int moved = 0;
        for (int i = 0; i < 4; i++) {
            if (rules_try_move(state, a, (Action)shuffled[i])) {
                moved = 1;
                break;
            }
        }
        rules_pickup_powerup(state, a);
        /* Occasionally place bombs */
        if (moved && state->agents[a].bomb_ammo > 0 && rng_bool(&env->rng)) {
            rules_try_place_bomb(state, a, cfg->bomb_timer);
        }
    }

    /* Tick bombs (decrement timers, explode at zero) */
    tick_bombs(state);

    /* Recompute danger map */
    danger_compute(&env->danger, state);
    danger_compute_escape(&env->danger, state, 0);

    /* Advance step counter */
    state->step++;

    /* Compute reward */
    float reward = reward_compute(&env->last_reward, env, action, 0,
                                   prev_crates, 0, prev_enemies_alive, was_in_danger);

    /* Apply invalid action penalty */
    if (!action_valid) {
        env->last_reward.invalid_action_penalty = cfg->invalid_action_penalty;
        reward += cfg->invalid_action_penalty;
        env->last_reward.total = reward;
    }

    state->total_reward += reward;

    /* Check terminal */
    TerminalReason terminal = rules_check_terminal(state, 0, cfg->max_steps);
    result.reward = reward;
    result.terminal_reason = terminal;
    result.done = (terminal != TERMINAL_NONE) ? 1 : 0;

    return result;
}

void env_observe(const BomberEnv* env, int agent_id, Observation* obs) {
    obs_compute(&env->state, &env->danger, agent_id,
                rb_get(&env->action_history, rb_size(&env->action_history) - 1), obs);
}

void env_get_debug_snapshot(const BomberEnv* env, DebugSnapshot* out) {
    memset(out, 0, sizeof(DebugSnapshot));
    out->state = env->state;
    out->danger = env->danger;
    out->last_reward = env->last_reward;
    out->last_action = (Action)rb_get(&env->action_history, rb_size(&env->action_history) - 1);
    out->cumulative_reward = env->state.total_reward;
    out->determinism_ok = 1;

    /* Generate decision text */
    const char* action_names[] = {"UP", "DOWN", "LEFT", "RIGHT", "BOMB", "WAIT"};
    if (out->last_action >= 0 && out->last_action < ACTION_COUNT) {
        snprintf(out->decision_text, sizeof(out->decision_text),
                 "Action: %s | Reward: %.3f | Step: %d",
                 action_names[out->last_action], out->last_reward.total, env->state.step);
    }
}
