# Reward System

## Overview

The reward system uses configurable components that are tracked separately and summed to a total. This allows analysis of what drives agent behavior.

## Reward components

| Component | Default | Description |
|-----------|---------|-------------|
| `survival_reward` | +0.01 | Per-step reward for being alive |
| `crate_destroy_reward` | +0.20 | Per crate destroyed by agent's bombs |
| `powerup_reward` | +0.30 | Per powerup collected |
| `enemy_damage_reward` | +0.50 | Per enemy damaged |
| `enemy_elimination_reward` | +1.00 | Per enemy eliminated |
| `win_reward` | +2.00 | For winning a match |
| `death_penalty` | -1.00 | For agent dying |
| `timeout_penalty` | -0.10 | For episode timeout |
| `invalid_action_penalty` | -0.05 | For invalid actions (blocked moves, no ammo) |
| `suicidal_bomb_penalty` | -0.30 | For placing a bomb that traps the agent |
| `stall_penalty` | -0.01 | For prolonged inactivity |
| `escape_danger_reward` | +0.15 | For moving from danger to safety |
| `trap_opportunity_reward` | +0.25 | For creating a trap opportunity |

## RewardBreakdown struct

```c
typedef struct {
    float survival;
    float crate_destroyed;
    float powerup;
    float enemy_damage;
    float enemy_elimination;
    float win;
    float escape_danger;
    float trap_opportunity;
    float invalid_action_penalty;
    float suicidal_bomb_penalty;
    float stall_penalty;
    float death_penalty;
    float timeout_penalty;
    float total;
} RewardBreakdown;
```

## Anti-hacking measures

The reward system is designed to prevent common reward hacking strategies:

- **Hiding forever**: `stall_penalty` activates after 10 steps of no movement; `timeout_penalty` applies at max steps.
- **Bomb spamming**: Limited by bomb ammo; `suicidal_bomb_penalty` discourages reckless placement.
- **Suicide bombing**: `death_penalty` (-1.0) outweighs `crate_destroy_reward` (+0.2), making suicide unprofitable.
- **Passive play**: `survival_reward` is small (+0.01) so active crate destruction and powerup collection are encouraged.

## Configuration

All reward values are configurable via `BomberConfig`:

```c
BomberConfig cfg;
config_survival(&cfg);
cfg.survival_reward = 0.02f;
cfg.death_penalty = -2.0f;
// etc.
```
