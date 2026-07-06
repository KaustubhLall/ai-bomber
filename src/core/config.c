#include "core/config.h"

void config_defaults(BomberConfig* cfg) {
    cfg->width = 13;
    cfg->height = 11;
    cfg->mode = MODE_SURVIVAL;
    cfg->agent_count = 2;
    cfg->bomb_timer = 4;
    cfg->blast_range = 2;
    cfg->max_steps = 500;
    cfg->powerup_rate = 0.3f;
    cfg->crate_density = 50;
    cfg->seed = 1337;

    cfg->survival_reward = 0.01f;
    cfg->crate_destroy_reward = 0.2f;
    cfg->powerup_reward = 0.3f;
    cfg->enemy_damage_reward = 0.5f;
    cfg->enemy_elimination_reward = 1.0f;
    cfg->win_reward = 2.0f;
    cfg->death_penalty = -1.0f;
    cfg->timeout_penalty = -0.1f;
    cfg->invalid_action_penalty = -0.05f;
    cfg->suicidal_bomb_penalty = -0.3f;
    cfg->stall_penalty = -0.01f;
    cfg->escape_danger_reward = 0.15f;
    cfg->trap_opportunity_reward = 0.25f;
}

void config_survival(BomberConfig* cfg) {
    config_defaults(cfg);
    cfg->mode = MODE_SURVIVAL;
    cfg->agent_count = 2;
}

void config_battle(BomberConfig* cfg) {
    config_defaults(cfg);
    cfg->mode = MODE_BATTLE;
    cfg->agent_count = 2;
}
