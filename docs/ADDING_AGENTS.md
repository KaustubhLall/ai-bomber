# Adding Agents

## Agent interface

All agents implement the same small interface:

```c
typedef struct Agent Agent;

typedef Action (*AgentActFn)(Agent* agent, const Observation* obs, const DebugSnapshot* debug);
typedef void (*AgentResetFn)(Agent* agent, uint64_t seed);

struct Agent {
    AgentType type;
    AgentActFn act;
    AgentResetFn reset;
    void* impl;
    AgentImplStorage storage;
    char name[32];
};
```

The built-in agents store their implementation state inside `Agent.storage` through `agent_impl_storage()`. Avoid file-static implementation structs for agent state; they make two instances of the same agent type share RNG state, which breaks side-by-side visualizer sessions and multi-agent evaluation.

## Adding a new agent

### 1. Create header and source files

`src/agents/my_agent.h`:

```c
#ifndef BOMBER_MY_AGENT_H
#define BOMBER_MY_AGENT_H

#include "agents/agent.h"

typedef struct {
    RNG rng;
    int last_action;
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
    (void)debug;
    MyAgent* ma = (MyAgent*)agent->impl;

    if (obs->in_danger) {
        for (int a = 0; a < 4; a++) {
            if (obs->safe_actions[a] && obs->valid_actions[a]) {
                ma->last_action = a;
                return (Action)a;
            }
        }
    }

    ma->last_action = ACTION_WAIT;
    return ACTION_WAIT;
}

void my_agent_reset(Agent* agent, uint64_t seed) {
    MyAgent* ma = (MyAgent*)agent->impl;
    rng_init(&ma->rng, seed);
    ma->last_action = ACTION_WAIT;
}

void my_agent_init(Agent* agent) {
    MyAgent* impl = (MyAgent*)agent_impl_storage(agent, sizeof(MyAgent));
    if (!impl) return;

    memset(impl, 0, sizeof(*impl));
    rng_init(&impl->rng, 42);
    agent->type = AGENT_EXTERNAL; /* or add a new enum value */
    agent->act = my_agent_act;
    agent->reset = my_agent_reset;
    agent->impl = impl;
    strncpy(agent->name, "my_agent", sizeof(agent->name) - 1);
}
```

### 2. Add to build system

Add the source file to `src/CMakeLists.txt`:

```cmake
agents/my_agent.c
```

### 3. Register in the agent factory

Add a type to `AgentType` in `src/agents/agent.h`:

```c
AGENT_MY_TYPE
```

Add it to `agent_init()` in `src/agents/agent.c`:

```c
case AGENT_MY_TYPE: my_agent_init(agent); break;
```

Add it to `agent_parse_type()`:

```c
if (strcmp(name, "my_agent") == 0) return AGENT_MY_TYPE;
```

## Using the observation

The observation provides local tile channels, valid actions, safety hints, previous action, and compact agent state:

```c
Action my_agent_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug) {
    (void)agent;
    (void)debug;

    if (obs->in_danger) {
        for (int a = 0; a < 4; a++) {
            if (obs->safe_actions[a] && obs->valid_actions[a]) {
                return (Action)a;
            }
        }
    }

    float flat[OBS_FLAT_SIZE];
    int size = 0;
    obs_to_flat(obs, flat, &size);

    return ACTION_WAIT;
}
```

## Implementation checklist

- Use `agent_impl_storage()` for per-instance state.
- Reset RNG and transient state in your reset function.
- Prefer `obs->valid_actions` before choosing a move.
- Treat `obs->safe_actions` as a danger-map hint, not as a full proof of long-term safety.
- Add a smoke test that creates two instances of the same agent type and verifies their `impl` pointers do not alias.
