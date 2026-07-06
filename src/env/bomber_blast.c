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

int explode_bomb(BomberState* state, int bomb_index) {
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

    /* Destroy crates and spawn powerups - use a local RNG derived from position+step */
    /* We need the env's RNG for determinism; pass via a temporary approach */
    /* For now, use a deterministic seed from bomb position and current step */
    RNG local_rng;
    rng_init(&local_rng, (uint64_t)(bomb->x * 1000 + bomb->y + state->step * 100000 + 1));
    destroy_crates(state, &blast, &local_rng, 0.3f);

    /* Apply blast damage to agents */
    apply_blast_damage(state, &blast);

    /* Trigger chain reactions */
    trigger_chain_reactions(state, &blast);

    return 1;
}

void apply_blast_damage(BomberState* state, const BlastResult* blast) {
    for (int i = 0; i < blast->count; i++) {
        int bx = blast->tiles[i].x;
        int by = blast->tiles[i].y;
        for (int a = 0; a < state->agent_count; a++) {
            if (state->agents[a].alive && state->agents[a].x == bx && state->agents[a].y == by) {
                state->agents[a].alive = 0;
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

void trigger_chain_reactions(BomberState* state, const BlastResult* blast) {
    for (int i = 0; i < blast->count; i++) {
        int bx = blast->tiles[i].x;
        int by = blast->tiles[i].y;
        for (int b = 0; b < MAX_BOMBS; b++) {
            if (state->bombs[b].active && state->bombs[b].x == bx && state->bombs[b].y == by) {
                explode_bomb(state, b);
            }
        }
    }
}
