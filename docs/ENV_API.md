# Environment API

## Actions

```c
typedef enum {
    ACTION_UP = 0,
    ACTION_DOWN,
    ACTION_LEFT,
    ACTION_RIGHT,
    ACTION_PLACE_BOMB,
    ACTION_WAIT,
    ACTION_COUNT
} Action;
```

## Terminal reasons

```c
typedef enum {
    TERMINAL_NONE = 0,
    TERMINAL_AGENT_DEAD,
    TERMINAL_ENEMY_DEAD,
    TERMINAL_WIN,
    TERMINAL_LOSS,
    TERMINAL_DRAW,
    TERMINAL_TIMEOUT
} TerminalReason;
```

## Step result

```c
typedef struct {
    float reward;
    int done;
    TerminalReason terminal_reason;
} StepResult;
```

## API functions

### `env_init`
```c
void env_init(BomberEnv* env, const BomberConfig* config);
```
Initializes the environment with the given configuration. Calls `env_reset` internally.

### `env_reset`
```c
void env_reset(BomberEnv* env, uint64_t seed);
```
Resets the environment with a deterministic seed. Regenerates the map, resets agents, clears bombs.

### `env_step`
```c
StepResult env_step(BomberEnv* env, Action action);
```
Executes one step for agent 0 with the given action. Internally:
1. Executes the agent's action (move, bomb, wait).
2. Runs enemy AI for other agents.
3. Picks up powerups.
4. Ticks all bombs (decrement timers, explode at zero).
5. Recomputes danger map.
6. Computes reward.
7. Checks terminal conditions.

Returns the step reward, done flag, and terminal reason.

### `env_observe`
```c
void env_observe(const BomberEnv* env, int agent_id, Observation* obs);
```
Computes the observation for the given agent. See [OBSERVATION.md](OBSERVATION.md).

### `env_get_debug_snapshot`
```c
void env_get_debug_snapshot(const BomberEnv* env, DebugSnapshot* out);
```
Returns a full snapshot of the environment state for visualization and debugging.

## Usage example

```c
BomberConfig cfg;
config_survival(&cfg);
cfg.seed = 1337;

BomberEnv env;
env_init(&env, &cfg);

Observation obs;
for (int step = 0; step < cfg.max_steps; step++) {
    env_observe(&env, 0, &obs);
    Action action = my_agent_act(&obs);
    StepResult result = env_step(&env, action);
    if (result.done) break;
}
```
