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

static int export_replay_trace(const Replay* replay, const char* path) {
    FILE* file = fopen(path, "w");
    if (!file) return 0;
    fprintf(file, "frame,step,blue_x,blue_y,red_x,red_y,blue_action,red_action,blue_alive,red_alive,blue_death_owner,red_death_owner,bombs\n");
    for (int i = 0; i < replay->frame_count; i++) {
        const ReplayFrame* frame = &replay->frames[i]; const BomberState* state = &frame->state;
        Action red = frame->joint_action_count > 1 ? frame->joint_actions[1] : ACTION_WAIT;
        fprintf(file, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,\"", i, frame->step,
                state->agents[0].x, state->agents[0].y, state->agents[1].x, state->agents[1].y,
                (int)frame->action, (int)red, state->agents[0].alive, state->agents[1].alive,
                state->death_owner[0], state->death_owner[1]);
        int first = 1;
        for (int b = 0; b < MAX_BOMBS; b++) if (state->bombs[b].active) {
            fprintf(file, "%s%d:%d:%d:%d", first ? "" : "|", state->bombs[b].owner_id,
                    state->bombs[b].x, state->bombs[b].y, state->bombs[b].timer);
            first = 0;
        }
        fprintf(file, "\"\n");
    }
    fclose(file);
    return 1;
}

/* Round-robin tournament: every ordered pair plays `episodes` games (so each agent plays
   both seats), tallied from the row agent's perspective. Prints a W-D-L matrix and a
   score-ranked standings table. Optional replay_prefix saves one game per matchup. This is
   the batch/bracket engine that the live multigrid arena view surfaces interactively. */
static void run_tournament(const char* list, BomberConfig cfg, int episodes,
                           uint64_t seed, const char* replay_prefix) {
    AgentType types[16];
    char names[16][24];
    int n = 0;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", list);
    for (char* tok = strtok(buf, ","); tok && n < 16; tok = strtok(NULL, ",")) {
        while (*tok == ' ') tok++;
        snprintf(names[n], sizeof(names[n]), "%s", tok);
        types[n] = agent_parse_type(tok);
        n++;
    }
    if (n < 2) { fprintf(stderr, "Tournament needs >= 2 agents.\n"); return; }

    /* A full round-robin (n*(n-1) matchups x episodes games, each up to max_steps) can run
       for minutes with zero output otherwise — runner_run() itself never prints — which reads
       as a hung console. Announce each matchup as it starts/finishes so the terminal visibly
       updates throughout, not just once at the end. */
    int total_matchups = n * (n - 1);
    int matchup_index = 0;
    printf("Tournament: %d agents, %d matchups, %d games/matchup (%d games total)\n",
           n, total_matchups, episodes, total_matchups * episodes);
    fflush(stdout);

    int wins[16][16] = {{0}}, draws[16][16] = {{0}}, losses[16][16] = {{0}};
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            ++matchup_index;
            printf("  [%d/%d] %s vs %s ...", matchup_index, total_matchups, names[i], names[j]);
            fflush(stdout);
            RunConfig rc;
            memset(&rc, 0, sizeof(rc));
            rc.config = cfg;
            rc.agent_type = types[i];
            rc.enemy_type = types[j];
            rc.seed = seed + (uint64_t)i * 100003ULL + (uint64_t)j * 10007ULL;
            rc.episodes = episodes;
            rc.record_replay = replay_prefix ? 1 : 0;
            if (rc.record_replay) rc.replay = (Replay*)calloc(1, sizeof(Replay));
            Metrics m;
            runner_run(&rc, &m);
            printf(" %dW-%dD-%dL\n", m.wins, episodes - m.wins - m.losses, m.losses);
            fflush(stdout);
            /* Partition cleanly so W+D+L == games: everything that is not a decisive win or
               loss for the row agent (true draws, timeouts, mutual sudden-death crushes)
               counts as a draw-equivalent. */
            int w = m.wins, l = m.losses;
            if (w < 0) w = 0; if (l < 0) l = 0;
            if (w + l > episodes) l = episodes - w;
            wins[i][j] = w; losses[i][j] = l; draws[i][j] = episodes - w - l;
            if (replay_prefix && rc.replay) {
                char path[512];
                snprintf(path, sizeof(path), "%s_%s_vs_%s.bin", replay_prefix, names[i], names[j]);
                replay_set_policies(rc.replay, names[i], names[j]);
                replay_save(rc.replay, path);
            }
            free(rc.replay);
        }
    }

    printf("\n=== Tournament (%d games/matchup, both seats via reversed pairs) ===\n", episodes);
    printf("%-12s", "row beats col");
    for (int j = 0; j < n; j++) printf("%12s", names[j]);
    printf("%12s\n", "SCORE");
    for (int i = 0; i < n; i++) {
        printf("%-12s", names[i]);
        int gi = 0; double si = 0.0;
        for (int j = 0; j < n; j++) {
            if (i == j) { printf("%12s", "-"); continue; }
            int g = wins[i][j] + draws[i][j] + losses[i][j];
            char cell[16];
            snprintf(cell, sizeof(cell), "%d-%d-%d", wins[i][j], draws[i][j], losses[i][j]);
            printf("%12s", cell);
            gi += g; si += wins[i][j] + 0.5 * draws[i][j];
        }
        printf("%11.1f%%\n", gi > 0 ? 100.0 * si / gi : 0.0);
    }
    printf("(cells are row-agent W-D-L vs column agent; SCORE = (W+0.5D)/games across all matchups)\n");
}

static const char* action_label(int a) {
    switch (a) {
        case ACTION_UP: return "UP"; case ACTION_DOWN: return "DOWN";
        case ACTION_LEFT: return "LEFT"; case ACTION_RIGHT: return "RIGHT";
        case ACTION_PLACE_BOMB: return "BOMB"; case ACTION_WAIT: return "WAIT";
        default: return "?";
    }
}

/* Load a v4 replay and print an HONEST behavioral summary: how much each agent idles,
   how many bombs it places, and — the key question — HOW the loser actually died
   (opponent's bomb = a real/causal kill; arena crush = won by sudden-death attrition;
   self-kill = opponent blundered) and whether that happened in normal play or only once
   sudden-death forced the issue. This is the "is it playing or cheesing" diagnostic. */
static int analyze_replay(const char* path) {
    Replay* replay = (Replay*)calloc(1, sizeof(Replay));
    if (!replay) { fprintf(stderr, "alloc failed\n"); return 1; }
    if (!replay_load(replay, path)) {
        fprintf(stderr, "Failed to load replay (expects current v4 format): %s\n", path);
        free(replay); return 1;
    }
    int n = replay->frame_count;
    const char* names[2] = { replay->agent_name, replay->opponent_name };
    int sd = replay->config.sudden_death_start;
    printf("=== Replay behavior analysis: %s ===\n", path);
    printf("blue=%s  red=%s  seed=%llu\n", names[0], names[1], (unsigned long long)replay->seed);
    printf("board %dx%d  max_steps=%d  sudden_death_start=%d  shrink=%d  flame_dur=%d  frames=%d\n\n",
           replay->config.width, replay->config.height, replay->config.max_steps,
           sd, replay->config.shrink_interval, replay->config.flame_duration, n);
    if (n <= 0) { free(replay); return 1; }

    for (int a = 0; a < 2; a++) {
        int hist[ACTION_COUNT] = {0};
        int waits = 0, longest_wait = 0, cur_wait = 0, bombs = 0, moved = 0;
        int prev_x = -999, prev_y = -999;
        for (int i = 0; i < n; i++) {
            const ReplayFrame* f = &replay->frames[i];
            int act = f->joint_action_count > a ? (int)f->joint_actions[a]
                        : (a == 0 ? (int)f->action : ACTION_WAIT);
            if (act >= 0 && act < ACTION_COUNT) hist[act]++;
            if (act == ACTION_WAIT) { waits++; cur_wait++; if (cur_wait > longest_wait) longest_wait = cur_wait; }
            else cur_wait = 0;
            if (act == ACTION_PLACE_BOMB) bombs++;
            int x = f->state.agents[a].x, y = f->state.agents[a].y;
            if (i > 0 && (x != prev_x || y != prev_y)) moved++;
            prev_x = x; prev_y = y;
        }
        printf("Agent %d (%s):\n  ", a, names[a]);
        for (int k = 0; k < ACTION_COUNT; k++)
            printf("%s=%d(%.0f%%) ", action_label(k), hist[k], 100.0 * hist[k] / n);
        printf("\n  WAIT=%.1f%%  longest idle streak=%d frames  bombs placed=%d  moved on %.1f%% of steps\n\n",
               100.0 * waits / n, longest_wait, bombs, 100.0 * moved / (n > 1 ? n - 1 : 1));
    }

    printf("Death timeline:\n");
    int prev_alive[2] = {1, 1}, deaths = 0;
    for (int i = 0; i < n; i++) {
        const BomberState* s = &replay->frames[i].state;
        for (int a = 0; a < 2; a++) {
            if (prev_alive[a] && !s->agents[a].alive) {
                int owner = s->death_owner[a];
                char how[96];
                if (owner == -1) snprintf(how, sizeof(how), "arena crush (sudden-death wall)");
                else if (owner == a) snprintf(how, sizeof(how), "SELF-KILL (own bomb)");
                else snprintf(how, sizeof(how), "killed by %s's bomb", names[owner == 0 ? 0 : 1]);
                const char* phase = (sd > 0 && s->step >= sd) ? "SUDDEN-DEATH phase" : "normal play";
                printf("  step %d: %s died -> %s  [%s]\n", s->step, names[a], how, phase);
                deaths++;
            }
            prev_alive[a] = s->agents[a].alive;
        }
    }
    if (!deaths) printf("  (no deaths recorded — timeout/other)\n");
    const BomberState* last = &replay->frames[n - 1].state;
    printf("\nFinal @ step %d: %s alive=%d, %s alive=%d\n",
           last->step, names[0], last->agents[0].alive, names[1], last->agents[1].alive);
    free(replay);
    return 0;
}

static void usage(const char* prog) {
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  --agent <type>       Agent: random, scripted, heuristic, greedy, evasive, alpha-beta, mcts\n");
    printf("  --episodes <n>       Number of episodes (default 100)\n");
    printf("  --seed <n>           Random seed (default 1337)\n");
    printf("  --mode <mode>        Game mode: survival, battle (default survival)\n");
    printf("  --enemy <type>       Opponent policy (same choices as --agent)\n");
    printf("  --export <file>      Export metrics to file\n");
    printf("  --replay <file>      Save replay to file\n");
    printf("  --trace <csv>        Export first episode frame/action trace\n");
    printf("  --sudden-death-start N  Step the arena starts closing inward (0=off; try 120)\n");
    printf("  --shrink-interval N     Steps between inward wall rings (default 4)\n");
    printf("  --flame-duration N      Ticks a blast tile stays lethal (default 2)\n");
    printf("  --compare            Compare all agents\n");
    printf("  --tournament <a,b,c> Round-robin bracket among agents (role-balanced), print standings\n");
    printf("  --tournament-replays <prefix>  Save one replay per matchup (<prefix>_a_vs_b.bin)\n");
    printf("  --help               Show this help\n");
    printf("\nGenerate an MCTS self-play replay that matches training dynamics:\n");
    printf("  %s --mode battle --agent mcts --enemy mcts --sudden-death-start 120 \\\n", prog);
    printf("     --episodes 1 --seed 1 --replay mcts_selfplay.bin\n");
}

int main(int argc, char** argv) {
    const char* agent_name = "random";
    const char* enemy_name = NULL;
    const char* export_file = NULL;
    const char* replay_file = NULL;
    const char* trace_file = NULL;
    int episodes = 100;
    uint64_t seed = 1337;
    int do_compare = 0;
    GameMode mode = MODE_SURVIVAL;
    int sudden_death_start = -1; /* -1 = leave config default */
    int shrink_interval = -1;
    int flame_duration = -1;
    const char* tournament_list = NULL;
    const char* tournament_replays = NULL;

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
        } else if (strcmp(argv[i], "--trace") == 0 && i + 1 < argc) {
            trace_file = argv[++i];
        } else if (strcmp(argv[i], "--analyze") == 0 && i + 1 < argc) {
            return analyze_replay(argv[++i]);
        } else if (strcmp(argv[i], "--sudden-death-start") == 0 && i + 1 < argc) {
            sudden_death_start = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--shrink-interval") == 0 && i + 1 < argc) {
            shrink_interval = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--flame-duration") == 0 && i + 1 < argc) {
            flame_duration = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--tournament") == 0 && i + 1 < argc) {
            tournament_list = argv[++i];
        } else if (strcmp(argv[i], "--tournament-replays") == 0 && i + 1 < argc) {
            tournament_replays = argv[++i];
        } else if (strcmp(argv[i], "--compare") == 0) {
            do_compare = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
    }


    BomberConfig cfg;
    if (mode == MODE_BATTLE) config_battle(&cfg);
    else config_survival(&cfg);
    cfg.seed = (int)seed;
    /* Optional overrides so MCTS/self-play replays can match the training dynamics
       (sudden-death + persistent flame), which config_battle leaves off by default. */
    if (sudden_death_start >= 0) cfg.sudden_death_start = sudden_death_start;
    if (shrink_interval >= 0) cfg.shrink_interval = shrink_interval;
    if (flame_duration >= 0) cfg.flame_duration = flame_duration;
    config_normalize(&cfg);

    if (do_compare) {
        AgentType types[] = {AGENT_RANDOM, AGENT_SCRIPTED, AGENT_HEURISTIC, AGENT_GREEDY_CRATE};
        evaluator_compare(types, 4, &cfg, episodes, seed);
        return 0;
    }

    if (tournament_list) {
        run_tournament(tournament_list, cfg, episodes, seed, tournament_replays);
        return 0;
    }

    RunConfig rc;
    rc.config = cfg;
    rc.agent_type = agent_parse_type(agent_name);
    rc.enemy_type = enemy_name ? agent_parse_type(enemy_name) : (AgentType)-1;
    rc.seed = seed;
    rc.episodes = episodes;
    rc.record_replay = (replay_file != NULL || trace_file != NULL) ? 1 : 0;
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
    printf("Opponent: %s%s\n",
           enemy_name ? enemy_name : "built-in-random",
           cfg.agent_count > 2 ? " (shared by enemies 1..N)" : "");
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
            fprintf(f, "  \"owned_eliminations\": %d,\n", metrics.enemies_killed);
            fprintf(f, "  \"self_kills\": %d,\n", metrics.self_kills);
            fprintf(f, "  \"opponent_self_kills\": %d,\n", metrics.opponent_self_kills);
            fprintf(f, "  \"opponent_kills\": %d,\n", metrics.opponent_kills);
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
    }
    if (trace_file && rc.replay) {
        if (export_replay_trace(rc.replay, trace_file)) printf("Replay trace saved to %s\n", trace_file);
        else fprintf(stderr, "Failed to save replay trace to %s\n", trace_file);
    }
    free(rc.replay);

    return 0;
}
