#include "env/env.h"
#include "env/bomber_reward.h"
#include "env/bomber_map.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>

static void assert_breakdown_sums(const RewardBreakdown* rb) {
    float sum = rb->survival + rb->crate_destroyed + rb->powerup +
                rb->enemy_damage + rb->enemy_elimination + rb->win +
                rb->escape_danger + rb->trap_opportunity +
                rb->invalid_action_penalty + rb->suicidal_bomb_penalty +
                rb->stall_penalty + rb->death_penalty + rb->timeout_penalty;
    assert(fabsf(sum - rb->total) < 0.001f);
}

int main(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 1;

    BomberEnv env;
    env_init(&env, &cfg);

    /* Survival reward on wait */
    int prev_crates = map_count_crates(&env.state);
    int prev_enemies = 0;
    for (int a = 1; a < env.state.agent_count; a++) {
        if (env.state.agents[a].alive) prev_enemies++;
    }

    float reward = reward_compute(&env.last_reward, &env, ACTION_WAIT, 0,
                                  prev_crates, 0, prev_enemies, 0);
    (void)reward;

    /* Should get survival reward */
    assert(env.last_reward.survival > 0.0f);
    assert_breakdown_sums(&env.last_reward);

    /* Powerup pickups must be rewarded even though env_step clears the tile immediately. */
    env_reset(&env, 123);
    int start_x = env.state.agents[0].x;
    int start_y = env.state.agents[0].y;
    env.state.tiles[start_y][start_x + 1] = TILE_POWERUP_BOMB;
    int ammo_before = env.state.agents[0].bomb_ammo;

    StepResult step = env_step(&env, ACTION_RIGHT);
    (void)step;
    assert(env.state.agents[0].x == start_x + 1);
    assert(env.state.agents[0].bomb_ammo == ammo_before + 1);
    assert(env.state.tiles[start_y][start_x + 1] == TILE_FLOOR);
    assert(fabsf(env.last_reward.powerup - cfg.powerup_reward) < 0.001f);
    assert_breakdown_sums(&env.last_reward);

    /* Invalid action penalty */
    env.state.agents[0].x = 1;
    env.state.agents[0].y = 1;
    float r2 = reward_compute(&env.last_reward, &env, ACTION_UP, 0,
                              prev_crates, 0, prev_enemies, 0);
    /* Moving up from (1,1) hits wall -> invalid, but reward_compute doesn't check
       validity directly. The env_step does. Here we just verify components sum. */
    (void)r2;
    assert_breakdown_sums(&env.last_reward);

    printf("test_reward: ALL PASSED\n");
    return 0;
}
