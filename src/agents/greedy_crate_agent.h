#ifndef BOMBER_GREEDY_CRATE_AGENT_H
#define BOMBER_GREEDY_CRATE_AGENT_H

#include "agents/agent.h"

typedef struct {
    RNG rng;
} GreedyCrateAgent;

void greedy_crate_agent_init(Agent* agent);
Action greedy_crate_agent_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug);
void greedy_crate_agent_reset(Agent* agent, uint64_t seed);

#endif /* BOMBER_GREEDY_CRATE_AGENT_H */
