#include "core/config.h"

static int clamp_int(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static float clamp_float(float value, float lo, float hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

static int make_odd(int value) {
    return (value % 2 == 0) ? value - 1 : value;
}

void config_normalize(BomberConfig* cfg) {
    if (!cfg) return;

    cfg->width = make_odd(clamp_int(cfg->width, 5, MAX_WIDTH));
    cfg->height = make_odd(clamp_int(cfg->height, 5, MAX_HEIGHT));
    cfg->agent_count = clamp_int(cfg->agent_count, 1, MAX_AGENTS);
    cfg->bomb_timer = clamp_int(cfg->bomb_timer, 1, 1000);
    cfg->blast_range = clamp_int(cfg->blast_range, 1, MAX_WIDTH > MAX_HEIGHT ? MAX_WIDTH : MAX_HEIGHT);
    cfg->max_steps = clamp_int(cfg->max_steps, 1, MAX_REPLAY_STEPS);
    cfg->powerup_rate = clamp_float(cfg->powerup_rate, 0.0f, 1.0f);
    cfg->crate_density = clamp_int(cfg->crate_density, 0, 100);
}

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

    config_normalize(cfg);
}

void config_survival(BomberConfig* cfg) {
    config_defaults(cfg);
    cfg->mode = MODE_SURVIVAL;
    cfg->agent_count = 2;
    config_normalize(cfg);
}

void config_battle(BomberConfig* cfg) {
    config_defaults(cfg);
    cfg->mode = MODE_BATTLE;
    cfg->agent_count = 2;
    config_normalize(cfg);
}
