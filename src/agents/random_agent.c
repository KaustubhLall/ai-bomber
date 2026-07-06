#include "agents/random_agent.h"
#include <string.h>

Action random_agent_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug) {
    (void)obs; (void)debug;
    RandomAgent* ra = (RandomAgent*)agent->impl;
    return (Action)rng_range(&ra->rng, 0, ACTION_COUNT);
}

void random_agent_reset(Agent* agent, uint64_t seed) {
    RandomAgent* ra = (RandomAgent*)agent->impl;
    rng_init(&ra->rng, seed);
}

void random_agent_init(Agent* agent) {
    static RandomAgent impl;
    memset(&impl, 0, sizeof(impl));
    rng_init(&impl.rng, 12345);
    agent->type = AGENT_RANDOM;
    agent->act = random_agent_act;
    agent->reset = random_agent_reset;
    agent->impl = &impl;
    strncpy(agent->name, "random", sizeof(agent->name) - 1);
}
