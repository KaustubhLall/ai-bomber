# AI Bomber

A deterministic Bomberman-style AI simulation environment, headless benchmark harness, and raylib visualizer written in C17.

The project is built around a small fixed-size simulator that can run without a renderer or ML framework. Agents interact with the same observation and action interface whether they are rule-based baselines today or external model policies later.

## What this demonstrates

- A simulator-first architecture with deterministic seeds and replayable episodes
- Fixed-size C data structures for predictable performance and simple debugging
- Pluggable agents for random, scripted, heuristic, and crate-focused baselines
- A local observation format designed to be exported to Python, C++, or another training loop
- A raylib visualizer for inspecting reward, danger, bombs, local observations, and agent behavior
- CTest coverage for core environment rules, determinism, replay, rewards, agents, metrics, and hardened invariants

## Current status

The current implementation is a local research sandbox, not a trained neural agent. It includes rule-based baselines and a stable C interface so model training can be added without rewriting the simulator. The most useful next step is to connect the observation/action API to a training process and compare learned policies against the included baselines.

## Build

### Prerequisites

- C17 compiler: GCC, Clang, or MSVC
- CMake 3.16+
- Ninja, recommended for fast local builds

### Build instructions

```bash
# Configure a release build.
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build everything, including the raylib visualizer.
cmake --build build

# Build the simulator, CLIs, and tests without the visualizer.
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAI_BOMBER_BUILD_VIZ=OFF
cmake --build build
```

raylib is fetched automatically through CMake FetchContent when it is not already available on the system.

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Run headless simulations

```bash
# Random agent, 1000 episodes.
./build/bomber_headless --agent random --episodes 1000 --seed 1337

# Heuristic agent with metrics export.
./build/bomber_headless --agent heuristic --episodes 1000 --seed 1337 --export metrics.json

# Battle mode with a heuristic opponent.
./build/bomber_headless --mode battle --agent heuristic --enemy scripted --episodes 100

# Battle mode with built-in opponent AI (no --enemy flag).
./build/bomber_headless --mode battle --agent heuristic --episodes 100

# Compare the built-in agents.
./build/bomber_headless --compare --episodes 500

# Save a replay of the first episode.
./build/bomber_headless --agent heuristic --episodes 1 --seed 42 --replay replay.bin
```

`--agent` controls agent 0. `--enemy` installs one shared opponent policy for
enemies 1..N; when it is omitted, enemies use the environment's
`built-in-random` fallback. These are independent choices: a run is not self-play
unless both sides are explicitly configured with the same policy.

## Run benchmark

```bash
./build/bomber_benchmark --episodes 10000 --seed 1
```

## Run visualizer

```bash
# Default: compare random, heuristic, and greedy agents across epochs.
./build/bomber_viz --seed 1337 --epochs 500

# Specific agents, repeating --agent for each policy.
./build/bomber_viz --agent random --agent scripted --agent heuristic --agent greedy

# Single agent.
./build/bomber_viz --agent heuristic --enemy random --agents 2 --seed 1337

# Replay mode.
./build/bomber_viz --replay replay.bin
```

Visualizer matchup configuration mirrors the headless runner: `--agent` selects
agent 0, `--enemy` selects the opponent policy, `--agents` sets the total arena
agent count, and `--seed` controls deterministic setup. With three or more agents,
the selected opponent policy is shared by enemies 1..N. Omitting `--enemy` is
shown explicitly as `Opponent Policy: built-in-random`.

### Visualizer views

- **Arena View (1)**: redesigned dashboard with large centered arena, top status bar, right inspector panel (decision trace, agent/enemy stats, active bombs), bottom event timeline, and optional legend
- **Compare View (2)**: side-by-side mini arenas for all configured agents with a shared training overview panel
- **Graphs View (3)**: per-epoch reward, running average reward, action distribution, and training overview table for all agents
- **Debug View (4)**: detailed technical view with raw local observation, danger map, and event log

### Visualizer controls

- **SPACE**: pause or resume
- **R**: reset all sessions
- **+/-**: adjust speed multiplier
- **S**: step once while paused
- **TAB**: switch active agent
- **1/2/3/4**: switch view (Arena/Compare/Graphs/Debug)
- **N**: start a new epoch for the active agent
- **L**: toggle legend
- **O**: toggle observation window overlay
- **D**: toggle danger overlay
- **G**: toggle grid lines
- **P**: save screenshot to `screenshots/ai-bomber-arena.png`
- **ESC**: quit

### Screenshots

To capture a screenshot of the current visualizer state:
1. Navigate to the desired view (typically Arena View for best results)
2. Press **P** to save a screenshot
3. Screenshots are saved to the `screenshots/` directory

Note: The `screenshots/` directory is created automatically on first screenshot capture. Generated screenshots should not be committed to version control unless specifically intended for documentation.

## Project structure

```text
src/
  core/       RNG, config, math utilities, ring buffer, replay, metrics
  env/        environment state, rules, maps, bombs, blasts, danger, observation, reward
  agents/     agent interface and built-in policies
  sim/        runner, benchmark, evaluator
  viz/        raylib visualizer and session manager
  cli/        headless and benchmark entry points
tests/        CTest suite
docs/         design notes and API documentation
assets/       optional configs, sprites, and fonts
```

## Built-in agents

| Agent | Description |
| --- | --- |
| `random` | Uniform random action selection. Useful as a sanity-check baseline. |
| `scripted` | Avoids immediate danger, bombs adjacent crates, and seeks visible powerups. |
| `heuristic` | Uses danger information and escape checks before bombing. |
| `greedy` / `greedy_crate` | Prioritizes crate destruction while still avoiding known danger. |

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Environment API](docs/ENV_API.md)
- [Observation Design](docs/OBSERVATION.md)
- [Reward System](docs/REWARD.md)
- [Danger Map](docs/DANGER_MAP.md)
- [Visualizer](docs/VISUALIZER.md)
- [Adding Agents](docs/ADDING_AGENTS.md)
- [Future Model Integration](docs/FUTURE_MODELS.md)

## Roadmap

- External policy bridge for Python or another model runtime
- Better replay metadata for full multi-policy reproduction
- Stable benchmark fixtures for comparing simulator changes over time
