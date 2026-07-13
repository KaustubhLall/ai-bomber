#include "env/bomber_blast.h"
#include "env/bomber_map.h"
#include "env/bomber_bombs.h"
#include "core/math_util.h"

void compute_blast_tiles(const BomberState* state, int bx, int by, int range, BlastResult* result) {
    result->count = 0;

    /* Center */
    result->tiles[result->count].x = bx;
    result->tiles[result->count].y = by;
    result->count++;

    int dx[4] = {0, 0, -1, 1};
    int dy[4] = {-1, 1, 0, 0};

    for (int d = 0; d < 4; d++) {
        for (int r = 1; r <= range; r++) {
            if (result->count >= MAX_BLAST_TILES) return; /* guard the fixed buffer */
            int nx = bx + dx[d] * r;
            int ny = by + dy[d] * r;
            if (!map_in_bounds(state, nx, ny)) break;
            TileType t = state->tiles[ny][nx];
            if (t == TILE_SOLID_WALL) break;
            result->tiles[result->count].x = nx;
            result->tiles[result->count].y = ny;
            result->count++;
            if (t == TILE_CRATE) break; /* crate blocks further blast */
        }
    }
}

int explode_bomb(BomberState* state, int bomb_index, RNG* rng, float powerup_rate) {
    BombState* bomb = &state->bombs[bomb_index];
    if (!bomb->active) return 0;

    BlastResult blast;
    compute_blast_tiles(state, bomb->x, bomb->y, bomb->range, &blast);

    /* Return ammo to owner */
    int owner = bomb->owner_id;
    if (owner >= 0 && owner < state->agent_count) {
        state->agents[owner].bomb_ammo++;
        state->agents[owner].bombs_active--;
        if (state->agents[owner].bombs_active < 0) state->agents[owner].bombs_active = 0;
    }

    bomb->active = 0;

    if (owner >= 0 && owner < state->agent_count) {
        for (int i = 0; i < blast.count; i++)
            if (state->tiles[blast.tiles[i].y][blast.tiles[i].x] == TILE_CRATE)
                state->agents[owner].crates_destroyed++;
    }

    /* Destroy loose powerups already lying in the blast (before crate reveal, so a
       powerup revealed by THIS blast survives it) — flames consume items canonically. */
    for (int i = 0; i < blast.count; i++) {
        TileType t = state->tiles[blast.tiles[i].y][blast.tiles[i].x];
        if (t == TILE_POWERUP_BOMB || t == TILE_POWERUP_RANGE || t == TILE_POWERUP_SPEED)
            state->tiles[blast.tiles[i].y][blast.tiles[i].x] = TILE_FLOOR;
    }

    destroy_crates(state, &blast, rng, powerup_rate);

    /* Lay persistent flame; damage is applied by the per-tick flame pass so the fire
       has canonical area denial rather than a single-tick instantaneous hit. */
    ignite_flame(state, &blast, owner);

    trigger_chain_reactions(state, &blast, rng, powerup_rate);

    return 1;
}

void ignite_flame(BomberState* state, const BlastResult* blast, int owner_id) {
    int ttl = state->flame_duration > 0 ? state->flame_duration : 1;
    for (int i = 0; i < blast->count; i++) {
        int bx = blast->tiles[i].x;
        int by = blast->tiles[i].y;
        if (ttl > state->flame_ttl[by][bx]) state->flame_ttl[by][bx] = ttl; /* keep the max on overlap */
        state->flame_owner[by][bx] = owner_id;
    }
}

void apply_flame_damage(BomberState* state) {
    for (int a = 0; a < state->agent_count; a++) {
        if (!state->agents[a].alive) continue;
        int ax = state->agents[a].x;
        int ay = state->agents[a].y;
        if (state->flame_ttl[ay][ax] > 0) {
            int owner = state->flame_owner[ay][ax];
            state->agents[a].alive = 0;
            state->death_owner[a] = owner;
            if (owner >= 0 && owner < state->agent_count && owner != a)
                state->agents[owner].eliminations++;
        }
    }
}

void decay_flame(BomberState* state) {
    for (int y = 0; y < state->height; y++) {
        for (int x = 0; x < state->width; x++) {
            if (state->flame_ttl[y][x] > 0) {
                state->flame_ttl[y][x]--;
                if (state->flame_ttl[y][x] == 0) state->flame_owner[y][x] = -1;
            }
        }
    }
}

void apply_blast_damage(BomberState* state, const BlastResult* blast, int owner_id) {
    for (int i = 0; i < blast->count; i++) {
        int bx = blast->tiles[i].x;
        int by = blast->tiles[i].y;
        for (int a = 0; a < state->agent_count; a++) {
            if (state->agents[a].alive && state->agents[a].x == bx && state->agents[a].y == by) {
                state->agents[a].alive = 0;
                state->death_owner[a] = owner_id;
                if (owner_id >= 0 && owner_id < state->agent_count && owner_id != a)
                    state->agents[owner_id].eliminations++;
            }
        }
    }
}

void destroy_crates(BomberState* state, const BlastResult* blast, RNG* rng, float powerup_rate) {
    for (int i = 0; i < blast->count; i++) {
        int bx = blast->tiles[i].x;
        int by = blast->tiles[i].y;
        if (state->tiles[by][bx] == TILE_CRATE) {
            /* Spawn powerup based on RNG */
            if (rng_float(rng) < powerup_rate) {
                int ptype = rng_range(rng, 0, 3);
                state->tiles[by][bx] = (TileType)(TILE_POWERUP_BOMB + ptype);
            } else {
                state->tiles[by][bx] = TILE_FLOOR;
            }
        }
    }
}

void spawn_powerups(BomberState* state, const BlastResult* blast, RNG* rng, float powerup_rate) {
    /* Integrated into destroy_crates; this function is for external/manual spawning */
    (void)state; (void)blast; (void)rng; (void)powerup_rate;
}

void trigger_chain_reactions(BomberState* state, const BlastResult* blast, RNG* rng, float powerup_rate) {
    for (int i = 0; i < blast->count; i++) {
        int bx = blast->tiles[i].x;
        int by = blast->tiles[i].y;
        for (int b = 0; b < MAX_BOMBS; b++) {
            if (state->bombs[b].active && state->bombs[b].x == bx && state->bombs[b].y == by) {
                explode_bomb(state, b, rng, powerup_rate);
            }
        }
    }
}
