#include "viz/viz_session.h"
#include "env/bomber_map.h"
#include <string.h>
#include <stdio.h>

void viz_session_init(VizSession* vs, int max_epochs, uint64_t base_seed) {
    memset(vs, 0, sizeof(VizSession));
    vs->max_epochs = max_epochs;
    vs->base_seed = base_seed;
    vs->active_session = 0;
    vs->paused = 0;
    vs->speed_mult = 1;
    vs->target_fps = 60;
    vs->show_help = 1;
    vs->step_once = 0;
    vs->auto_advance_epoch = 1;
    vs->view_mode = VIEW_ARENA;
    vs->show_danger = 1;
    vs->show_obs = 1;
    vs->show_local_obs = 1;
    vs->show_legend = 1;
    vs->show_observation_window = 0;
    vs->show_grid = 0;
}

int viz_session_add_agent(VizSession* vs, AgentType type, const char* name,
                          AgentType opponent_type, const char* opponent_name,
                          int has_opponent_policy, const BomberConfig* config) {
    if (vs->session_count >= MAX_VIZ_AGENTS) return -1;
    int idx = vs->session_count;
    AgentSession* s = &vs->sessions[idx];

    memset(s, 0, sizeof(AgentSession));
    s->type = type;
    s->opponent_type = opponent_type;
    s->has_opponent_policy = has_opponent_policy;
    s->config = *config;
    strncpy(s->name, name, sizeof(s->name) - 1);
    strncpy(s->opponent_name, has_opponent_policy ? opponent_name : "built-in-random",
            sizeof(s->opponent_name) - 1);

    agent_init(&s->agent, type);
    env_init(&s->env, &s->config);
    if (has_opponent_policy) {
        agent_init(&s->opponent, opponent_type);
        env_set_opponent(&s->env, &s->opponent);
    }
    env_reset(&s->env, vs->base_seed);
    agent_reset(&s->agent, vs->base_seed);
    if (has_opponent_policy) agent_reset(&s->opponent, vs->base_seed ^ UINT64_C(0x9E3779B97F4A7C15));

    dashboard_init(&s->dashboard);
    s->epoch_count = 0;
    s->episode_done = 0;

    vs->session_count++;
    return idx;
}

void viz_session_reset_epoch(VizSession* vs, int session_idx) {
    if (session_idx < 0 || session_idx >= vs->session_count) return;
    AgentSession* s = &vs->sessions[session_idx];

    uint64_t ep_seed = vs->base_seed + (uint64_t)s->epoch_count;
    env_reset(&s->env, ep_seed);
    agent_reset(&s->agent, ep_seed);
    if (s->has_opponent_policy) agent_reset(&s->opponent, ep_seed ^ UINT64_C(0x9E3779B97F4A7C15));
    dashboard_init(&s->dashboard);
    s->current_reward = 0.0f;
    s->current_step = 0;
    s->episode_done = 0;
}

void viz_session_reset_all(VizSession* vs) {
    for (int i = 0; i < vs->session_count; i++) {
        AgentSession* s = &vs->sessions[i];
        s->epoch_count = 0;
        s->total_reward = 0.0f;
        s->total_wins = 0;
        s->total_deaths = 0;
        s->total_crates = 0;
        viz_session_reset_epoch(vs, i);
    }
}

void viz_session_switch_agent(VizSession* vs, int idx) {
    if (idx >= 0 && idx < vs->session_count) {
        vs->active_session = idx;
    }
}

void viz_session_switch_view(VizSession* vs, ViewMode mode) {
    vs->view_mode = mode;
}

AgentSession* viz_session_active(VizSession* vs) {
    if (vs->active_session < 0 || vs->active_session >= vs->session_count) return NULL;
    return &vs->sessions[vs->active_session];
}

static void record_epoch(VizSession* vs, int session_idx) {
    AgentSession* s = &vs->sessions[session_idx];
    if (s->epoch_count >= MAX_VIZ_EPOCHS) return;

    int idx = s->epoch_count;
    s->epoch_rewards[idx] = s->current_reward;
    s->epoch_lengths[idx] = s->current_step;

    int alive_enemies = 0;
    for (int a = 1; a < s->env.state.agent_count; a++) {
        if (s->env.state.agents[a].alive) alive_enemies++;
    }

    int won = (s->env.state.agents[0].alive && alive_enemies == 0 && s->env.state.agent_count > 1) ? 1 : 0;
    int died = !s->env.state.agents[0].alive;

    s->epoch_wins[idx] = won;
    s->epoch_deaths[idx] = died;
    s->epoch_crates[idx] = s->dashboard.action_counts[ACTION_PLACE_BOMB];

    /* Running average */
    s->total_reward += s->current_reward;
    s->total_wins += won;
    s->total_deaths += died;
    s->epoch_count++;
    s->epoch_avg_rewards[idx] = s->total_reward / (float)s->epoch_count;
}

void viz_session_step(VizSession* vs) {
    for (int i = 0; i < vs->session_count; i++) {
        AgentSession* s = &vs->sessions[i];
        if (s->episode_done) {
            if (vs->auto_advance_epoch && s->epoch_count < vs->max_epochs) {
                record_epoch(vs, i);
                if (s->epoch_count < vs->max_epochs) viz_session_reset_epoch(vs, i);
            }
            continue;
        }

        Observation obs;
        DebugSnapshot snap;

        for (int sp = 0; sp < vs->speed_mult; sp++) {
            if (s->episode_done) break;

            DebugSnapshot before;
            env_get_debug_snapshot(&s->env, &before);
            env_observe(&s->env, 0, &obs);
            env_get_debug_snapshot(&s->env, &snap);
            Action action = agent_act(&s->agent, &obs, &snap);
            StepResult result = env_step(&s->env, action);
            DebugSnapshot after;
            env_get_debug_snapshot(&s->env, &after);

            dashboard_add_action(&s->dashboard, action);
            dashboard_add_reward(&s->dashboard, result.reward);
            s->current_reward += result.reward;
            s->current_step++;

            char event[MAX_EVENT_LEN];
            const char* action_names[] = {"UP", "DOWN", "LEFT", "RIGHT", "BOMB", "WAIT"};
            if (before.state.agents[0].x != after.state.agents[0].x ||
                before.state.agents[0].y != after.state.agents[0].y) {
                snprintf(event, sizeof(event), "Step %d: Agent moved %s", after.state.step, action_names[action]);
                dashboard_add_event(&s->dashboard, event);
            } else if (action == ACTION_PLACE_BOMB) {
                snprintf(event, sizeof(event), "Step %d: Agent placed bomb", after.state.step);
                dashboard_add_event(&s->dashboard, event);
            }
            for (int a = 1; a < after.state.agent_count; a++) {
                if (before.state.agents[a].alive && !after.state.agents[a].alive) {
                    snprintf(event, sizeof(event), "Step %d: Enemy %d defeated", after.state.step, a);
                    dashboard_add_event(&s->dashboard, event);
                } else if (before.state.agents[a].x != after.state.agents[a].x ||
                           before.state.agents[a].y != after.state.agents[a].y) {
                    snprintf(event, sizeof(event), "Step %d: Enemy %d moved", after.state.step, a);
                    dashboard_add_event(&s->dashboard, event);
                }
            }
            int bombs_before = 0, bombs_after = 0;
            for (int b = 0; b < MAX_BOMBS; b++) {
                bombs_before += before.state.bombs[b].active;
                bombs_after += after.state.bombs[b].active;
            }
            if (bombs_after < bombs_before) {
                snprintf(event, sizeof(event), "Step %d: Bomb exploded", after.state.step);
                dashboard_add_event(&s->dashboard, event);
            }
            int crates_before = map_count_crates(&before.state);
            int crates_after = map_count_crates(&after.state);
            if (crates_after < crates_before) {
                snprintf(event, sizeof(event), "Step %d: Crate destroyed", after.state.step);
                dashboard_add_event(&s->dashboard, event);
            }
            int danger_before = before.danger.time_to_blast[before.state.agents[0].y][before.state.agents[0].x] >= 0;
            int danger_after = after.danger.time_to_blast[after.state.agents[0].y][after.state.agents[0].x] >= 0;
            if (danger_before != danger_after) {
                snprintf(event, sizeof(event), "Step %d: Agent %s danger", after.state.step,
                         danger_after ? "entered" : "escaped");
                dashboard_add_event(&s->dashboard, event);
            }
            if (result.done) {
                snprintf(event, sizeof(event), "Step %d: Episode ended (%d)", after.state.step,
                         (int)result.terminal_reason);
                dashboard_add_event(&s->dashboard, event);
            }

            if (result.done) {
                s->episode_done = 1;
                break;
            }
        }
    }
    int all_complete = vs->session_count > 0;
    for (int i = 0; i < vs->session_count; i++)
        if (!vs->sessions[i].episode_done || vs->sessions[i].epoch_count < vs->max_epochs) all_complete = 0;
    if (all_complete) vs->paused = 1;
}
