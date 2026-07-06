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
} BomberAgentState;

typedef struct {
    int x;
    int y;
    int owner_id;
    int timer;
    int range;
    int active;
} BombState;

typedef struct {
    int width;
    int height;
    TileType tiles[MAX_HEIGHT][MAX_WIDTH];
    BomberAgentState agents[MAX_AGENTS];
    BombState bombs[MAX_BOMBS];
    int agent_count;
    int step;
    float total_reward;
} BomberState;

#endif /* BOMBER_STATE_H */
