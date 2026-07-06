#ifndef BOMBER_AGENT_H
#define BOMBER_AGENT_H

#include "env/env.h"
#include "env/bomber_observation.h"

typedef struct Agent Agent;

typedef Action (*AgentActFn)(Agent* agent, const Observation* obs, const DebugSnapshot* debug);
typedef void (*AgentResetFn)(Agent* agent, uint64_t seed);

typedef enum {
    AGENT_RANDOM = 0,
    AGENT_SCRIPTED,
    AGENT_HEURISTIC,
    AGENT_GREEDY_CRATE,
    AGENT_ENEMY_BOT,
    AGENT_EXTERNAL
} AgentType;

struct Agent {
    AgentType type;
    AgentActFn act;
    AgentResetFn reset;
    void* impl; /* Pointer to specific agent state */
    char name[32];
};

void agent_init(Agent* agent, AgentType type);
Action agent_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug);
void agent_reset(Agent* agent, uint64_t seed);
AgentType agent_parse_type(const char* name);

#endif /* BOMBER_AGENT_H */
