# AI Bomber

A high-performance Bomberman-style AI simulation environment and visualizer built in C17 with raylib.

## Overview

AI Bomber is a portfolio-quality local AI training sandbox where different agents learn to survive, place bombs, destroy crates, avoid blast zones, collect powerups, and eliminate opponents. The core is a clean, fast, deterministic simulator with a polished visualizer.

**Key principles:**
- Simulator first, models second
- Deterministic seeded simulation
- No heap allocation in the inner loop
- Clean C ABI for future Python/ML integration
- No dependency on PyTorch, TensorFlow, or any ML framework

## Build

### Prerequisites

- C17 compiler (GCC, Clang, or MSVC)
- CMake 3.16+
- Ninja (recommended)

### Build instructions

```bash
# Configure
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build everything (including visualizer with raylib)
cmake --build build

# Build without visualizer (headless only)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAI_BOMBER_BUILD_VIZ=OFF
cmake --build build
```

raylib is fetched automatically via CMake FetchContent when not found on the system.

## Run tests

```bash
cd build
ctest --output-on-failure
```

## Run headless simulation

```bash
# Random agent, 1000 episodes
./build/bomber_headless --agent random --episodes 1000 --seed 1337

# Heuristic agent with metrics export
./build/bomber_headless --agent heuristic --episodes 1000 --seed 1337 --export metrics.json

# Battle mode
./build/bomber_headless --mode battle --agent heuristic --enemy scripted --episodes 100

# Compare all agents
./build/bomber_headless --compare --episodes 500

# Save replay
./build/bomber_headless --agent heuristic --episodes 1 --seed 42 --replay replay.bin
```

## Run benchmark

```bash
./build/bomber_benchmark --episodes 10000 --seed 1
```

## Run visualizer

```bash
# Live mode with heuristic agent
./build/bomber_viz --live --agent heuristic --seed 1337

# Replay mode
./build/bomber_viz --replay replay.bin
```

### Visualizer controls

- **SPACE**: Pause/Resume
- **R**: Reset episode
- **+/-**: Speed multiplier
- **S**: Step (when paused)
- **ESC**: Quit

## Project structure

```
/src
  /core       - RNG, config, math utils, ring buffer, replay, metrics
  /env        - Bomberman environment, state, map, bombs, blast, danger, observation, reward
  /agents     - Agent interface and implementations (random, scripted, heuristic, greedy)
  /sim        - Runner, benchmark, evaluator
  /viz        - Raylib visualizer (renderer, dashboard, charts, UI controls)
  /cli        - Headless and benchmark CLI tools
/tests        - CTest test suite
/docs         - Documentation
/assets       - Config files, sprites, fonts
```

## Agents

| Agent | Description |
|-------|-------------|
| `random` | Uniform random actions |
| `scripted` | Avoids blast danger, bombs near crates, seeks powerups |
| `heuristic` | Uses danger map, only bombs with escape route, prioritizes survival |
| `greedy` | Focuses on crate destruction while avoiding death |

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Environment API](docs/ENV_API.md)
- [Observation Design](docs/OBSERVATION.md)
- [Reward System](docs/REWARD.md)
- [Danger Map](docs/DANGER_MAP.md)
- [Visualizer](docs/VISUALIZER.md)
- [Adding Agents](docs/ADDING_AGENTS.md)
- [Future Model Integration](docs/FUTURE_MODELS.md)
