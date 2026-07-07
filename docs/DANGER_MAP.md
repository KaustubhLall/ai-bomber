# Danger Map

## Overview

The danger map is a first-class module that computes blast danger information for every tile on the grid. It is used by heuristic agents, the visualizer, reward logic, and tests.

## Data structure

```c
typedef struct {
    int current_blast[MAX_HEIGHT][MAX_WIDTH];   // 1 if tile is currently exploding
    int time_to_blast[MAX_HEIGHT][MAX_WIDTH];    // Ticks until blast reaches tile (-1 = safe)
    int safe_now[MAX_HEIGHT][MAX_WIDTH];         // 1 if tile is safe this tick
    int reachable_safe[MAX_HEIGHT][MAX_WIDTH];   // 1 if agent can reach this safe tile
    int action_safe[ACTION_COUNT];               // 1 if action leads to a safe tile
} DangerMap;
```

## Computation

### `danger_compute`
Computes `current_blast`, `time_to_blast`, and `safe_now` for all tiles based on active bombs:
- For each active bomb, compute its blast tiles using `compute_blast_tiles`.
- `time_to_blast` = minimum timer across all bombs that can reach a tile.
- `current_blast` = 1 for tiles where a bomb has timer == 0.
- `safe_now` = 0 for any tile in a blast path.

### `danger_compute_escape`
BFS from the agent's position to find reachable safe tiles:
- `reachable_safe[y][x]` = 1 if the agent can reach (x,y) before any bomb explodes there.
- `action_safe[a]` = 1 if taking action `a` leads to a safe tile. Movement actions use arrival-time safety: a tile is safe if the blast arrives after the agent does (time_to_blast > arrival_tick), not just if it's safe this tick.

### `danger_is_action_safe_at_arrival`
```c
int danger_is_action_safe_at_arrival(const DangerMap* dm, int x, int y, int arrival_tick);
```
Checks whether a tile is safe for an agent arriving at `arrival_tick` ticks from now. Returns 1 if the tile has no blast (`time_to_blast == -1`) or the blast arrives after the agent (`time_to_blast > arrival_tick`), and the tile is not currently exploding (`current_blast == 0`).

### `danger_would_trap_agent`
Simulates placing a bomb at a given position and checks if the agent can escape:
- Computes hypothetical blast tiles.
- BFS from agent position avoiding blast tiles.
- Returns 1 if no escape route exists (agent would be trapped).

### `danger_detect_dead_end`
Returns 1 if a tile has <= 1 walkable exit (potential trap location).

## Usage by agents

```c
// In agent act function:
if (obs->in_danger) {
    // Find a safe action
    for (int a = 0; a < 4; a++) {
        if (obs->safe_actions[a] && obs->valid_actions[a]) {
            return (Action)a;
        }
    }
}

// Check if placing a bomb would be safe
if (obs->safe_actions[ACTION_PLACE_BOMB]) {
    return ACTION_PLACE_BOMB;
}
```

## Visualizer overlay

The danger map is rendered as:
- Red gradient: tiles in danger (intensity based on time-to-blast)
- Green dots: reachable safe tiles
- Red circles: active blast zones
