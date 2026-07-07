#include "env/bomber_reward.h"
#include "env/env.h"
#include "env/bomber_map.h"
#include "env/bomber_blast.h"
#include "core/config.h"
#include <string.h>

float reward_compute(RewardBreakdown* rb, BomberEnv* env, Action action,
                     int agent_id, int prev_crates, int powerups_collected,
                     int prev_enemies_alive, int was_in_danger) {
    memset(rb, 0, sizeof(RewardBreakdown));
    const BomberConfig* cfg = &env->config;
    BomberState* state = &env->state;
    BomberAgentState* agent = &state->agents[agent_id];

    if (agent->alive) {
        rb->survival = cfg->survival_reward;
    }

    int cur_crates = map_count_crates(state);
    int crates_destroyed = prev_crates - cur_crates;
    if (crates_destroyed > 0) {
        rb->crate_destroyed = cfg->crate_destroy_reward * (float)crates_destroyed;
    }

    if (powerups_collected > 0) {
        rb->powerup = cfg->powerup_reward * (float)powerups_collected;
    }

    int cur_enemies_alive = 0;
    for (int a = 0; a < state->agent_count; a++) {
        if (a != agent_id && state->agents[a].alive) cur_enemies_alive++;
    }
    int enemies_killed = prev_enemies_alive - cur_enemies_alive;
    if (enemies_killed > 0) {
        rb->enemy_elimination = cfg->enemy_elimination_reward * (float)enemies_killed;
    }

    if (!agent->alive) {
        rb->death_penalty = cfg->death_penalty;
    }

    int in_danger_now = (env->danger.current_blast[agent->y][agent->x] ||
                         (env->danger.time_to_blast[agent->y][agent->x] >= 0 &&
                          env->danger.time_to_blast[agent->y][agent->x] <= 2)) ? 1 : 0;
    if (was_in_danger && !in_danger_now && agent->alive) {
        rb->escape_danger = cfg->escape_danger_reward;
    }

    if (agent->alive && action == ACTION_WAIT && env->steps_since_progress > 10) {
        rb->stall_penalty = cfg->stall_penalty;
    }

    if (state->step >= cfg->max_steps) {
        rb->timeout_penalty = cfg->timeout_penalty;
    }

    if (agent->alive && cur_enemies_alive == 0 && state->agent_count > 1) {
        rb->win = cfg->win_reward;
    }

    if (action == ACTION_PLACE_BOMB && env->danger.action_safe[ACTION_PLACE_BOMB] == 0) {
        rb->suicidal_bomb_penalty = cfg->suicidal_bomb_penalty;
    }

    rb->total = rb->survival + rb->crate_destroyed + rb->powerup +
                rb->enemy_damage + rb->enemy_elimination + rb->win +
                rb->escape_danger + rb->trap_opportunity +
                rb->invalid_action_penalty + rb->suicidal_bomb_penalty +
                rb->stall_penalty + rb->death_penalty + rb->timeout_penalty;

    return rb->total;
}
