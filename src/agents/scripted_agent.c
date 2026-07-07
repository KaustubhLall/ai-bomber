#include "agents/scripted_agent.h"
#include "env/bomber_map.h"
#include "core/math_util.h"
#include <string.h>

static int find_nearest_powerup(const Observation* obs, int* out_dx, int* out_dy) {
    if (obs->powerup_count == 0) return 0;
    int best_dist = 999999;
    int best_dx = 0, best_dy = 0;
    for (int i = 0; i < obs->powerup_count; i++) {
        int dx = obs->powerup_x[i] - obs->agent_x;
        int dy = obs->powerup_y[i] - obs->agent_y;
        int dist = absi(dx) + absi(dy);
        if (dist < best_dist) {
            best_dist = dist;
            best_dx = dx;
            best_dy = dy;
        }
    }
    *out_dx = best_dx;
    *out_dy = best_dy;
    return 1;
}

static int has_adjacent_crate(const Observation* obs) {
    int cx = LOCAL_OBS_HALF;
    int cy = LOCAL_OBS_HALF;
    if (obs->local_tiles[cy][cx+1] == TILE_CRATE) return 1;
    if (obs->local_tiles[cy][cx-1] == TILE_CRATE) return 1;
    if (obs->local_tiles[cy+1][cx] == TILE_CRATE) return 1;
    if (obs->local_tiles[cy-1][cx] == TILE_CRATE) return 1;
    return 0;
}

Action scripted_agent_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug) {
    (void)debug;
    ScriptedAgent* sa = (ScriptedAgent*)agent->impl;

    if (obs->in_danger || obs->imminent_danger) {
        for (int a = 0; a < 4; a++) {
            if (obs->safe_actions[a] && obs->valid_actions[a]) {
                sa->last_action = a;
                return (Action)a;
            }
        }
        for (int a = 0; a < 4; a++) {
            if (obs->valid_actions[a]) {
                sa->last_action = a;
                return (Action)a;
            }
        }
    }

    if (has_adjacent_crate(obs) && obs->valid_actions[ACTION_PLACE_BOMB] &&
        obs->safe_actions[ACTION_PLACE_BOMB]) {
        sa->last_action = ACTION_PLACE_BOMB;
        return ACTION_PLACE_BOMB;
    }

    int pdx, pdy;
    if (find_nearest_powerup(obs, &pdx, &pdy)) {
        if (absi(pdx) > absi(pdy)) {
            Action a = (pdx > 0) ? ACTION_RIGHT : ACTION_LEFT;
            if (obs->valid_actions[a] && obs->safe_actions[a]) {
                sa->last_action = a;
                return a;
            }
        }
        if (pdy != 0) {
            Action a = (pdy > 0) ? ACTION_DOWN : ACTION_UP;
            if (obs->valid_actions[a] && obs->safe_actions[a]) {
                sa->last_action = a;
                return a;
            }
        }
        if (pdx != 0) {
            Action a = (pdx > 0) ? ACTION_RIGHT : ACTION_LEFT;
            if (obs->valid_actions[a] && obs->safe_actions[a]) {
                sa->last_action = a;
                return a;
            }
        }
    }

    int safe_moves[4];
    int safe_count = 0;
    for (int a = 0; a < 4; a++) {
        if (obs->valid_actions[a] && obs->safe_actions[a]) {
            safe_moves[safe_count++] = a;
        }
    }
    if (safe_count > 0) {
        int pick = rng_range(&sa->rng, 0, safe_count);
        sa->last_action = safe_moves[pick];
        return (Action)safe_moves[pick];
    }

    sa->last_action = ACTION_WAIT;
    return ACTION_WAIT;
}

void scripted_agent_reset(Agent* agent, uint64_t seed) {
    ScriptedAgent* sa = (ScriptedAgent*)agent->impl;
    rng_init(&sa->rng, seed);
    sa->last_action = ACTION_WAIT;
}

void scripted_agent_init(Agent* agent) {
    ScriptedAgent* impl = (ScriptedAgent*)agent_impl_storage(agent, sizeof(ScriptedAgent));
    if (!impl) return;
    memset(impl, 0, sizeof(*impl));
    rng_init(&impl->rng, 42);
    agent->type = AGENT_SCRIPTED;
    agent->act = scripted_agent_act;
    agent->reset = scripted_agent_reset;
    agent->impl = impl;
    strncpy(agent->name, "scripted", sizeof(agent->name) - 1);
}
