#include "env/bomber_observation.h"
#include "env/bomber_map.h"
#include "core/math_util.h"
#include <string.h>
#include <assert.h>

void obs_compute(const BomberState* state, const DangerMap* dm, int agent_id,
                 int prev_action, Observation* obs) {
    memset(obs, 0, sizeof(Observation));

    if (agent_id < 0 || agent_id >= state->agent_count) return;

    const BomberAgentState* agent = &state->agents[agent_id];
    obs->agent_id = agent_id;
    obs->agent_x = agent->x;
    obs->agent_y = agent->y;
    obs->agent_alive = agent->alive;
    obs->agent_bomb_ammo = agent->bomb_ammo;
    obs->agent_blast_range = agent->blast_range;
    obs->agent_speed = agent->speed;
    obs->agent_score = agent->score;
    obs->prev_action = prev_action;

    /* Fill local grid centered on agent */
    for (int ly = 0; ly < LOCAL_OBS_SIZE; ly++) {
        for (int lx = 0; lx < LOCAL_OBS_SIZE; lx++) {
            int wx = agent->x + (lx - LOCAL_OBS_HALF);
            int wy = agent->y + (ly - LOCAL_OBS_HALF);
            if (!map_in_bounds(state, wx, wy)) {
                obs->local_tiles[ly][lx] = TILE_SOLID_WALL;
                obs->local_bombs[ly][lx] = 0;
                obs->local_bomb_timers[ly][lx] = -1;
                obs->local_danger[ly][lx] = -1;
            } else {
                obs->local_tiles[ly][lx] = state->tiles[wy][wx];
                obs->local_bombs[ly][lx] = map_has_bomb(state, wx, wy) ? 1 : 0;
                /* Find bomb timer */
                obs->local_bomb_timers[ly][lx] = -1;
                for (int b = 0; b < MAX_BOMBS; b++) {
                    if (state->bombs[b].active && state->bombs[b].x == wx && state->bombs[b].y == wy) {
                        obs->local_bomb_timers[ly][lx] = state->bombs[b].timer;
                        break;
                    }
                }
                obs->local_danger[ly][lx] = dm->time_to_blast[wy][wx];
            }
        }
    }

    /* Enemy info */
    obs->enemy_count = 0;
    for (int a = 0; a < state->agent_count && obs->enemy_count < MAX_AGENTS; a++) {
        if (a == agent_id) continue;
        obs->enemy_x[obs->enemy_count] = state->agents[a].x;
        obs->enemy_y[obs->enemy_count] = state->agents[a].y;
        obs->enemy_alive[obs->enemy_count] = state->agents[a].alive;
        obs->enemy_count++;
    }

    /* Powerup positions */
    obs->powerup_count = 0;
    for (int y = 0; y < state->height; y++) {
        for (int x = 0; x < state->width; x++) {
            TileType t = state->tiles[y][x];
            if (t == TILE_POWERUP_BOMB || t == TILE_POWERUP_RANGE || t == TILE_POWERUP_SPEED) {
                if (obs->powerup_count < MAX_AGENTS * 4) {
                    obs->powerup_x[obs->powerup_count] = x;
                    obs->powerup_y[obs->powerup_count] = y;
                    obs->powerup_count++;
                }
            }
        }
    }

    /* Valid actions */
    int dx[4] = {0, 0, -1, 1};
    int dy[4] = {-1, 1, 0, 0};
    for (int a = 0; a < 4; a++) {
        int nx = agent->x + dx[a];
        int ny = agent->y + dy[a];
        obs->valid_actions[a] = map_is_walkable(state, nx, ny) ? 1 : 0;
    }
    obs->valid_actions[ACTION_PLACE_BOMB] = (agent->bomb_ammo > 0 && !map_has_bomb(state, agent->x, agent->y)) ? 1 : 0;
    obs->valid_actions[ACTION_WAIT] = 1;

    /* Safe actions from danger map */
    for (int a = 0; a < ACTION_COUNT; a++) {
        obs->safe_actions[a] = dm->action_safe[a];
    }

    /* Danger info */
    obs->in_danger = dm->current_blast[agent->y][agent->x];
    obs->danger_timer = dm->time_to_blast[agent->y][agent->x];
    obs->imminent_danger = (obs->danger_timer >= 0 && obs->danger_timer <= 2) ? 1 : 0;
}

void obs_to_flat(const Observation* obs, float* out, int* out_size) {
    int idx = 0;

    /* Agent info (7 values) */
    out[idx++] = (float)obs->agent_x;
    out[idx++] = (float)obs->agent_y;
    out[idx++] = (float)obs->agent_alive;
    out[idx++] = (float)obs->agent_bomb_ammo;
    out[idx++] = (float)obs->agent_blast_range;
    out[idx++] = (float)obs->agent_speed;
    out[idx++] = (float)obs->agent_score;

    /* Local grids (4 channels * LOCAL_OBS_SIZE^2) */
    for (int y = 0; y < LOCAL_OBS_SIZE; y++) {
        for (int x = 0; x < LOCAL_OBS_SIZE; x++) {
            out[idx++] = (float)obs->local_tiles[y][x];
        }
    }
    for (int y = 0; y < LOCAL_OBS_SIZE; y++) {
        for (int x = 0; x < LOCAL_OBS_SIZE; x++) {
            out[idx++] = (float)obs->local_bombs[y][x];
        }
    }
    for (int y = 0; y < LOCAL_OBS_SIZE; y++) {
        for (int x = 0; x < LOCAL_OBS_SIZE; x++) {
            out[idx++] = (float)obs->local_bomb_timers[y][x];
        }
    }
    for (int y = 0; y < LOCAL_OBS_SIZE; y++) {
        for (int x = 0; x < LOCAL_OBS_SIZE; x++) {
            out[idx++] = (float)obs->local_danger[y][x];
        }
    }

    /* Valid actions (6) */
    for (int a = 0; a < ACTION_COUNT; a++) out[idx++] = (float)obs->valid_actions[a];

    /* Safe actions (6) */
    for (int a = 0; a < ACTION_COUNT; a++) out[idx++] = (float)obs->safe_actions[a];

    /* Danger info (3) */
    out[idx++] = (float)obs->in_danger;
    out[idx++] = (float)obs->danger_timer;
    out[idx++] = (float)obs->imminent_danger;

    /* Previous action (1) */
    out[idx++] = (float)obs->prev_action;

    *out_size = idx;
    assert(idx <= OBS_FLAT_SIZE);
}
