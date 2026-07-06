# AI Bomber

AI Bomber is a fast, deterministic Bomberman-style simulation environment with a headless runner, baseline agents, replay capture, metrics, benchmarks, and an optional raylib visualizer.

The project is intentionally simulator-first: the C17 core has fixed-size state, deterministic seeding, and no dependency on Python or any ML framework. That makes it a compact sandbox for experimenting with agent policies first and connecting learning systems later.

## What is implemented

- Deterministic map generation, bomb ticking, blast propagation, crate destruction, powerups, terminal states, and reward breakdowns.
- Fixed-size C structs for environment state, agents, bombs, observations, replay frames, and metrics.
- Baseline agents: random, scripted, danger-aware heuristic, and greedy crate-focused.
- Headless CLI for episode runs, agent comparison, metrics export, and replay capture.
- raylib visualizer with arena, graph, and comparison views.
- CTest suite covering RNG, maps, bombs, blasts, danger maps, observations, rewards, replay, agents, and determinism.

## Current scope

AI Bomber is not a neural-network training framework yet. The repository provides the simulation loop, baselines, metrics, and visualizer that a training stack can use. Future Python/ML integration is documented separately in [Future Model Integration](docs/FUTURE_MODELS.md).

Battle-mode enemy behavior is currently driven inside the environment by a deterministic seeded baseline. The headless runner selects the controlled agent policy; full opponent-policy injection is a natural next step.

## Build

### Prerequisites

- C17 compiler: GCC, Clang, or MSVC
- CMake 3.16+
- Ninja or your platform's default CMake generator

### Headless build, tests, and benchmarks

Use this path when you only need the simulator, CLI, and tests.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DAI_BOMBER_BUILD_VIZ=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure

./build/bomber_headless --agent heuristic --episodes 1000 --seed 1337
./build/bomber_benchmark --episodes 10000 --seed 1
```

### Visualizer build

raylib is fetched automatically with CMake FetchContent when it is not installed locally.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bomber_viz --agent heuristic --seed 1337
```

On Windows with Visual Studio generators, executables may be under `build/Release/` instead of directly under `build/`.

## Headless usage

```bash
# Random agent, 1000 episodes
./build/bomber_headless --agent random --episodes 1000 --seed 1337

# Heuristic agent with metrics export
./build/bomber_headless --agent heuristic --episodes 1000 --seed 1337 --export metrics.json

# Battle mode with the environment's seeded baseline opponent behavior
./build/bomber_headless --mode battle --agent heuristic --episodes 100

# Compare all built-in controlled-agent policies
./build/bomber_headless --compare --episodes 500

# Save a replay for the first episode
./build/bomber_headless --agent heuristic --episodes 1 --seed 42 --replay replay.bin
```

Supported controlled agents are `random`, `scripted`, `heuristic`, and `greedy`.

## Visualizer

```bash
# Default: compare random, heuristic, and greedy agents across epochs
./build/bomber_viz --seed 1337 --epochs 500

# Specific agents, repeated for multiple panels
./build/bomber_viz --agent random --agent scripted --agent heuristic --agent greedy

# Replay mode
./build/bomber_viz --replay replay.bin
```

### Views

- **Arena View (1)**: Board, danger overlay, local observation, status panel, reward graph, action distribution, bomb timeline, decision trace, event log, and controls.
- **Graphs View (2)**: Per-epoch reward, running average reward, action distribution, and training overview table.
- **Comparison View (3)**: Side-by-side mini arenas for several agents.

### Controls

- **Space**: Pause/resume
- **R**: Reset sessions
- **+ / -**: Adjust speed
- **S**: Step while paused
- **Tab**: Switch active agent
- **1 / 2 / 3**: Switch view
- **N**: New epoch for active agent
- **Esc**: Quit

## Project structure

```text
src/
  core/       RNG, config, math helpers, ring buffer, replay, metrics
  env/        Bomberman state, rules, map, bombs, blast, danger, observations, rewards
  agents/     Random, scripted, heuristic, and greedy baseline policies
  sim/        Episode runner, benchmark, evaluator
  viz/        raylib renderer, dashboard, charts, controls, session manager
  cli/        Headless and benchmark executables
tests/        CTest coverage for simulator components and determinism
docs/         Architecture and integration notes
assets/       Config files, sprites, and fonts
```

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Environment API](docs/ENV_API.md)
- [Observation Design](docs/OBSERVATION.md)
- [Reward System](docs/REWARD.md)
- [Danger Map](docs/DANGER_MAP.md)
- [Visualizer](docs/VISUALIZER.md)
- [Adding Agents](docs/ADDING_AGENTS.md)
- [Future Model Integration](docs/FUTURE_MODELS.md)

## Quality checks

Before opening changes, run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DAI_BOMBER_BUILD_VIZ=OFF
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

For performance-sensitive changes, also run the benchmark in Release mode and compare steps/sec before and after the change.
