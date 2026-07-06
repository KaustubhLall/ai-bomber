#ifndef BOMBER_SCRIPTED_AGENT_H
#define BOMBER_SCRIPTED_AGENT_H

#include "agents/agent.h"

typedef struct {
    RNG rng;
    int last_action;
} ScriptedAgent;

void scripted_agent_init(Agent* agent);
Action scripted_agent_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug);
void scripted_agent_reset(Agent* agent, uint64_t seed);

#endif /* BOMBER_SCRIPTED_AGENT_H */
