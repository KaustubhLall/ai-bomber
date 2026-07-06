#ifndef BOMBER_HEURISTIC_AGENT_H
#define BOMBER_HEURISTIC_AGENT_H

#include "agents/agent.h"

typedef struct {
    RNG rng;
    char decision_text[256];
} HeuristicBomberAgent;

void heuristic_agent_init(Agent* agent);
Action heuristic_agent_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug);
void heuristic_agent_reset(Agent* agent, uint64_t seed);

#endif /* BOMBER_HEURISTIC_AGENT_H */
