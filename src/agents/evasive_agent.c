#include "agents/evasive_agent.h"
#include "env/bomber_map.h"
#include <string.h>

typedef struct { RNG rng; } EvasiveImpl;

static int exits_at(const BomberState* state, int x, int y) {
    static const int dx[] = {0, 0, -1, 1}; static const int dy[] = {-1, 1, 0, 0};
    int exits = 0;
    for (int d = 0; d < 4; d++) if (map_is_walkable(state, x + dx[d], y + dy[d])) exits++;
    return exits;
}

static Action evasive_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug) {
    EvasiveImpl* impl = (EvasiveImpl*)agent->impl;
    static const int dx[] = {0, 0, -1, 1}; static const int dy[] = {-1, 1, 0, 0};
    Action best_actions[4]; int best_count = 0, best_score = -999999;
    for (int action = ACTION_UP; action <= ACTION_RIGHT; action++) {
        if (!obs->valid_actions[action] || !obs->safe_actions[action]) continue;
        int x = obs->agent_x + dx[action], y = obs->agent_y + dy[action];
        int timer = debug->danger.time_to_blast[y][x];
        int nearest_enemy = 999;
        for (int i = 0; i < obs->enemy_count; i++) if (obs->enemy_alive[i]) {
            int ex = obs->enemy_x[i] - x; if (ex < 0) ex = -ex;
            int ey = obs->enemy_y[i] - y; if (ey < 0) ey = -ey;
            if (ex + ey < nearest_enemy) nearest_enemy = ex + ey;
        }
        int score = exits_at(&debug->state, x, y) * 15 + nearest_enemy * 4;
        if (timer < 0) score += 1000; else score += timer * 100;
        if (score > best_score) { best_score = score; best_count = 0; }
        if (score == best_score) best_actions[best_count++] = (Action)action;
    }
    if (best_count) return best_actions[rng_range(&impl->rng, 0, best_count)];
    return ACTION_WAIT;
}

static void evasive_reset(Agent* agent, uint64_t seed) {
    EvasiveImpl* impl = (EvasiveImpl*)agent->impl;
    rng_init(&impl->rng, seed);
}

void evasive_agent_init(Agent* agent) {
    EvasiveImpl* impl = (EvasiveImpl*)agent_impl_storage(agent, sizeof(*impl));
    agent->impl = impl; agent->act = evasive_act; agent->reset = evasive_reset;
    strncpy(agent->name, "evasive", sizeof(agent->name) - 1);
    evasive_reset(agent, 1);
}
