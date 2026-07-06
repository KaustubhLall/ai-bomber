# Architecture

## Design principles

1. **Simulator first**: The core simulation is independent of rendering, ML, or any external framework.
2. **Deterministic**: Same seed + same actions = same result, every time, on every platform.
3. **No heap allocation in inner loop**: All state uses fixed-size arrays.
4. **Clean C ABI**: Designed for future Python/ctypes/cffi integration.
5. **Pluggable agents**: Agents implement a simple function pointer interface.

## Module overview

### Core (`src/core/`)
- `rng`: SplitMix64-based deterministic PRNG. No global state.
- `config`: Default and custom game configurations.
- `math_util`: Inline math helpers (clamp, abs, min, max, manhattan distance).
- `ring_buffer`: Fixed-size circular buffer for action history.
- `replay`: Binary replay save/load and deterministic playback.
- `metrics`: Episode-level statistics tracking and reporting.

### Environment (`src/env/`)
- `env`: Main environment API (init, reset, step, observe, debug snapshot).
- `bomber_state`: Fixed-size state struct (tiles, agents, bombs).
- `bomber_map`: Map generation, bounds checking, walkability, bomb collision.
- `bomber_bombs`: Bomb placement and ticking.
- `bomber_blast`: Blast computation, crate destruction, chain reactions, damage.
- `bomber_danger`: Danger map with time-to-blast, safe tiles, escape routes, trap detection.
- `bomber_observation`: Compact local observation for ML + debug observation for viz.
- `bomber_reward`: Configurable reward components with breakdown tracking.
- `bomber_rules`: Movement, bomb placement, powerup pickup, terminal condition checks.

### Agents (`src/agents/`)
- `agent`: Generic agent interface with function pointers.
- `random_agent`: Uniform random action selection.
- `scripted_agent`: Rule-based baseline (danger avoidance, crate bombing, powerup seeking).
- `heuristic_bomber_agent`: Danger-map-aware agent with escape route verification.
- `greedy_crate_agent`: Crate-focused agent that avoids suicide.

### Sim (`src/sim/`)
- `runner`: Episode runner with metrics and replay recording.
- `benchmark`: Performance benchmarking.
- `evaluator`: Agent comparison and evaluation.

### Visualizer (`src/viz/`)
- `main_viz`: raylib entry point with live and replay modes.
- `renderer`: All drawing functions for arena, panels, overlays.
- `dashboard`: Layout manager and event detection.
- `charts`: Line and bar chart primitives.
- `ui_controls`: Button and slider widgets.

## Data flow

```
Config + Seed
    |
    v
env_init() -> env_reset()
    |
    v
+---<--- loop ---<---+
|                   |
|  env_observe()    |
|       |           |
|       v           |
|  agent_act()      |
|       |           |
|       v           |
|  env_step()       |
|       |           |
|       v           |
|  metrics_update() |
|       |           |
|  replay_record()  |
|       |           |
+-------<-----------+
    |
    v
  done? -> metrics_print() / replay_save()
```

## State representation

All game state is in `BomberState`:
- Fixed `MAX_HEIGHT x MAX_WIDTH` tile grid (31x31 max)
- Up to `MAX_AGENTS` (8) agents
- Up to `MAX_BOMBS` (64) bombs
- No pointers, no heap allocation

## Determinism

- PRNG: SplitMix64 with explicit seed, no global state.
- Map generation: deterministic from seed.
- Enemy AI: uses the same RNG stream (sequential, not parallel).
- Replay: seed + action list is sufficient for exact reproduction.
