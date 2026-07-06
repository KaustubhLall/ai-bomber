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
    if (obs->local_tiles[cy][cx+1] == t) return 1;
    if (obs->local_tiles[cy][cx-1] == t) return 1;
    if (obs->local_tiles[cy+1][cx] == t) return 1;
    if (obs->local_tiles[cy-1][cx] == t) return 1;
    return 0;
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
    (void)debug;
    HeuristicBomberAgent* ha = (HeuristicBomberAgent*)agent->impl;

    /* Priority 1: If in danger, escape immediately */
    if (obs->in_danger) {
        for (int a = 0; a < 4; a++) {
            if (obs->safe_actions[a] && obs->valid_actions[a]) {
                snprintf(ha->decision_text, sizeof(ha->decision_text),
                         "Move %s: escaping danger (blast in %d ticks)",
                         a == ACTION_UP ? "up" : a == ACTION_DOWN ? "down" :
                         a == ACTION_LEFT ? "left" : "right", obs->danger_timer);
                return (Action)a;
            }
        }
        snprintf(ha->decision_text, sizeof(ha->decision_text),
                 "Wait: no safe escape from danger!");
        return ACTION_WAIT;
    }

    /* Priority 2: If adjacent to crate and can bomb safely, place bomb */
    if (has_adjacent_tile_type(obs, TILE_CRATE) &&
        obs->valid_actions[ACTION_PLACE_BOMB] &&
        obs->safe_actions[ACTION_PLACE_BOMB]) {
        snprintf(ha->decision_text, sizeof(ha->decision_text),
                 "Place bomb: crate adjacent, escape path available");
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

    /* Priority 5: Random safe walk */
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
    static HeuristicBomberAgent impl;
    memset(&impl, 0, sizeof(impl));
    rng_init(&impl.rng, 99);
    agent->type = AGENT_HEURISTIC;
    agent->act = heuristic_agent_act;
    agent->reset = heuristic_agent_reset;
    agent->impl = &impl;
    strncpy(agent->name, "heuristic", sizeof(agent->name) - 1);
}
