#ifndef BOMBER_EVALUATOR_H
#define BOMBER_EVALUATOR_H

#include "agents/agent.h"
#include "core/config.h"

typedef struct {
    int episodes;
    float avg_reward;
    float win_rate;
    float death_rate;
    float avg_episode_length;
    float crate_destruction_rate;
    float powerup_pickup_rate;
} EvalResult;

EvalResult evaluator_run(AgentType agent_type, const BomberConfig* cfg,
                         int episodes, uint64_t seed);
void evaluator_compare(AgentType* types, int num_types, const BomberConfig* cfg,
                       int episodes, uint64_t seed);

#endif /* BOMBER_EVALUATOR_H */
