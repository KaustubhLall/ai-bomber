# Visualizer

## Overview

The visualizer is built with raylib and provides a portfolio-quality dashboard for inspecting simulation state, agent behavior, and training metrics.

## Running

```bash
# Live mode
./build/bomber_viz --live --agent heuristic --seed 1337

# Replay mode
./build/bomber_viz --replay replay.bin
```

## Controls

| Key | Action |
|-----|--------|
| SPACE | Pause/Resume |
| R | Reset episode |
| + / = | Increase speed |
| - | Decrease speed |
| S | Single step (when paused) |
| ESC | Quit |

## Dashboard layout

The 1280x800 window is divided into three regions:

### Left panels (240px)
1. **Status panel**: Step, reward, agent status (alive/dead, position, ammo, range, speed, score), enemy status.
2. **Local observation panel**: 11x11 grid showing exactly what the agent sees (tiles, bombs, danger, agent center).
3. **Danger map panel**: Minimap with time-to-blast heatmap and safe reachable tiles.

### Center: Arena
- Full Bomberman arena with tiles, walls, crates, powerups.
- Bomb rendering with pulsing effect and countdown labels.
- Agent and enemy rendering with color-coded circles.
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

- Tiles are color-coded by type (floor, wall, crate, powerup variants).
- Bombs pulse based on remaining timer (closer to explosion = larger).
- Danger overlay alpha decreases with time-to-blast (urgent = opaque red).
- Agent colors are assigned per agent ID from a fixed palette.
