#include "sim/benchmark.h"
#include "sim/runner.h"
#include "agents/agent.h"
#include <stdio.h>

void benchmark_run(const BomberConfig* cfg, int episodes, uint64_t seed, Metrics* metrics) {
    RunConfig rc;
    rc.config = *cfg;
    rc.agent_type = AGENT_HEURISTIC;
    rc.seed = seed;
    rc.episodes = episodes;
    rc.record_replay = 0;

    runner_run(&rc, metrics);

    printf("\n=== Benchmark Results ===\n");
    printf("Episodes: %d\n", episodes);
    metrics_print(metrics);
}
