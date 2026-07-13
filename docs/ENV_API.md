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
Initializes the environment with the given configuration. Calls `env_reset`
internally. `BomberEnv` must be zero-initialized before its first API call. An
opponent installed with `env_set_opponent` on a zero-initialized environment is
preserved by `env_init`.

### `env_reset`
```c
void env_reset(BomberEnv* env, uint64_t seed);
```
Resets the environment with a deterministic seed. Regenerates the map, resets
agents, and clears bombs. Policy wiring is preserved.

### `env_step`
```c
StepResult env_step(BomberEnv* env, Action action);
```
Executes one step for agent 0 with the given action. Internally:
1. Executes the agent's action (move, bomb, wait).
2. Runs enemy AI for other agents (uses opponent agent if set via `env_set_opponent`, otherwise built-in AI).
3. Picks up powerups.
4. Ticks all bombs (decrement timers, explode at zero).
5. Recomputes danger map.
6. Computes reward.
7. Checks terminal conditions.

Returns the step reward, done flag, and terminal reason.

### `env_set_opponent`
```c
void env_set_opponent(BomberEnv* env, Agent* opponent);
```
Sets one explicit opponent policy shared by enemy agents 1..N. Pass `NULL` to
revert to the built-in random fallback. The opponent is queried once per living
enemy each step. The environment does not call this self-play unless both agent 0
and the opponent are explicitly configured with policies.

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
