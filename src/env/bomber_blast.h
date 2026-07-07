#ifndef BOMBER_BLAST_H
#define BOMBER_BLAST_H

#include "env/bomber_state.h"
#include "core/rng.h"

typedef struct {
    int x;
    int y;
} BlastTile;

typedef struct {
    BlastTile tiles[MAX_BLAST_TILES];
    int count;
} BlastResult;

void compute_blast_tiles(const BomberState* state, int bx, int by, int range, BlastResult* result);
int explode_bomb(BomberState* state, int bomb_index, RNG* rng, float powerup_rate);
void apply_blast_damage(BomberState* state, const BlastResult* blast);
void destroy_crates(BomberState* state, const BlastResult* blast, RNG* rng, float powerup_rate);
void spawn_powerups(BomberState* state, const BlastResult* blast, RNG* rng, float powerup_rate);
void trigger_chain_reactions(BomberState* state, const BlastResult* blast, RNG* rng, float powerup_rate);

#endif /* BOMBER_BLAST_H */
