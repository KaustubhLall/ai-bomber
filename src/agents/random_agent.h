#ifndef BOMBER_RANDOM_AGENT_H
#define BOMBER_RANDOM_AGENT_H

#include "agents/agent.h"

typedef struct {
    RNG rng;
} RandomAgent;

void random_agent_init(Agent* agent);
Action random_agent_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug);
void random_agent_reset(Agent* agent, uint64_t seed);

#endif /* BOMBER_RANDOM_AGENT_H */
