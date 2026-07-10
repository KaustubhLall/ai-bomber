#include "env/bomber_map.h"
#include "core/math_util.h"

static void spawn_position(const BomberState* state, int agent_count, int index, int* x, int* y) {
    int max_x = state->width - 2, max_y = state->height - 2;
    int mid_x = state->width / 2, mid_y = state->height / 2;
    /* Two-player battle spawns in diagonally opposite corners (canonical maximum
       separation, and it denies the perfect-reflection mirror strategy that a pair of
       adjacent top corners otherwise makes a guaranteed draw). */
    if (agent_count == 2) {
        if (index % 2 == 0) { *x = 1; *y = 1; } else { *x = max_x; *y = max_y; }
        return;
    }
    static const int slot_x[] = {0, 1, 0, 1, 2, 2, 0, 1};
    static const int slot_y[] = {0, 0, 1, 1, 0, 1, 2, 2};
    int xs[] = {1, max_x, mid_x}; int ys[] = {1, max_y, mid_y};
    *x = xs[slot_x[index % MAX_AGENTS]]; *y = ys[slot_y[index % MAX_AGENTS]];
}

/* Clear each configured spawn and its cardinal exits. Corner spawns retain the
 * classic three-tile L; edge spawns for players 5-8 get equivalent exits. */
static int is_spawn_safe_tile(const BomberState* state, const BomberConfig* cfg, int x, int y) {
    int spawn_count = cfg->agent_count;
    for (int i = 0; i < spawn_count && i < MAX_AGENTS; i++) {
        int sx, sy; spawn_position(state, cfg->agent_count, i, &sx, &sy);
        int distance = (x > sx ? x - sx : sx - x) + (y > sy ? y - sy : sy - y);
        if (distance <= 1 && state->tiles[y][x] != TILE_SOLID_WALL) return 1;
    }
    return 0;
}

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
            if (is_spawn_safe_tile(state, cfg, x, y)) continue;
            if (rng_range(rng, 0, 100) < cfg->crate_density) {
                state->tiles[y][x] = TILE_CRATE;
            }
        }
    }

    /* Initialize agents */
    state->agent_count = cfg->agent_count;
    for (int i = 0; i < cfg->agent_count && i < MAX_AGENTS; i++) {
        spawn_position(state, cfg->agent_count, i, &state->agents[i].x, &state->agents[i].y);
        state->agents[i].alive = 1;
        state->agents[i].bomb_ammo = 1;
        state->agents[i].bombs_active = 0;
        state->agents[i].blast_range = cfg->blast_range;
        state->agents[i].speed = 1;
        state->agents[i].score = 0;
        state->agents[i].crates_destroyed = 0;
        state->agents[i].eliminations = 0;
        state->agents[i].powerups_collected = 0;
        state->death_owner[i] = -1;
    }

    /* Clear bombs */
    for (int i = 0; i < MAX_BOMBS; i++) {
        state->bombs[i].active = 0;
    }

    /* Clear persistent flame and copy the fire lifetime for the blast module. */
    for (int y = 0; y < MAX_HEIGHT; y++) {
        for (int x = 0; x < MAX_WIDTH; x++) {
            state->flame_ttl[y][x] = 0;
            state->flame_owner[y][x] = -1;
        }
    }
    state->flame_duration = cfg->flame_duration > 0 ? cfg->flame_duration : 1;

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

void map_apply_sudden_death(BomberState* state, int start, int interval) {
    if (start <= 0 || state->step < start) return;
    if (interval < 1) interval = 1;
    int rings = (state->step - start) / interval + 1;
    for (int y = 1; y < state->height - 1; y++) {
        for (int x = 1; x < state->width - 1; x++) {
            int bd = x;
            if (y < bd) bd = y;
            if (state->width - 1 - x < bd) bd = state->width - 1 - x;
            if (state->height - 1 - y < bd) bd = state->height - 1 - y;
            if (bd > rings) continue;
            if (state->tiles[y][x] == TILE_SOLID_WALL) continue;
            state->tiles[y][x] = TILE_SOLID_WALL;
            for (int a = 0; a < state->agent_count; a++) {
                if (state->agents[a].alive && state->agents[a].x == x && state->agents[a].y == y) {
                    state->agents[a].alive = 0;
                    state->death_owner[a] = -1; /* crushed by the closing arena */
                }
            }
            for (int b = 0; b < MAX_BOMBS; b++) {
                if (state->bombs[b].active && state->bombs[b].x == x && state->bombs[b].y == y) {
                    int owner = state->bombs[b].owner_id;
                    state->bombs[b].active = 0;
                    if (owner >= 0 && owner < state->agent_count) {
                        state->agents[owner].bomb_ammo++;
                        state->agents[owner].bombs_active--;
                        if (state->agents[owner].bombs_active < 0) state->agents[owner].bombs_active = 0;
                    }
                }
            }
            state->flame_ttl[y][x] = 0;
            state->flame_owner[y][x] = -1;
        }
    }
}
