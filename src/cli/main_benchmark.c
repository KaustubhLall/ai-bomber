#include "sim/benchmark.h"
#include "core/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --episodes <n>   Number of episodes (default 10000)\n");
    printf("  --seed <n>       Random seed (default 1)\n");
    printf("  --help           Show this help\n");
}

int main(int argc, char** argv) {
    int episodes = 10000;
    uint64_t seed = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--episodes") == 0 && i + 1 < argc) {
            episodes = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (uint64_t)strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
    }

    BomberConfig cfg;
    config_survival(&cfg);

    Metrics metrics;
    benchmark_run(&cfg, episodes, seed, &metrics);

    return 0;
}
