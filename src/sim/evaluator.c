#include "sim/evaluator.h"
#include "sim/runner.h"
#include "core/metrics.h"
#include <stdio.h>

float evaluator_score_state(const BomberEnv* env, int agent_id) {
    if (!env || agent_id < 0 || agent_id >= env->state.agent_count) return -100000.0f;
    const BomberAgentState* me = &env->state.agents[agent_id];
    if (!me->alive) return -10000.0f;
    int enemies = 0, mobility = 0, nearby_crates = 0;
    for (int a = 0; a < env->state.agent_count; a++) if (a != agent_id && env->state.agents[a].alive) enemies++;
    if (env->state.agent_count > 1 && enemies == 0) return 10000.0f;
    Action legal[ACTION_COUNT]; env_legal_actions(env, agent_id, legal, &mobility);
    for (int y = me->y - 2; y <= me->y + 2; y++) for (int x = me->x - 2; x <= me->x + 2; x++)
        if (x >= 0 && y >= 0 && x < env->state.width && y < env->state.height && env->state.tiles[y][x] == TILE_CRATE) nearby_crates++;
    int danger = env->danger.time_to_blast[me->y][me->x];
    float score = 100.0f - enemies * 60.0f + me->bomb_ammo * 8.0f + me->blast_range * 4.0f;
    score += mobility * 3.0f - nearby_crates * 0.5f - env->state.step * 0.02f;
    if (danger >= 0) score -= 120.0f / (float)(danger + 1);
    return score;
}

EvalResult evaluator_run(AgentType agent_type, const BomberConfig* cfg,
                         int episodes, uint64_t seed) {
    RunConfig rc;
    rc.config = *cfg;
    rc.agent_type = agent_type;
    rc.enemy_type = (AgentType)-1;
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
        printf("%-15s %10.4f %10.2f %10.2f %10.1f %10.2f %10.2f\n",
               agent_type_name(types[i]), r.avg_reward, r.win_rate, r.death_rate,
               r.avg_episode_length, r.crate_destruction_rate, r.powerup_pickup_rate);
    }
}
