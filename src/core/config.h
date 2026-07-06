#ifndef BOMBER_CONFIG_H
#define BOMBER_CONFIG_H

#include <stdint.h>

#define MAX_WIDTH 31
#define MAX_HEIGHT 31
#define MAX_AGENTS 8
#define MAX_BOMBS 64
#define MAX_REPLAY_STEPS 20000
#define LOCAL_OBS_SIZE 11
#define MAX_BLAST_TILES 128

typedef enum {
    MODE_SURVIVAL = 0,
    MODE_BATTLE
} GameMode;

typedef struct {
    int width;
    int height;
    GameMode mode;
    int agent_count;
    int bomb_timer;
    int blast_range;
    int max_steps;
    float powerup_rate;
    int crate_density;
    int seed;

    /* Reward weights */
    float survival_reward;
    float crate_destroy_reward;
    float powerup_reward;
    float enemy_damage_reward;
    float enemy_elimination_reward;
    float win_reward;
    float death_penalty;
    float timeout_penalty;
    float invalid_action_penalty;
    float suicidal_bomb_penalty;
    float stall_penalty;
    float escape_danger_reward;
    float trap_opportunity_reward;
} BomberConfig;

void config_defaults(BomberConfig* cfg);
void config_survival(BomberConfig* cfg);
void config_battle(BomberConfig* cfg);

#endif /* BOMBER_CONFIG_H */
