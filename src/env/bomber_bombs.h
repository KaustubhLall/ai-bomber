#ifndef BOMBER_BOMBS_H
#define BOMBER_BOMBS_H

#include "env/bomber_state.h"
#include "core/rng.h"

int place_bomb(BomberState* state, int agent_id);
void tick_bombs(BomberState* state, RNG* rng, float powerup_rate);
int explode_bomb(BomberState* state, int bomb_index, RNG* rng, float powerup_rate);

#endif /* BOMBER_BOMBS_H */
