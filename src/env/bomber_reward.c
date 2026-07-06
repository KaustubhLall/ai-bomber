#include "env/bomber_reward.h"
#include "env/env.h"
#include "env/bomber_map.h"
#include "env/bomber_blast.h"
#include "core/config.h"

float reward_compute(RewardBreakdown* rb, BomberEnv* env, Action action,
                     int agent_id, int prev_crates, int prev_powerups,
                     int prev_enemies_alive, int was_in_danger) {
    memset(rb, 0, sizeof(RewardBreakdown));
    const BomberConfig* cfg = &env->config;
    BomberState* state = &env->state;
    BomberAgentState* agent = &state->agents[agent_id];

    /* Survival reward (per step alive) */
    if (agent->alive) {
        rb->survival = cfg->survival_reward;
    }

    /* Crate destruction reward */
    int cur_crates = map_count_crates(state);
    int crates_destroyed = prev_crates - cur_crates;
    if (crates_destroyed > 0) {
        rb->crate_destroyed = cfg->crate_destroy_reward * (float)crates_destroyed;
    }

    /* Powerup pickup: check if agent moved onto a powerup tile */
    TileType tile = state->tiles[agent->y][agent->x];
    if (tile == TILE_POWERUP_BOMB || tile == TILE_POWERUP_RANGE || tile == TILE_POWERUP_SPEED) {
        rb->powerup = cfg->powerup_reward;
    }

    /* Enemy damage/elimination */
    int cur_enemies_alive = 0;
    for (int a = 0; a < state->agent_count; a++) {
        if (a != agent_id && state->agents[a].alive) cur_enemies_alive++;
    }
    int enemies_killed = prev_enemies_alive - cur_enemies_alive;
    if (enemies_killed > 0) {
        rb->enemy_elimination = cfg->enemy_elimination_reward * (float)enemies_killed;
    }

    /* Death penalty */
    if (!agent->alive) {
        rb->death_penalty = cfg->death_penalty;
    }

    /* Escape danger reward */
    int in_danger_now = (env->danger.time_to_blast[agent->y][agent->x] >= 0) ? 1 : 0;
    if (was_in_danger && !in_danger_now && agent->alive) {
        rb->escape_danger = cfg->escape_danger_reward;
    }

    /* Invalid action penalty (handled in env_step, but we can check here) */
    /* Stall penalty: if agent hasn't moved or made progress */
    if (agent->alive && action == ACTION_WAIT && env->steps_since_progress > 10) {
        rb->stall_penalty = cfg->stall_penalty;
    }

    /* Timeout penalty */
    if (state->step >= cfg->max_steps) {
        rb->timeout_penalty = cfg->timeout_penalty;
    }

    /* Win/loss */
    if (agent->alive && cur_enemies_alive == 0 && state->agent_count > 1) {
        rb->win = cfg->win_reward;
    }

    /* Suicidal bomb penalty: if agent placed a bomb and is now in its blast with no escape */
    if (action == ACTION_PLACE_BOMB && env->danger.action_safe[ACTION_PLACE_BOMB] == 0) {
        rb->suicidal_bomb_penalty = cfg->suicidal_bomb_penalty;
    }

    /* Total */
    rb->total = rb->survival + rb->crate_destroyed + rb->powerup +
                rb->enemy_damage + rb->enemy_elimination + rb->win +
                rb->escape_danger + rb->trap_opportunity +
                rb->invalid_action_penalty + rb->suicidal_bomb_penalty +
                rb->stall_penalty + rb->death_penalty + rb->timeout_penalty;

    return rb->total;
}
