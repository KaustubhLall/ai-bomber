#ifndef BOMBER_BENCHMARK_H
#define BOMBER_BENCHMARK_H

#include "core/metrics.h"
#include "core/config.h"

void benchmark_run(const BomberConfig* cfg, int episodes, uint64_t seed, Metrics* metrics);
int benchmark_matrix(const BomberConfig* cfg, int episodes, uint64_t seed,
                     const char* suite, const char* output_path, int json);

#endif /* BOMBER_BENCHMARK_H */
