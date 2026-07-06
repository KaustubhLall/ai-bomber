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

#endif /* BOMBER_MAP_H */
