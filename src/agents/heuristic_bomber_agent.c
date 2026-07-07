#include "agents/heuristic_bomber_agent.h"
#include "env/bomber_map.h"
#include "env/bomber_danger.h"
#include "core/math_util.h"
#include <string.h>
#include <stdio.h>

/* Heuristic safe bomber: uses danger map, places bomb only if escape exists,
   prioritizes survival > crates > powerups > enemy traps */

static int find_nearest_target(const Observation* obs, int target_tile, int* out_dx, int* out_dy) {
    int best_dist = 999999;
    int best_dx = 0, best_dy = 0;
    int found = 0;
    for (int y = 0; y < LOCAL_OBS_SIZE; y++) {
        for (int x = 0; x < LOCAL_OBS_SIZE; x++) {
            if (obs->local_tiles[y][x] == target_tile) {
                int dx = (x - LOCAL_OBS_HALF);
                int dy = (y - LOCAL_OBS_HALF);
                int dist = absi(dx) + absi(dy);
                if (dist < best_dist && dist > 0) {
                    best_dist = dist;
                    best_dx = dx;
                    best_dy = dy;
                    found = 1;
                }
            }
        }
    }
    *out_dx = best_dx;
    *out_dy = best_dy;
    return found;
}

static int has_adjacent_tile_type(const Observation* obs, TileType t) {
    int cx = LOCAL_OBS_HALF;
    int cy = LOCAL_OBS_HALF;
    if (obs->local_tiles[cy][cx+1] == (int)t) return 1;
    if (obs->local_tiles[cy][cx-1] == (int)t) return 1;
    if (obs->local_tiles[cy+1][cx] == (int)t) return 1;
    if (obs->local_tiles[cy-1][cx] == (int)t) return 1;
    return 0;
}

static int enemy_in_blast_range(const Observation* obs) {
    for (int i = 0; i < obs->enemy_count; i++) if (obs->enemy_alive[i]) {
        int dx = absi(obs->enemy_x[i] - obs->agent_x);
        int dy = absi(obs->enemy_y[i] - obs->agent_y);
        if ((dx == 0 || dy == 0) && dx + dy <= obs->agent_blast_range) return 1;
    }
    return 0;
}

static int nearest_enemy(const Observation* obs, int* out_dx, int* out_dy) {
    int best = 999999, found = 0;
    for (int i = 0; i < obs->enemy_count; i++) if (obs->enemy_alive[i]) {
        int dx = obs->enemy_x[i] - obs->agent_x;
        int dy = obs->enemy_y[i] - obs->agent_y;
        int distance = absi(dx) + absi(dy);
        if (distance < best) { best = distance; *out_dx = dx; *out_dy = dy; found = 1; }
    }
    return found;
}

static Action best_escape(const Observation* obs, const DebugSnapshot* debug) {
    static const int dx[] = {0, 0, -1, 1}; static const int dy[] = {-1, 1, 0, 0};
    Action best = ACTION_WAIT; int best_score = -999999;
    for (int a = ACTION_UP; a <= ACTION_RIGHT; a++) {
        if (!obs->valid_actions[a] || !obs->safe_actions[a]) continue;
        int x = obs->agent_x + dx[a], y = obs->agent_y + dy[a];
        int timer = debug->danger.time_to_blast[y][x];
        int exits = 0;
        for (int d = 0; d < 4; d++)
            if (map_is_walkable(&debug->state, x + dx[d], y + dy[d])) exits++;
        int score = (timer < 0 ? 1000 : timer * 20) + exits * 5;
        if (score > best_score) { best_score = score; best = (Action)a; }
    }
    return best;
}

static Action move_toward(int dx, int dy, const Observation* obs, HeuristicBomberAgent* ha) {
    /* Try to move in the direction of the target, preferring safe moves */
    if (absi(dx) >= absi(dy) && dx != 0) {
        Action a = (dx > 0) ? ACTION_RIGHT : ACTION_LEFT;
        if (obs->valid_actions[a] && obs->safe_actions[a]) return a;
    }
    if (dy != 0) {
        Action a = (dy > 0) ? ACTION_DOWN : ACTION_UP;
        if (obs->valid_actions[a] && obs->safe_actions[a]) return a;
    }
    if (dx != 0) {
        Action a = (dx > 0) ? ACTION_RIGHT : ACTION_LEFT;
        if (obs->valid_actions[a] && obs->safe_actions[a]) return a;
    }
    /* Try any safe move */
    for (int a = 0; a < 4; a++) {
        if (obs->valid_actions[a] && obs->safe_actions[a]) return (Action)a;
    }
    (void)ha;
    return ACTION_WAIT;
}

Action heuristic_agent_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug) {
    HeuristicBomberAgent* ha = (HeuristicBomberAgent*)agent->impl;

    /* Priority 1: Start following an escape route as soon as a blast is scheduled. */
    if (obs->in_danger || obs->danger_timer >= 0) {
        Action escape = best_escape(obs, debug);
        if (escape != ACTION_WAIT) {
            snprintf(ha->decision_text, sizeof(ha->decision_text),
                     "Move %s: escape route (blast in %d ticks)",
                     escape == ACTION_UP ? "up" : escape == ACTION_DOWN ? "down" :
                     escape == ACTION_LEFT ? "left" : "right", obs->danger_timer);
            return escape;
        }
        snprintf(ha->decision_text, sizeof(ha->decision_text),
                 "Wait: no safe escape from danger!");
        return ACTION_WAIT;
    }

    /* Priority 2: Bomb crates or an enemy in range only when an escape exists. */
    if ((has_adjacent_tile_type(obs, TILE_CRATE) || enemy_in_blast_range(obs)) &&
        obs->valid_actions[ACTION_PLACE_BOMB] &&
        obs->safe_actions[ACTION_PLACE_BOMB]) {
        snprintf(ha->decision_text, sizeof(ha->decision_text),
                 "Place bomb: useful target, escape path available");
        return ACTION_PLACE_BOMB;
    }

    /* Priority 3: Move toward powerups */
    int pdx, pdy;
    if (find_nearest_target(obs, TILE_POWERUP_BOMB, &pdx, &pdy) ||
        find_nearest_target(obs, TILE_POWERUP_RANGE, &pdx, &pdy) ||
        find_nearest_target(obs, TILE_POWERUP_SPEED, &pdx, &pdy)) {
        Action a = move_toward(pdx, pdy, obs, ha);
        if (a != ACTION_WAIT) {
            snprintf(ha->decision_text, sizeof(ha->decision_text),
                     "Move toward powerup at (%d, %d)", obs->agent_x + pdx, obs->agent_y + pdy);
            return a;
        }
    }

    /* Priority 4: Move toward nearest crate to bomb it */
    int cdx, cdy;
    if (find_nearest_target(obs, TILE_CRATE, &cdx, &cdy)) {
        Action a = move_toward(cdx, cdy, obs, ha);
        if (a != ACTION_WAIT) {
            snprintf(ha->decision_text, sizeof(ha->decision_text),
                     "Move toward crate at (%d, %d)", obs->agent_x + cdx, obs->agent_y + cdy);
            return a;
        }
    }

    /* Priority 5: Once local crates are gone, pressure the nearest enemy. */
    int edx, edy;
    if (nearest_enemy(obs, &edx, &edy)) {
        Action a = move_toward(edx, edy, obs, ha);
        if (a != ACTION_WAIT) {
            snprintf(ha->decision_text, sizeof(ha->decision_text), "Pressure nearest enemy");
            return a;
        }
    }

    /* Priority 6: Random safe walk */
    int safe_moves[4];
    int safe_count = 0;
    for (int a = 0; a < 4; a++) {
        if (obs->valid_actions[a] && obs->safe_actions[a]) {
            safe_moves[safe_count++] = a;
        }
    }
    if (safe_count > 0) {
        int pick = rng_range(&ha->rng, 0, safe_count);
        snprintf(ha->decision_text, sizeof(ha->decision_text),
                 "Random safe move");
        return (Action)safe_moves[pick];
    }

    snprintf(ha->decision_text, sizeof(ha->decision_text),
             "Wait: all moves unsafe");
    return ACTION_WAIT;
}

void heuristic_agent_reset(Agent* agent, uint64_t seed) {
    HeuristicBomberAgent* ha = (HeuristicBomberAgent*)agent->impl;
    rng_init(&ha->rng, seed);
    ha->decision_text[0] = '\0';
}

void heuristic_agent_init(Agent* agent) {
    HeuristicBomberAgent* impl = (HeuristicBomberAgent*)agent_impl_storage(agent, sizeof(HeuristicBomberAgent));
    if (!impl) return;
    memset(impl, 0, sizeof(*impl));
    rng_init(&impl->rng, 99);
    agent->type = AGENT_HEURISTIC;
    agent->act = heuristic_agent_act;
    agent->reset = heuristic_agent_reset;
    agent->impl = impl;
    strncpy(agent->name, "heuristic", sizeof(agent->name) - 1);
}
