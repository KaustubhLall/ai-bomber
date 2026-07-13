#ifndef BOMBER_STATE_H
#define BOMBER_STATE_H

#include "core/config.h"
#include "core/rng.h"

typedef enum {
    TILE_FLOOR = 0,
    TILE_SOLID_WALL,
    TILE_CRATE,
    TILE_POWERUP_BOMB,
    TILE_POWERUP_RANGE,
    TILE_POWERUP_SPEED
} TileType;

typedef struct {
    int x;
    int y;
    int alive;
    int bomb_ammo;
    int bombs_active;
    int blast_range;
    int speed;
    int score;
    int crates_destroyed;
    int eliminations;
    int powerups_collected;
} BomberAgentState;

typedef struct {
    int x;
    int y;
    int owner_id;
    int timer;
    int range;
    int active;
} BombState;

typedef struct BomberState {
    int width;
    int height;
    TileType tiles[MAX_HEIGHT][MAX_WIDTH];
    /* Persistent flame: ticks of lethal fire remaining on each tile (0 = none).
       Set to cfg->flame_duration when a bomb detonates over the tile, decremented
       once per step; any agent on a tile with flame_ttl > 0 is killed. This makes
       blasts have canonical area-denial rather than a single-tick instant hit. */
    int flame_ttl[MAX_HEIGHT][MAX_WIDTH];
    int flame_owner[MAX_HEIGHT][MAX_WIDTH]; /* bomb owner that lit each flame tile, -1 if none */
    BomberAgentState agents[MAX_AGENTS];
    BombState bombs[MAX_BOMBS];
    int agent_count;
    int death_owner[MAX_AGENTS]; /* bomb owner that eliminated each agent, -1 if alive/unknown */
    int flame_duration; /* copied from config at reset so blast code needs no extra params */
    int step;
    float total_reward;
} BomberState;

#endif /* BOMBER_STATE_H */
