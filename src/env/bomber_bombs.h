#ifndef BOMBER_BOMBS_H
#define BOMBER_BOMBS_H

#include "env/bomber_state.h"

int place_bomb(BomberState* state, int agent_id);
void tick_bombs(BomberState* state);
int explode_bomb(BomberState* state, int bomb_index);

#endif /* BOMBER_BOMBS_H */
