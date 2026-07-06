# Observation Design

## Overview

The observation system provides two modes: **compact** for ML training and **debug** for visualization.

## Compact observation

The `Observation` struct contains everything an agent or model needs to make decisions:

### Agent info
- `agent_x`, `agent_y`: Agent position
- `agent_alive`: Whether agent is alive
- `agent_bomb_ammo`: Available bombs
- `agent_blast_range`: Blast range
- `agent_speed`: Speed level
- `agent_score`: Current score

### Local grid (11x11)
Centered on the agent, providing:
- `local_tiles[y][x]`: Tile type (floor, wall, crate, powerup)
- `local_bombs[y][x]`: Bomb presence (0/1)
- `local_bomb_timers[y][x]`: Bomb timer (-1 if no bomb)
- `local_danger[y][x]`: Time-to-blast from danger map (-1 if safe)

### Enemy info
- `enemy_count`: Number of enemies
- `enemy_x[]`, `enemy_y[]`: Enemy positions
- `enemy_alive[]`: Enemy alive status

### Powerup info
- `powerup_count`: Number of powerups on map
- `powerup_x[]`, `powerup_y[]`: Powerup positions

### Action info
- `valid_actions[ACTION_COUNT]`: Which actions are valid (can execute)
- `safe_actions[ACTION_COUNT]`: Which actions lead to safe tiles
- `prev_action`: Last action taken

### Danger info
- `in_danger`: Whether agent is currently in a blast zone
- `danger_timer`: Ticks until blast reaches agent (-1 if safe)

## Flat observation vector

For ML models, `obs_to_flat()` converts the observation to a flat float array:

| Section | Size | Description |
|---------|------|-------------|
| Agent info | 7 | x, y, alive, ammo, range, speed, score |
| Local tiles | 121 | 11x11 tile types |
| Local bombs | 121 | 11x11 bomb presence |
| Local bomb timers | 121 | 11x11 bomb timers |
| Local danger | 121 | 11x11 time-to-blast |
| Valid actions | 6 | One-hot valid action mask |
| Safe actions | 6 | One-hot safe action mask |
| Danger info | 2 | in_danger, danger_timer |
| Previous action | 1 | Last action |
| **Total** | **506** | Fixed size |

The flat vector has a fixed shape for a given config, making it suitable for neural network input.

## Debug observation

The `DebugSnapshot` struct contains:
- Full `BomberState` (all tiles, agents, bombs)
- `DangerMap` (full-grid danger data)
- `RewardBreakdown` (last reward components)
- `last_action` and `decision_text` (for decision trace)

This is used by the visualizer and for debugging.
