# Visualizer

## Overview

The visualizer is built with raylib and provides a portfolio-quality dashboard for inspecting simulation state, agent behavior, and training metrics.

## Running

```bash
# Live mode
./build/bomber_viz --live --agent heuristic --seed 1337

# Replay mode
./build/bomber_viz --replay replay.bin

# Independent simultaneous matchups in View 2 (repeat up to eight)
./build/bomber_viz --view compare \
  --matchup mcts:heuristic --matchup heuristic:mcts \
  --matchup mcts:greedy --matchup greedy:mcts --epochs 20
```

## Controls

| Key | Action |
|-----|--------|
| SPACE | Pause/Resume |
| R | Reset episode |
| + / = | Increase speed |
| - | Decrease speed |
| F2 / F3, +/- or Game buttons | Choose simulation steps per second (1/2/3/5/10/30) |
| F5 / F6 or Render buttons | Change only the window render FPS |
| F7 or Set Game | Enter exact simulation steps per second (1-60) |
| M | Open live policy-vs-policy arena picker |
| 2 | Open the simultaneous matchup board |
| Click a View 2 game | Select it; press 1 for full-size inspection |
| 5 | Open persistent match history and replay |
| H | Toggle help and powerup guide |
| S | Single step (when paused) |
| ESC | Quit |

Simulation speed is a real-time game clock, independent of rendering. At 1 step/s
the state advances once per wall-clock second whether the window renders at 15,
60, or 240 FPS. Match history controls are PageUp/PageDown (match), Left/Right
(frame), Home (restart), and Space (play/pause).

## Simultaneous matchup board

View 2 renders one to eight independent live games in an adaptive grid. Each
card owns its blue and red policy, replay, reward, seed, epoch, step, and
outcome. Reversing a pair (for example `mcts:heuristic` and
`heuristic:mcts`) gives a role-balanced visual comparison on the same seed.
All cards advance from the same game clock but maintain independent state.

With no explicit `--agent` or `--matchup`, the visualizer creates a six-game
board for 20 epochs. The Policy Comparison desktop shortcut opens this board
directly. Legacy repeated `--agent` arguments still work and use the shared
`--enemy` policy.

## Dashboard layout

The 1280x800 window is divided into three regions:

### Left panels (240px)
1. **Status panel**: Step, reward, agent status (alive/dead, position, ammo, range, speed, score), enemy status.
2. **Local observation panel**: 11x11 grid showing exactly what the agent sees (tiles, bombs, danger, agent center).
3. **Danger map panel**: Minimap with time-to-blast heatmap and safe reachable tiles.

### Center: Arena
- Full Bomberman arena with tiles, walls, crates, powerups.
- Bomb rendering with pulsing effect and countdown labels.
- Agent and enemy rendering with color-coded Bomberman-style sprites.
- Danger overlay (red gradient for blast zones).
- Safe tile indicators (green dots).
- Local observation box (yellow outline around agent's view).

### Right panels (280px)
1. **Reward graph**: Reward over time with zero line.
2. **Action distribution**: Bar chart of action frequencies.
3. **Bomb timeline**: Active bombs with owner, position, timer, range.
4. **Decision trace**: Text explanation of the agent's last decision.
5. **Event log**: Scrollable log of game events (bomb placed, crate destroyed, etc.).
6. **Controls**: Key bindings and current speed/pause status.

## Event detection

The dashboard automatically detects and logs events:
- Bomb placed
- Bomb exploded
- Crate destroyed
- Powerup collected
- Agent died
- Enemy eliminated
- Match won

## Rendering details

- Tiles use simple procedural sprites: masonry pillars, wooden crates, and distinct powerup icons.
- Bombs pulse based on remaining timer (closer to explosion = larger).
- Danger overlay alpha decreases with time-to-blast (urgent = opaque red).
- Agent colors are assigned per agent ID from a fixed palette.

## Map generation

Maps follow the classic Bomberman layout: odd dimensions, an indestructible
outer border, indestructible pillars at every even/even coordinate, seeded
random destructible crates on the remaining floor, and a three-tile L-shaped
safe pocket at each of the four corner spawns.
