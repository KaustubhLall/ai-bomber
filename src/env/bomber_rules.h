#ifndef BOMBER_RULES_H
#define BOMBER_RULES_H

#include "env/bomber_state.h"
#include "env/types.h"

int rules_try_move(BomberState* state, int agent_id, Action action);
int rules_try_place_bomb(BomberState* state, int agent_id, int bomb_timer);
void rules_pickup_powerup(BomberState* state, int agent_id);
TerminalReason rules_check_terminal(const BomberState* state, int agent_id, int max_steps);

#endif /* BOMBER_RULES_H */
