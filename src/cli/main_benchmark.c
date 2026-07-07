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
    printf("  --matrix         Run the policy-vs-policy benchmark matrix\n");
    printf("  --output <path>  Export CSV/JSON (default benchmark.csv)\n");
    printf("  --suite <name>   Seed suite label: train/validation/holdout\n");
}

int main(int argc, char** argv) {
    int episodes = 10000;
    uint64_t seed = 1;
    int matrix = 0; const char* output="benchmark.csv"; const char* suite="holdout";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--episodes") == 0 && i + 1 < argc) {
            episodes = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (uint64_t)strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else if(strcmp(argv[i],"--matrix")==0) matrix=1;
        else if(strcmp(argv[i],"--output")==0&&i+1<argc) output=argv[++i];
        else if(strcmp(argv[i],"--suite")==0&&i+1<argc) suite=argv[++i];
    }

    BomberConfig cfg;
    config_battle(&cfg); cfg.agent_count=2;

    Metrics metrics;
    if(matrix){int json=strlen(output)>5&&strcmp(output+strlen(output)-5,".json")==0;if(!benchmark_matrix(&cfg,episodes,seed,suite,output,json)){fprintf(stderr,"failed to write %s\n",output);return 1;}}
    else benchmark_run(&cfg, episodes, seed, &metrics);

    return 0;
}
