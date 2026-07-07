#include "sim/runner.h"
#include "env/bomber_map.h"
#include <string.h>
#include <time.h>

void runner_run_single(BomberEnv* env, Agent* agent, uint64_t seed,
                       Metrics* metrics, Replay* replay, int record) {
    BomberConfig cfg = env->config;
    env_init(env, &cfg);
    env_reset(env, seed);
    agent_reset(agent, seed);

    Observation obs;
    DebugSnapshot debug;

    int prev_crates = map_count_crates(&env->state);
    (void)prev_crates;

    for (int step = 0; step < env->config.max_steps; step++) {
        env_observe(env, 0, &obs);
        env_get_debug_snapshot(env, &debug);
        Action action = agent_act(agent, &obs, &debug);

        int prev_c = map_count_crates(&env->state);
        StepResult result = env_step(env, action);
        int cur_c = map_count_crates(&env->state);
        int crates_destroyed = prev_c - cur_c;

        /* Count powerups: check if agent picked up */
        int powerups = 0;
        /* Simple: if reward has powerup component */
        if (env->last_reward.powerup > 0) powerups = 1;

        metrics_update(metrics, action, result, crates_destroyed, powerups);

        if (record && replay) {
            replay_record(replay, action, &env->state, result.reward, result.terminal_reason);
        }

        if (result.done) break;
    }
}

void runner_run(const RunConfig* rc, Metrics* metrics) {
    BomberEnv env;
    env.config = rc->config;
    Agent agent;
    agent_init(&agent, rc->agent_type);

    Agent enemy;
    Agent* enemy_ptr = NULL;
    if ((int)rc->enemy_type >= 0) {
        agent_init(&enemy, rc->enemy_type);
        enemy_ptr = &enemy;
    }

    metrics_init(metrics);

    clock_t start = clock();

    for (int ep = 0; ep < rc->episodes; ep++) {
        uint64_t ep_seed = rc->seed + (uint64_t)ep;
        Replay* replay_ptr = (rc->record_replay && rc->replay) ? rc->replay : NULL;

        if (replay_ptr && ep == 0) {
            replay_init(replay_ptr, &rc->config, ep_seed);
        }

        env_set_opponent(&env, enemy_ptr);
        runner_run_single(&env, &agent, ep_seed, metrics, replay_ptr,
                         replay_ptr && ep == 0);
    }

    metrics->total_time_ms = (double)(clock() - start) * 1000.0 / CLOCKS_PER_SEC;
}
