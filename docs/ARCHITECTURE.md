# Architecture

AI Bomber is organized around a small deterministic simulation core. Rendering, command-line tooling, and future ML integrations sit outside that core so the simulator can stay fast, testable, and easy to embed.

## Design principles

1. **Simulator first**: The environment is independent of rendering, Python, and external ML frameworks.
2. **Deterministic**: Same seed and same controlled-agent action sequence should produce the same result.
3. **Fixed-size state**: Core state uses bounded arrays instead of per-step heap allocation.
4. **Portable C ABI**: The public structs and functions are designed to be callable from C, C++, Python `ctypes`/`cffi`, or other FFI layers.
5. **Pluggable controlled agents**: Baseline agents implement a simple function-pointer interface.

## Module overview

### Core (`src/core/`)

- `rng`: SplitMix64-based deterministic PRNG with explicit state.
- `config`: Default game configuration presets and reward weights.
- `math_util`: Inline helpers for clamp, absolute value, min/max, and Manhattan distance.
- `ring_buffer`: Fixed-size circular buffer for action history.
- `replay`: Native binary replay save/load and deterministic playback.
- `metrics`: Episode-level statistics tracking and reporting.

### Environment (`src/env/`)

- `env`: Main environment API: init, reset, step, observe, and debug snapshot.
- `bomber_state`: Fixed-size tile, agent, and bomb state.
- `bomber_map`: Map generation, bounds checks, walkability, and bomb lookup.
- `bomber_bombs`: Bomb placement and ticking.
- `bomber_blast`: Blast tile computation, crate destruction, chain reactions, and damage.
- `bomber_danger`: Time-to-blast, safe tile, escape route, and trap checks.
- `bomber_observation`: Compact local observations for agents and visualizer diagnostics.
- `bomber_reward`: Reward component breakdown and total reward calculation.
- `bomber_rules`: Movement, bomb placement, powerup pickup, and terminal conditions.

### Agents (`src/agents/`)

- `agent`: Generic controlled-agent interface with function pointers.
- `random_agent`: Uniform random action selection.
- `scripted_agent`: Rule-based baseline for danger avoidance, crate bombing, and powerup seeking.
- `heuristic_bomber_agent`: Danger-map-aware baseline that checks escape routes before bombing.
- `greedy_crate_agent`: Crate-focused baseline that still avoids immediate suicide.

### Sim (`src/sim/`)

- `runner`: Episode runner with metrics and replay recording.
- `benchmark`: Performance benchmarking.
- `evaluator`: Agent comparison and evaluation.

### Visualizer (`src/viz/`)

- `main_viz`: raylib entry point with live and replay modes.
- `renderer`: Arena, panels, overlays, and sprite drawing.
- `dashboard`: Layout and higher-level view composition.
- `charts`: Line and bar chart primitives.
- `ui_controls`: Button and slider widgets.
- `viz_session`: Multi-agent session state for visualizer comparisons.

## Data flow

```text
Config + seed
    |
    v
env_init() -> env_reset()
    |
    v
+---<--- episode loop ---<---+
|                            |
|  env_observe()             |
|       |                    |
|       v                    |
|  agent_act()               |
|       |                    |
|       v                    |
|  env_step()                |
|       |                    |
|       v                    |
|  metrics_update()          |
|       |                    |
|  replay_record()           |
|       |                    |
+-------<--------------------+
    |
    v
metrics_print() / replay_save()
```

## State representation

All game state is stored in `BomberState`:

- Fixed `MAX_HEIGHT x MAX_WIDTH` tile grid, currently capped at 31x31.
- Up to `MAX_AGENTS` agents, currently capped at 8.
- Up to `MAX_BOMBS` active bombs, currently capped at 64.
- No pointers inside the state struct, which keeps snapshots and replay frames cheap to copy.

## Determinism model

- Map generation uses an explicit `RNG` seeded by `env_reset`.
- Controlled agent action history is stored in a fixed ring buffer.
- Baseline enemy behavior uses the environment RNG stream, not global randomness.
- Native replay playback stores configuration, seed, and action sequence. The binary replay format is optimized for local development and should not be treated as a cross-version archival format.

## Current limitations

- Opponent policy injection is not fully wired into the headless runner yet; non-controlled agents currently use the environment's seeded baseline behavior.
- The simulator is suitable for policy experiments and future ML integration, but there is no neural training loop in this repository yet.
- Config values are expected to stay within the fixed compile-time bounds defined in `core/config.h`.
