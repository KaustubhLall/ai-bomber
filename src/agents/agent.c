#include "agents/agent.h"
#include "agents/random_agent.h"
#include "agents/scripted_agent.h"
#include "agents/heuristic_bomber_agent.h"
#include "agents/greedy_crate_agent.h"
#include <string.h>

void* agent_impl_storage(Agent* agent, size_t required_size) {
    if (!agent || required_size > sizeof(agent->storage.bytes)) {
        return NULL;
    }
    return (void*)agent->storage.bytes;
}

void agent_init(Agent* agent, AgentType type) {
    memset(agent, 0, sizeof(Agent));
    agent->type = type;
    switch (type) {
        case AGENT_RANDOM:       random_agent_init(agent); break;
        case AGENT_SCRIPTED:     scripted_agent_init(agent); break;
        case AGENT_HEURISTIC:    heuristic_agent_init(agent); break;
        case AGENT_GREEDY_CRATE: greedy_crate_agent_init(agent); break;
        case AGENT_ENEMY_BOT:    scripted_agent_init(agent); break; /* reuse scripted baseline */
        case AGENT_EXTERNAL:     random_agent_init(agent); break;  /* placeholder */
    }
}

Action agent_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug) {
    if (!agent || !agent->act) return ACTION_WAIT;
    return agent->act(agent, obs, debug);
}

void agent_reset(Agent* agent, uint64_t seed) {
    if (agent && agent->reset) agent->reset(agent, seed);
}

AgentType agent_parse_type(const char* name) {
    if (!name) return AGENT_RANDOM;
    if (strcmp(name, "random") == 0) return AGENT_RANDOM;
    if (strcmp(name, "scripted") == 0) return AGENT_SCRIPTED;
    if (strcmp(name, "heuristic") == 0) return AGENT_HEURISTIC;
    if (strcmp(name, "greedy") == 0 || strcmp(name, "greedy_crate") == 0) return AGENT_GREEDY_CRATE;
    if (strcmp(name, "enemy") == 0 || strcmp(name, "enemy_bot") == 0) return AGENT_ENEMY_BOT;
    if (strcmp(name, "external") == 0) return AGENT_EXTERNAL;
    return AGENT_RANDOM;
}
