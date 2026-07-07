#ifndef BOMBER_AGENT_H
#define BOMBER_AGENT_H

#include "env/env.h"
#include "env/bomber_observation.h"
#include <stddef.h>

#define AGENT_IMPL_CAPACITY 512

typedef struct Agent Agent;

typedef Action (*AgentActFn)(Agent* agent, const Observation* obs, const DebugSnapshot* debug);
typedef void (*AgentResetFn)(Agent* agent, uint64_t seed);

typedef enum {
    AGENT_RANDOM = 0,
    AGENT_SCRIPTED,
    AGENT_HEURISTIC,
    AGENT_GREEDY_CRATE,
    AGENT_ENEMY_BOT,
    AGENT_EXTERNAL,
    AGENT_ALPHABETA,
    AGENT_MCTS,
    AGENT_EVASIVE
} AgentType;

typedef struct {
    int depth, nodes, simulations, prunes;
    Action selected_action;
    float value;
    int visits[ACTION_COUNT];
    float action_values[ACTION_COUNT];
} SearchDiagnostics;

typedef union {
    unsigned char bytes[AGENT_IMPL_CAPACITY];
    uint64_t align_u64;
    double align_double;
    void* align_ptr;
} AgentImplStorage;

struct Agent {
    AgentType type;
    AgentActFn act;
    AgentResetFn reset;
    void* impl; /* Pointer into storage or an external policy implementation. */
    AgentImplStorage storage;
    char name[32];
    SearchDiagnostics diagnostics;
};

void agent_init(Agent* agent, AgentType type);
Action agent_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug);
void agent_reset(Agent* agent, uint64_t seed);
AgentType agent_parse_type(const char* name);
const char* agent_type_name(AgentType type);
void* agent_impl_storage(Agent* agent, size_t required_size);

#endif /* BOMBER_AGENT_H */
