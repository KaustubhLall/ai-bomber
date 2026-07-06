# Architecture

## Design principles

1. **Simulator first**: The core simulation is independent of rendering, ML, or any external framework.
2. **Deterministic**: Same seed and same action sequence produce the same result.
3. **Fixed-size state**: The environment uses bounded arrays for maps, agents, bombs, replay data, and observations.
4. **Clean C surface area**: The environment API is intentionally small so future Python, C++, or model-runtime bindings are straightforward.
5. **Pluggable agents**: Agents implement a function-pointer interface and own per-instance state.

## Module overview

### Core (`src/core/`)

- `rng`: SplitMix64-based deterministic PRNG with no global state.
- `config`: Default configurations and bounds normalization.
- `math_util`: Inline math helpers.
- `ring_buffer`: Fixed-size circular buffer for action history.
- `replay`: Binary replay save/load helpers.
- `metrics`: Episode-level statistics tracking and reporting.

### Environment (`src/env/`)

- `env`: Main environment API: init, reset, step, observe, debug snapshot.
- `bomber_state`: Fixed-size state struct for tiles, agents, and bombs.
- `bomber_map`: Map generation, bounds checking, walkability, and bomb collision.
- `bomber_bombs`: Bomb placement and ticking.
- `bomber_blast`: Blast computation, crate destruction, chain reactions, and damage.
- `bomber_danger`: Time-to-blast map, safe tiles, escape routes, and trap detection.
- `bomber_observation`: Local observation tensor and compact debug observation.
- `bomber_reward`: Configurable reward components with a per-step breakdown.
- `bomber_rules`: Movement, placement, pickup, and terminal condition rules.

### Agents (`src/agents/`)

- `agent`: Generic agent factory and dispatch interface.
- `random_agent`: Uniform random baseline.
- `scripted_agent`: Rule-based baseline with danger avoidance and target seeking.
- `heuristic_bomber_agent`: Danger-map-aware baseline with escape route checks.
- `greedy_crate_agent`: Crate-focused baseline that avoids known danger.

### Simulation (`src/sim/`)

- `runner`: Episode runner with metrics and optional replay recording.
- `benchmark`: Performance benchmark entry point.
- `evaluator`: Agent comparison and aggregate evaluation.

### Visualizer (`src/viz/`)

- `main_viz`: raylib entry point with live and replay modes.
- `renderer`: Arena, overlays, panels, and status rendering.
- `dashboard`: Layout and event tracking.
- `charts`: Line and bar chart primitives.
- `ui_controls`: Button and slider widgets.
- `viz_session`: Multi-agent visualizer session state.

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
|  replay_record() optional  |
|       |                    |
+------------<---------------+
    |
    v
metrics_print() / replay_save()
```

## State representation

All game state is stored in `BomberState`:

- Fixed `MAX_HEIGHT x MAX_WIDTH` tile grid, currently capped at 31x31.
- Up to `MAX_AGENTS` agents.
- Up to `MAX_BOMBS` active or inactive bomb slots.
- No heap pointers inside the environment state.

## Determinism notes

- PRNG state is explicit and local to the environment or the agent instance.
- Map generation is seeded and deterministic.
- Built-in agent RNG state must be per instance; file-static agent state would make side-by-side sessions interfere with each other.
- The simple one-agent stepping API still owns the current environment opponent behavior. Explicit opponent policy wiring is tracked as a future runner improvement.
- Replays currently record the controlled agent action stream; full multi-policy replay should also store opponent policy metadata and seed schedule.
