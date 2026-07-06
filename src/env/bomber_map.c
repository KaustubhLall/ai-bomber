#include "env/bomber_map.h"
#include "core/math_util.h"

void map_generate(BomberState* state, const BomberConfig* cfg, RNG* rng) {
    state->width = cfg->width;
    state->height = cfg->height;

    /* Fill with floor */
    for (int y = 0; y < state->height; y++) {
        for (int x = 0; x < state->width; x++) {
            state->tiles[y][x] = TILE_FLOOR;
        }
    }

    /* Solid walls: border + grid pattern (every even row/col intersection) */
    for (int y = 0; y < state->height; y++) {
        for (int x = 0; x < state->width; x++) {
            if (x == 0 || y == 0 || x == state->width - 1 || y == state->height - 1) {
                state->tiles[y][x] = TILE_SOLID_WALL;
            } else if (x % 2 == 0 && y % 2 == 0) {
                state->tiles[y][x] = TILE_SOLID_WALL;
            }
        }
    }

    /* Place crates randomly on floor tiles, keeping spawn corners clear */
    for (int y = 1; y < state->height - 1; y++) {
        for (int x = 1; x < state->width - 1; x++) {
            if (state->tiles[y][x] != TILE_FLOOR) continue;
            /* Keep spawn corners (agent start positions) clear */
            int is_spawn = 0;
            int spawn_positions[8][2] = {
                {1, 1}, {2, 1}, {1, 2},
                {state->width - 2, 1}, {state->width - 3, 1}, {state->width - 2, 2},
                {1, state->height - 2}, {state->width - 2, state->height - 2}
            };
            for (int s = 0; s < 8; s++) {
                if (x == spawn_positions[s][0] && y == spawn_positions[s][1]) {
                    is_spawn = 1;
                    break;
                }
            }
            if (is_spawn) continue;
            if (rng_range(rng, 0, 100) < cfg->crate_density) {
                state->tiles[y][x] = TILE_CRATE;
            }
        }
    }

    /* Initialize agents */
    state->agent_count = cfg->agent_count;
    int spawn_x[4] = {1, state->width - 2, 1, state->width - 2};
    int spawn_y[4] = {1, 1, state->height - 2, state->height - 2};
    for (int i = 0; i < cfg->agent_count && i < MAX_AGENTS; i++) {
        state->agents[i].x = spawn_x[i % 4];
        state->agents[i].y = spawn_y[i % 4];
        state->agents[i].alive = 1;
        state->agents[i].bomb_ammo = 1;
        state->agents[i].bombs_active = 0;
        state->agents[i].blast_range = cfg->blast_range;
        state->agents[i].speed = 1;
        state->agents[i].score = 0;
    }

    /* Clear bombs */
    for (int i = 0; i < MAX_BOMBS; i++) {
        state->bombs[i].active = 0;
    }

    state->step = 0;
    state->total_reward = 0.0f;
}

int map_in_bounds(const BomberState* state, int x, int y) {
    return x >= 0 && x < state->width && y >= 0 && y < state->height;
}

int map_is_walkable(const BomberState* state, int x, int y) {
    if (!map_in_bounds(state, x, y)) return 0;
    TileType t = state->tiles[y][x];
    if (t == TILE_SOLID_WALL || t == TILE_CRATE) return 0;
    if (map_has_bomb(state, x, y)) return 0;
    return 1;
}

int map_has_bomb(const BomberState* state, int x, int y) {
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (state->bombs[i].active && state->bombs[i].x == x && state->bombs[i].y == y)
            return 1;
    }
    return 0;
}

BombState* map_bomb_at(BomberState* state, int x, int y) {
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (state->bombs[i].active && state->bombs[i].x == x && state->bombs[i].y == y)
            return &state->bombs[i];
    }
    return 0;
}

int map_count_crates(const BomberState* state) {
    int count = 0;
    for (int y = 0; y < state->height; y++) {
        for (int x = 0; x < state->width; x++) {
            if (state->tiles[y][x] == TILE_CRATE) count++;
        }
    }
    return count;
}
