#ifndef BOMBER_REWARD_H
#define BOMBER_REWARD_H

#include "env/bomber_state.h"
#include "env/types.h"
#include "env/bomber_danger.h"

/* Forward declaration to avoid circular include */
typedef struct BomberEnv BomberEnv;

typedef struct {
    float survival;
    float crate_destroyed;
    float powerup;
    float enemy_damage;
    float enemy_elimination;
    float win;
    float escape_danger;
    float trap_opportunity;
    float invalid_action_penalty;
    float suicidal_bomb_penalty;
    float stall_penalty;
    float death_penalty;
    float timeout_penalty;
    float total;
} RewardBreakdown;

float reward_compute(RewardBreakdown* rb, BomberEnv* env, Action action,
                     int agent_id, int prev_crates, int powerups_collected,
                     int prev_enemies_alive, int was_in_danger);

#endif /* BOMBER_REWARD_H */
