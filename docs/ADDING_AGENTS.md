# Adding Agents

## Agent interface

All agents implement the same interface:

```c
typedef struct Agent Agent;

typedef Action (*AgentActFn)(Agent* agent, const Observation* obs, const DebugSnapshot* debug);
typedef void (*AgentResetFn)(Agent* agent, uint64_t seed);

struct Agent {
    AgentType type;
    AgentActFn act;
    AgentResetFn reset;
    void* impl;     // Pointer to agent-specific state
    char name[32];
};
```

## Adding a new agent

### 1. Create header and source files

`src/agents/my_agent.h`:
```c
#ifndef BOMBER_MY_AGENT_H
#define BOMBER_MY_AGENT_H

#include "agents/agent.h"

typedef struct {
    RNG rng;
    // Your agent's internal state
} MyAgent;

void my_agent_init(Agent* agent);
Action my_agent_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug);
void my_agent_reset(Agent* agent, uint64_t seed);

#endif
```

`src/agents/my_agent.c`:
```c
#include "agents/my_agent.h"
#include <string.h>

Action my_agent_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug) {
    MyAgent* ma = (MyAgent*)agent->impl;
    // Your decision logic here
    return ACTION_WAIT;
}

void my_agent_reset(Agent* agent, uint64_t seed) {
    MyAgent* ma = (MyAgent*)agent->impl;
    rng_init(&ma->rng, seed);
}

void my_agent_init(Agent* agent) {
    static MyAgent impl;
    memset(&impl, 0, sizeof(impl));
    rng_init(&impl.rng, 42);
    agent->type = AGENT_EXTERNAL; // or add a new type
    agent->act = my_agent_act;
    agent->reset = my_agent_reset;
    agent->impl = &impl;
    strncpy(agent->name, "my_agent", sizeof(agent->name) - 1);
}
```

### 2. Add to build system

Add your source file to `src/CMakeLists.txt`:
```cmake
agents/my_agent.c
```

### 3. Register in agent factory

Add to `agent_init()` in `src/agents/agent.c`:
```c
case AGENT_MY_TYPE: my_agent_init(agent); break;
```

Add to `agent_parse_type()`:
```c
if (strcmp(name, "my_agent") == 0) return AGENT_MY_TYPE;
```

### 4. Add agent type enum

Add to `AgentType` in `src/agents/agent.h`:
```c
AGENT_MY_TYPE
```

## Using the observation

The observation provides everything needed for decision-making:

```c
Action my_agent_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug) {
    // Check danger
    if (obs->in_danger) {
        for (int a = 0; a < 4; a++) {
            if (obs->safe_actions[a] && obs->valid_actions[a])
                return (Action)a;
        }
    }

    // Check local grid for targets
    for (int y = 0; y < LOCAL_OBS_SIZE; y++) {
        for (int x = 0; x < LOCAL_OBS_SIZE; x++) {
            if (obs->local_tiles[y][x] == TILE_CRATE) {
                // Move toward crate
            }
        }
    }

    // Use flat observation for neural networks
    float flat[OBS_FLAT_SIZE];
    int size;
    obs_to_flat(obs, flat, &size);

    return ACTION_WAIT;
}
```
