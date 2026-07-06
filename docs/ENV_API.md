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

## Main API functions

### `env_init`

```c
void env_init(BomberEnv* env, const BomberConfig* config);
```

Initializes the environment with the supplied configuration and calls `env_reset` internally using `config->seed`.

### `env_reset`

```c
void env_reset(BomberEnv* env, uint64_t seed);
```

Resets the environment with a deterministic seed. This regenerates the map, respawns agents, clears bombs, resets action history, and recomputes danger state.

### `env_step`

```c
StepResult env_step(BomberEnv* env, Action action);
```

Executes one step for controlled agent `0`.

Order of operations:

1. Record the controlled agent action.
2. Execute movement, bomb placement, or wait.
3. Pick up any powerup on the controlled agent's tile and preserve that pickup for reward/metrics accounting before clearing the tile.
4. Run deterministic seeded baseline behavior for other agents.
5. Tick bombs, including explosions and chain reactions.
6. Recompute danger and escape maps.
7. Advance the step counter.
8. Compute reward components and total reward.
9. Check terminal conditions.

Returns the step reward, done flag, and terminal reason.

### `env_observe`

```c
void env_observe(const BomberEnv* env, int agent_id, Observation* obs);
```

Computes the observation for the requested agent. See [OBSERVATION.md](OBSERVATION.md).

### `env_get_debug_snapshot`

```c
void env_get_debug_snapshot(const BomberEnv* env, DebugSnapshot* out);
```

Copies the full environment state, danger map, latest reward breakdown, latest action, cumulative reward, and a short decision string for the visualizer/debug UI.

## Rules helpers

### `rules_pickup_powerup`

```c
int rules_pickup_powerup(BomberState* state, int agent_id);
```

Applies the powerup at the agent's current tile and clears that tile to floor. Returns `1` when a powerup was collected and `0` otherwise. `env_step` uses this return value for powerup reward and metrics accounting.

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
