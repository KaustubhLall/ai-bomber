#ifndef BOMBER_MAP_H
#define BOMBER_MAP_H

#include "core/config.h"
#include "core/rng.h"
#include "env/bomber_state.h"

void map_generate(BomberState* state, const BomberConfig* cfg, RNG* rng);
int map_in_bounds(const BomberState* state, int x, int y);
int map_is_walkable(const BomberState* state, int x, int y);
int map_has_bomb(const BomberState* state, int x, int y);
BombState* map_bomb_at(BomberState* state, int x, int y);
int map_count_crates(const BomberState* state);
/* Sudden death: once state->step >= start, convert inward wall rings (one ring per
   `interval` steps) to solid walls, crushing any agent on a newly-walled tile
   (death_owner = -1) and clearing any bomb there. Canonical arena-shrink that forces a
   decisive result and prevents infinite stalling. No-op when start <= 0. */
void map_apply_sudden_death(BomberState* state, int start, int interval);

#endif /* BOMBER_MAP_H */
