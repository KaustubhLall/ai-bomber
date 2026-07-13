#include "env/env.h"
#include "env/bomber_reward.h"
#include "env/bomber_map.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>

int main(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 1;

    BomberEnv env;
    env_init(&env, &cfg);

    int prev_crates = env.state.agents[0].crates_destroyed;
    int prev_enemies = 0;
    for (int a = 1; a < env.state.agent_count; a++) {
        if (env.state.agents[a].alive) prev_enemies++;
    }

    float reward = reward_compute(&env.last_reward, &env, ACTION_WAIT, 0,
                                  prev_crates, 0, prev_enemies, 0);
    (void)reward;

    assert(env.last_reward.survival > 0.0f);
    float sum = env.last_reward.survival + env.last_reward.crate_destroyed +
                env.last_reward.powerup + env.last_reward.enemy_damage +
                env.last_reward.enemy_elimination + env.last_reward.win +
                env.last_reward.escape_danger + env.last_reward.trap_opportunity +
                env.last_reward.invalid_action_penalty + env.last_reward.suicidal_bomb_penalty +
                env.last_reward.stall_penalty + env.last_reward.death_penalty +
                env.last_reward.timeout_penalty;

    assert(fabsf(sum - env.last_reward.total) < 0.001f);

    float powerup_reward = reward_compute(&env.last_reward, &env, ACTION_WAIT, 0,
                                          prev_crates, 1, prev_enemies, 0);
    (void)powerup_reward;
    assert(fabsf(env.last_reward.powerup - cfg.powerup_reward) < 0.001f);

    /* An opponent self-elimination is a win, but not our elimination credit. */
    env.state.agents[1].alive = 0;
    env.state.death_owner[1] = 1;
    (void)reward_compute(&env.last_reward, &env, ACTION_WAIT, 0,
                         prev_crates, 0, prev_enemies, 0);
    assert(env.last_reward.enemy_elimination == 0.0f);
    assert(env.last_reward.win == cfg.win_reward);
    env.state.death_owner[1] = 0;
    (void)reward_compute(&env.last_reward, &env, ACTION_WAIT, 0,
                         prev_crates, 0, prev_enemies, 0);
    assert(env.last_reward.enemy_elimination == cfg.enemy_elimination_reward);
    env.state.agents[1].alive = 1;
    env.state.death_owner[1] = -1;

    env.state.agents[0].x = 1;
    env.state.agents[0].y = 1;
    float r2 = reward_compute(&env.last_reward, &env, ACTION_UP, 0,
                              prev_crates, 0, prev_enemies, 0);
    (void)r2;
    float sum2 = env.last_reward.survival + env.last_reward.crate_destroyed +
                 env.last_reward.powerup + env.last_reward.enemy_damage +
                 env.last_reward.enemy_elimination + env.last_reward.win +
                 env.last_reward.escape_danger + env.last_reward.trap_opportunity +
                 env.last_reward.invalid_action_penalty + env.last_reward.suicidal_bomb_penalty +
                 env.last_reward.stall_penalty + env.last_reward.death_penalty +
                 env.last_reward.timeout_penalty;
    assert(fabsf(sum2 - env.last_reward.total) < 0.001f);

    printf("test_reward: ALL PASSED\n");
    return 0;
}
