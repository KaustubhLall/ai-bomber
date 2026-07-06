#include "env/bomber_bombs.h"
#include "env/bomber_blast.h"
#include "env/bomber_map.h"

int place_bomb(BomberState* state, int agent_id) {
    BomberAgentState* agent = &state->agents[agent_id];
    if (!agent->alive) return 0;
    if (agent->bomb_ammo <= 0) return 0;
    if (map_has_bomb(state, agent->x, agent->y)) return 0;

    /* Find free bomb slot */
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!state->bombs[i].active) {
            state->bombs[i].x = agent->x;
            state->bombs[i].y = agent->y;
            state->bombs[i].owner_id = agent_id;
            state->bombs[i].timer = 0; /* will be set by caller via config */
            state->bombs[i].range = agent->blast_range;
            state->bombs[i].active = 1;
            agent->bomb_ammo--;
            agent->bombs_active++;
            return 1;
        }
    }
    return 0;
}

void tick_bombs(BomberState* state) {
    /* Decrement timers and explode any at zero */
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!state->bombs[i].active) continue;
        state->bombs[i].timer--;
        if (state->bombs[i].timer <= 0) {
            explode_bomb(state, i);
        }
    }
}
