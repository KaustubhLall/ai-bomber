#include "sim/evaluator.h"
#include "sim/runner.h"
#include "core/metrics.h"
#include <stdio.h>

EvalResult evaluator_run(AgentType agent_type, const BomberConfig* cfg,
                         int episodes, uint64_t seed) {
    RunConfig rc;
    rc.config = *cfg;
    rc.agent_type = agent_type;
    rc.enemy_type = -1;
    rc.seed = seed;
    rc.episodes = episodes;
    rc.record_replay = 0;

    Metrics metrics;
    runner_run(&rc, &metrics);

    EvalResult result;
    result.episodes = metrics.episodes;
    result.avg_reward = metrics.episodes > 0 ? metrics.total_reward / metrics.episodes : 0.0f;
    result.win_rate = metrics.episodes > 0 ? (float)metrics.wins / metrics.episodes : 0.0f;
    result.death_rate = metrics.episodes > 0 ? (float)metrics.deaths / metrics.episodes : 0.0f;
    result.avg_episode_length = metrics.episodes > 0 ? (float)metrics.total_steps / metrics.episodes : 0.0f;
    result.crate_destruction_rate = metrics.episodes > 0 ? (float)metrics.crates_destroyed / metrics.episodes : 0.0f;
    result.powerup_pickup_rate = metrics.episodes > 0 ? (float)metrics.powerups_collected / metrics.episodes : 0.0f;

    return result;
}

void evaluator_compare(AgentType* types, int num_types, const BomberConfig* cfg,
                       int episodes, uint64_t seed) {
    printf("=== Agent Comparison ===\n");
    printf("%-15s %10s %10s %10s %10s %10s %10s\n",
           "Agent", "AvgReward", "WinRate", "DeathRate", "AvgLen", "Crates", "Powerups");

    for (int i = 0; i < num_types; i++) {
        EvalResult r = evaluator_run(types[i], cfg, episodes, seed);
        const char* names[] = {"random", "scripted", "heuristic", "greedy_crate", "enemy_bot", "external"};
        printf("%-15s %10.4f %10.2f %10.2f %10.1f %10.2f %10.2f\n",
               names[types[i]], r.avg_reward, r.win_rate, r.death_rate,
               r.avg_episode_length, r.crate_destruction_rate, r.powerup_pickup_rate);
    }
}
