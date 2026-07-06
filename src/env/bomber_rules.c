#include "env/bomber_rules.h"
#include "env/bomber_map.h"
#include "env/bomber_bombs.h"

int rules_try_move(BomberState* state, int agent_id, Action action) {
    BomberAgentState* agent = &state->agents[agent_id];
    if (!agent->alive) return 0;

    int dx = 0, dy = 0;
    switch (action) {
        case ACTION_UP:    dy = -1; break;
        case ACTION_DOWN:  dy = 1;  break;
        case ACTION_LEFT:  dx = -1; break;
        case ACTION_RIGHT: dx = 1;  break;
        default: return 0;
    }

    int nx = agent->x + dx;
    int ny = agent->y + dy;
    if (!map_is_walkable(state, nx, ny)) return 0;

    agent->x = nx;
    agent->y = ny;
    return 1;
}

int rules_try_place_bomb(BomberState* state, int agent_id, int bomb_timer) {
    if (!place_bomb(state, agent_id)) return 0;
    /* Set the timer on the just-placed bomb */
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (state->bombs[i].active && state->bombs[i].owner_id == agent_id &&
            state->bombs[i].timer == 0) {
            state->bombs[i].timer = bomb_timer;
            break;
        }
    }
    return 1;
}

void rules_pickup_powerup(BomberState* state, int agent_id) {
    BomberAgentState* agent = &state->agents[agent_id];
    if (!agent->alive) return;
    TileType tile = state->tiles[agent->y][agent->x];
    switch (tile) {
        case TILE_POWERUP_BOMB:
            agent->bomb_ammo++;
            state->tiles[agent->y][agent->x] = TILE_FLOOR;
            break;
        case TILE_POWERUP_RANGE:
            agent->blast_range++;
            state->tiles[agent->y][agent->x] = TILE_FLOOR;
            break;
        case TILE_POWERUP_SPEED:
            agent->speed++;
            state->tiles[agent->y][agent->x] = TILE_FLOOR;
            break;
        default:
            break;
    }
}

TerminalReason rules_check_terminal(const BomberState* state, int agent_id, int max_steps) {
    if (state->step >= max_steps) return TERMINAL_TIMEOUT;

    if (!state->agents[agent_id].alive) return TERMINAL_AGENT_DEAD;

    /* Check for win/loss in multi-agent */
    if (state->agent_count > 1) {
        int alive_enemies = 0;
        for (int a = 0; a < state->agent_count; a++) {
            if (a != agent_id && state->agents[a].alive) alive_enemies++;
        }
        if (alive_enemies == 0) return TERMINAL_WIN;
    }

    return TERMINAL_NONE;
}
