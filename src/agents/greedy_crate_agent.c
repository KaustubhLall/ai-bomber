#include "agents/greedy_crate_agent.h"
#include "env/bomber_map.h"
#include "core/math_util.h"
#include <string.h>

/* Greedy crate destroyer: focuses on crate destruction while trying not to die */

static int find_nearest_crate(const Observation* obs, int* out_dx, int* out_dy) {
    int best_dist = 999999;
    int best_dx = 0, best_dy = 0;
    int found = 0;
    for (int y = 0; y < LOCAL_OBS_SIZE; y++) {
        for (int x = 0; x < LOCAL_OBS_SIZE; x++) {
            if (obs->local_tiles[y][x] == TILE_CRATE) {
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

static int has_adjacent_crate(const Observation* obs) {
    int cx = LOCAL_OBS_HALF;
    int cy = LOCAL_OBS_HALF;
    return (obs->local_tiles[cy][cx+1] == TILE_CRATE ||
            obs->local_tiles[cy][cx-1] == TILE_CRATE ||
            obs->local_tiles[cy+1][cx] == TILE_CRATE ||
            obs->local_tiles[cy-1][cx] == TILE_CRATE);
}

Action greedy_crate_agent_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug) {
    (void)debug;
    GreedyCrateAgent* ga = (GreedyCrateAgent*)agent->impl;

    /* Always escape danger first */
    if (obs->in_danger || obs->imminent_danger) {
        for (int a = 0; a < 4; a++) {
            if (obs->safe_actions[a] && obs->valid_actions[a]) return (Action)a;
        }
        return ACTION_WAIT;
    }

    /* If adjacent to crate, bomb it (if safe) */
    if (has_adjacent_crate(obs) && obs->valid_actions[ACTION_PLACE_BOMB] &&
        obs->safe_actions[ACTION_PLACE_BOMB]) {
        return ACTION_PLACE_BOMB;
    }

    /* Move toward nearest crate */
    int cdx, cdy;
    if (find_nearest_crate(obs, &cdx, &cdy)) {
        if (absi(cdx) >= absi(cdy) && cdx != 0) {
            Action a = (cdx > 0) ? ACTION_RIGHT : ACTION_LEFT;
            if (obs->valid_actions[a] && obs->safe_actions[a]) return a;
        }
        if (cdy != 0) {
            Action a = (cdy > 0) ? ACTION_DOWN : ACTION_UP;
            if (obs->valid_actions[a] && obs->safe_actions[a]) return a;
        }
        if (cdx != 0) {
            Action a = (cdx > 0) ? ACTION_RIGHT : ACTION_LEFT;
            if (obs->valid_actions[a] && obs->safe_actions[a]) return a;
        }
    }

    /* Random safe walk */
    int safe_moves[4];
    int safe_count = 0;
    for (int a = 0; a < 4; a++) {
        if (obs->valid_actions[a] && obs->safe_actions[a]) {
            safe_moves[safe_count++] = a;
        }
    }
    if (safe_count > 0) {
        return (Action)safe_moves[rng_range(&ga->rng, 0, safe_count)];
    }

    return ACTION_WAIT;
}

void greedy_crate_agent_reset(Agent* agent, uint64_t seed) {
    GreedyCrateAgent* ga = (GreedyCrateAgent*)agent->impl;
    rng_init(&ga->rng, seed);
}

void greedy_crate_agent_init(Agent* agent) {
    GreedyCrateAgent* impl = (GreedyCrateAgent*)agent_impl_storage(agent, sizeof(GreedyCrateAgent));
    if (!impl) return;
    memset(impl, 0, sizeof(*impl));
    rng_init(&impl->rng, 77);
    agent->type = AGENT_GREEDY_CRATE;
    agent->act = greedy_crate_agent_act;
    agent->reset = greedy_crate_agent_reset;
    agent->impl = impl;
    strncpy(agent->name, "greedy_crate", sizeof(agent->name) - 1);
}
