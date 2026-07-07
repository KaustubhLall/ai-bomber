#ifndef BOMBER_RUNNER_H
#define BOMBER_RUNNER_H

#include "env/env.h"
#include "agents/agent.h"
#include "core/metrics.h"
#include "core/replay.h"

typedef struct {
    BomberConfig config;
    AgentType agent_type;
    AgentType enemy_type;
    uint64_t seed;
    int episodes;
    int record_replay;
    Replay* replay;
} RunConfig;

void runner_run(const RunConfig* rc, Metrics* metrics);
void runner_run_single(BomberEnv* env, Agent* agent, uint64_t seed,
                       Metrics* metrics, Replay* replay, int record);

#endif /* BOMBER_RUNNER_H */
