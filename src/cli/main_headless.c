#include "env/env.h"
#include "agents/agent.h"
#include "sim/runner.h"
#include "sim/evaluator.h"
#include "core/metrics.h"
#include "core/replay.h"
#include "env/bomber_map.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void usage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --agent <type>       Agent type: random, scripted, heuristic, greedy\n");
    printf("  --episodes <n>       Number of episodes (default 100)\n");
    printf("  --seed <n>           Random seed (default 1337)\n");
    printf("  --mode <mode>        Game mode: survival, battle (default survival)\n");
    printf("  --enemy <type>       Reserved for explicit opponent policy wiring\n");
    printf("  --export <file>      Export metrics to file\n");
    printf("  --replay <file>      Save replay to file\n");
    printf("  --compare            Compare all agents\n");
    printf("  --help               Show this help\n");
}

int main(int argc, char** argv) {
    const char* agent_name = "random";
    const char* enemy_name = NULL;
    const char* export_file = NULL;
    const char* replay_file = NULL;
    int episodes = 100;
    uint64_t seed = 1337;
    int do_compare = 0;
    GameMode mode = MODE_SURVIVAL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--agent") == 0 && i + 1 < argc) {
            agent_name = argv[++i];
        } else if (strcmp(argv[i], "--enemy") == 0 && i + 1 < argc) {
            enemy_name = argv[++i];
        } else if (strcmp(argv[i], "--episodes") == 0 && i + 1 < argc) {
            episodes = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (uint64_t)strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            const char* m = argv[++i];
            if (strcmp(m, "battle") == 0) mode = MODE_BATTLE;
            else mode = MODE_SURVIVAL;
        } else if (strcmp(argv[i], "--export") == 0 && i + 1 < argc) {
            export_file = argv[++i];
        } else if (strcmp(argv[i], "--replay") == 0 && i + 1 < argc) {
            replay_file = argv[++i];
        } else if (strcmp(argv[i], "--compare") == 0) {
            do_compare = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
    }

    if (enemy_name) {
        fprintf(stderr, "Warning: --enemy=%s is parsed but not yet wired into the runner; using the environment opponent behavior.\n",
                enemy_name);
    }

    BomberConfig cfg;
    if (mode == MODE_BATTLE) config_battle(&cfg);
    else config_survival(&cfg);
    cfg.seed = (int)seed;

    if (do_compare) {
        AgentType types[] = {AGENT_RANDOM, AGENT_SCRIPTED, AGENT_HEURISTIC, AGENT_GREEDY_CRATE};
        evaluator_compare(types, 4, &cfg, episodes, seed);
        return 0;
    }

    RunConfig rc;
    rc.config = cfg;
    rc.agent_type = agent_parse_type(agent_name);
    rc.seed = seed;
    rc.episodes = episodes;
    rc.record_replay = (replay_file != NULL) ? 1 : 0;
    rc.replay = NULL;

    if (rc.record_replay) {
        rc.replay = (Replay*)calloc(1, sizeof(Replay));
        if (!rc.replay) {
            fprintf(stderr, "Failed to allocate replay buffer.\n");
            return 1;
        }
    }

    Metrics metrics;
    runner_run(&rc, &metrics);

    printf("\n=== Headless Run ===\n");
    printf("Agent: %s | Episodes: %d | Seed: %llu | Mode: %s\n",
           agent_name, episodes, (unsigned long long)seed,
           mode == MODE_BATTLE ? "battle" : "survival");
    metrics_print(&metrics);

    if (export_file) {
        FILE* f = fopen(export_file, "w");
        if (f) {
            fprintf(f, "{\n");
            fprintf(f, "  \"episodes\": %d,\n", metrics.episodes);
            fprintf(f, "  \"total_steps\": %d,\n", metrics.total_steps);
            fprintf(f, "  \"total_reward\": %.4f,\n", metrics.total_reward);
            fprintf(f, "  \"wins\": %d,\n", metrics.wins);
            fprintf(f, "  \"losses\": %d,\n", metrics.losses);
            fprintf(f, "  \"draws\": %d,\n", metrics.draws);
            fprintf(f, "  \"timeouts\": %d,\n", metrics.timeouts);
            fprintf(f, "  \"deaths\": %d,\n", metrics.deaths);
            fprintf(f, "  \"crates_destroyed\": %d,\n", metrics.crates_destroyed);
            fprintf(f, "  \"powerups_collected\": %d,\n", metrics.powerups_collected);
            fprintf(f, "  \"steps_per_sec\": %.1f\n",
                    metrics.total_time_ms > 0 ? (double)metrics.total_steps / (metrics.total_time_ms / 1000.0) : 0.0);
            fprintf(f, "}\n");
            fclose(f);
            printf("Metrics exported to %s\n", export_file);
        } else {
            fprintf(stderr, "Failed to open metrics export file: %s\n", export_file);
        }
    }

    if (replay_file && rc.replay) {
        if (replay_save(rc.replay, replay_file)) {
            printf("Replay saved to %s\n", replay_file);
        } else {
            fprintf(stderr, "Failed to save replay to %s\n", replay_file);
        }
        free(rc.replay);
    }

    return 0;
}
