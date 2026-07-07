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
- Cloneable joint-action search API, alpha-beta and MCTS planning baselines
- Fixed-seed tournament matrices plus NumPy AlphaZero-lite and PPO comparison pipelines

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

# Reproducible holdout policy matrix in machine-readable form.
./build/bomber_benchmark --matrix --episodes 20 --seed 9001 --suite holdout --output results/matrix.json
```

## Learning baselines

```bash
python tools/learning.py alphazero-lite --seed 1 --output results/az.json --checkpoint results/az.npz
python tools/learning.py ppo --seed 1 --output results/ppo.json --checkpoint results/ppo.npz
python tools/plot_results.py results/ppo.json results/ppo.svg
```

These framework-free reference pipelines provide reproducible policy/value, replay-target, rollout/advantage, clipped-update, entropy, checkpoint, and fixed-holdout evaluation paths. Their compact shaped-reward arena is intentionally easier than the full C simulator; see [experiment protocol](docs/EXPERIMENTS.md) before making comparative claims.

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

Windows users can run the preset launchers in `shortcuts/`, or execute
`shortcuts/install-desktop-shortcuts.ps1` once to create MCTS, alpha-beta,
policy-comparison, live-policy-arena, and match-history shortcuts on the Desktop.

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
- **Match History (5)**: persistent recorded matches with seed, policies, outcome, causal eliminations, state hash, and frame-by-frame replay

### Visualizer controls

- **SPACE**: pause or resume
- **R**: reset all sessions
- **F2/F3**, **+/-**, or the visible **Game - / Game +** buttons: choose 1/2/3/5/10/30 simulation steps per second
- **F5/F6** or **Render - / Render +**: change only the window refresh rate
- **F7** or **Set Game**: type an exact simulation rate from 1 to 60 steps per second
- **M**: choose two policies, seed, open/standard/dense map, and 1-20 matches, then launch recorded live play
- **5**: open match history; use PageUp/PageDown for matches and Left/Right for frames
- **H**: open the in-app controls and powerup guide
- **S**: step once while paused
- **TAB**: switch active agent
- **1/2/3/4/5**: switch view (Arena/Compare/Graphs/Debug/History)
- **N**: start a new epoch for the active agent
- **L**: toggle legend
- **O**: toggle observation window overlay
- **D**: toggle danger overlay
- **G**: toggle grid lines
- **P**: save screenshot to `screenshots/ai-bomber-arena.png`
- **ESC**: quit

### Powerups

- **Bomb capacity (red)** adds one reusable bomb slot; its ammo returns after that bomb explodes.
- **Blast range (orange)** extends future bomb flames by one tile in each open direction.
- **Speed level (blue)** is recorded in state and observations, but does not currently alter grid movement speed.

With the default configuration, a destroyed crate has a 30% chance to reveal one of the three powerups.

The default visualizer now runs only MCTS against the heuristic opponent for one match and then pauses. Use the Live Policy Arena shortcut to choose any two built-in policies, the Match History shortcut to inspect recorded games, the Policy Comparison shortcut for five simultaneous policy sessions, `--epochs N` for repeated matches, or `--enemy builtin-random` for the old non-adversarial fallback. Battle matches terminate after 200 steps instead of running indefinitely.

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
| `alpha-beta` | Depth-limited deterministic lookahead using the tactical evaluator. |
| `mcts` | Fixed-budget Monte Carlo rollouts with action visits and values. |

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
