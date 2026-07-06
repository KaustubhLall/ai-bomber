#include "raylib.h"
#include "env/env.h"
#include "env/bomber_observation.h"
#include "env/bomber_map.h"
#include "agents/agent.h"
#include "core/replay.h"
#include "viz/renderer.h"
#include "viz/dashboard.h"
#include "viz/ui_controls.h"
#include "viz/charts.h"
#include "viz/viz_session.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SCREEN_W 1400
#define SCREEN_H 860

static void draw_agent_tabs(VizSession* vs, int ox, int oy, int w) {
    int tab_w = w / vs->session_count;
    for (int i = 0; i < vs->session_count; i++) {
        int tx = ox + i * tab_w;
        Color bg = (i == vs->active_session) ? (Color){60, 60, 80, 255} : (Color){30, 30, 40, 255};
        DrawRectangle(tx, oy, tab_w, 24, bg);
        DrawRectangleLines(tx, oy, tab_w, 24, (Color){100, 100, 120, 255});
        const char* name = vs->sessions[i].name;
        int tw = MeasureText(name, 12);
        DrawText(name, tx + (tab_w - tw) / 2, oy + 6, 12,
                 (i == vs->active_session) ? WHITE : GRAY);
    }
}

static void draw_training_overview(VizSession* vs, int ox, int oy, int w, int h) {
    DrawRectangle(ox, oy, w, h, (Color){20, 20, 30, 255});
    DrawRectangleLines(ox, oy, w, h, (Color){100, 100, 120, 255});
    DrawText("Training Overview (All Agents)", ox + 8, oy + 4, 14, WHITE);

    int y = oy + 26;
    char buf[256];

    snprintf(buf, sizeof(buf), "%-15s %8s %8s %8s %8s %10s",
             "Agent", "Epochs", "Wins", "Deaths", "WinRate", "AvgReward");
    DrawText(buf, ox + 8, y, 12, (Color){180, 180, 200, 255}); y += 16;

    for (int i = 0; i < vs->session_count; i++) {
        AgentSession* s = &vs->sessions[i];
        float win_rate = s->epoch_count > 0 ? (float)s->total_wins / s->epoch_count : 0.0f;
        float avg_r = s->epoch_count > 0 ? s->total_reward / s->epoch_count : 0.0f;
        Color c = (i == vs->active_session) ? WHITE : (Color){160, 160, 170, 255};
        snprintf(buf, sizeof(buf), "%-15s %8d %8d %8d %7.1f%% %10.3f",
                 s->name, s->epoch_count, s->total_wins, s->total_deaths,
                 win_rate * 100.0f, avg_r);
        DrawText(buf, ox + 8, y, 12, c); y += 16;
    }

    int graph_oy = y + 8;
    int graph_h = h - (graph_oy - oy) - 8;
    if (graph_h > 40 && vs->session_count > 0) {
        DrawText("Epoch Rewards", ox + 8, graph_oy, 12, (Color){180, 180, 200, 255});
        graph_oy += 16;
        graph_h -= 16;

        int max_pts = 0;
        for (int i = 0; i < vs->session_count; i++) {
            if (vs->sessions[i].epoch_count > max_pts) max_pts = vs->sessions[i].epoch_count;
        }
        if (max_pts > 1) {
            float min_v = 1e30f, max_v = -1e30f;
            for (int i = 0; i < vs->session_count; i++) {
                for (int j = 0; j < vs->sessions[i].epoch_count; j++) {
                    float v = vs->sessions[i].epoch_rewards[j];
                    if (v < min_v) min_v = v;
                    if (v > max_v) max_v = v;
                }
            }
            if (max_v - min_v < 0.001f) { max_v = min_v + 1.0f; }
            float range = max_v - min_v;

            Color colors[] = {
                {100, 200, 255, 255}, {255, 100, 100, 255}, {100, 255, 100, 255},
                {255, 255, 100, 255}, {255, 100, 255, 255}, {100, 255, 255, 255},
                {255, 180, 100, 255}, {180, 100, 255, 255}
            };

            int graph_w = w - 16;
            for (int i = 0; i < vs->session_count; i++) {
                AgentSession* s = &vs->sessions[i];
                if (s->epoch_count < 2) continue;
                for (int j = 1; j < s->epoch_count; j++) {
                    int x0 = ox + 8 + (j - 1) * graph_w / (max_pts - 1);
                    int x1 = ox + 8 + j * graph_w / (max_pts - 1);
                    int y0 = graph_oy + graph_h - (int)((s->epoch_rewards[j-1] - min_v) / range * graph_h);
                    int y1 = graph_oy + graph_h - (int)((s->epoch_rewards[j] - min_v) / range * graph_h);
                    DrawLine(x0, y0, x1, y1, colors[i % 8]);
                }
            }
        }
    }
}

static void draw_comparison_view(VizSession* vs, int screen_w, int screen_h) {
    int overview_w = 400;
    draw_training_overview(vs, 8, 30, overview_w, screen_h - 40);

    int arena_area_w = screen_w - overview_w - 16;
    int cols = vs->session_count > 2 ? 3 : 2;
    if (cols < 1) cols = 1;
    int rows = (vs->session_count + cols - 1) / cols;
    if (rows < 1) rows = 1;
    int mini_w = arena_area_w / cols;
    int mini_h = (screen_h - 40) / rows;

    for (int i = 0; i < vs->session_count; i++) {
        int col = i % cols;
        int row = i / cols;
        int ax = overview_w + 16 + col * mini_w;
        int ay = 30 + row * mini_h;

        AgentSession* s = &vs->sessions[i];
        Observation obs;
        DebugSnapshot snap;
        env_observe(&s->env, 0, &obs);
        env_get_debug_snapshot(&s->env, &snap);

        int tile = 16;
        int arena_w = snap.state.width * tile;
        int arena_h = snap.state.height * tile;
        int aox = ax + (mini_w - arena_w) / 2;
        int aoy = ay + 20;

        DrawRectangle(ax, ay, mini_w, mini_h, (Color){15, 15, 20, 255});
        DrawRectangleLines(ax, ay, mini_w, mini_h, (Color){80, 80, 100, 255});
        DrawText(s->name, ax + 8, ay + 4, 12,
                 (i == vs->active_session) ? WHITE : (Color){160, 160, 170, 255});

        renderer_draw_arena(&snap, aox, aoy, tile);

        char info[128];
        snprintf(info, sizeof(info), "Ep:%d Step:%d R:%.2f %s",
                 s->epoch_count, s->current_step, s->current_reward,
                 s->env.state.agents[0].alive ? "ALIVE" : "DEAD");
        DrawText(info, ax + 4, ay + mini_h - 16, 10, (Color){180, 180, 200, 255});
    }
}

static void draw_arena_view(VizSession* vs, int screen_w, int screen_h) {
    AgentSession* s = viz_session_active(vs);
    if (!s) return;

    Observation obs;
    DebugSnapshot snap;
    env_observe(&s->env, 0, &obs);
    env_get_debug_snapshot(&s->env, &snap);

    int tile_size = TILE_SIZE;
    int arena_w = snap.state.width * tile_size;
    int arena_h = snap.state.height * tile_size;
    int arena_ox = 280 + (screen_w - 280 - 320 - arena_w) / 2;
    if (arena_ox < 280) arena_ox = 280;
    int arena_oy = 50;

    int left_w = 260;
    int left_ox = 8;

    renderer_draw_status_panel(&snap, left_ox, 50, left_w, 140);
    renderer_draw_local_obs(&obs, left_ox, 200, 16);
    renderer_draw_danger_map(&snap, left_ox, 430, left_w, 200);

    int right_w = 300;
    int right_ox = screen_w - right_w - 8;

    renderer_draw_reward_graph(s->dashboard.reward_history, s->dashboard.reward_count,
                               right_ox, 50, right_w, 120, 2.0f);
    renderer_draw_action_dist(s->dashboard.action_counts, s->dashboard.total_actions,
                              right_ox, 180, right_w, 130);
    renderer_draw_bomb_timeline(&snap, right_ox, 320, right_w);
    renderer_draw_decision_trace(snap.decision_text, right_ox, 450, right_w, 60);
    renderer_draw_event_log((const char**)s->dashboard.events, s->dashboard.event_count,
                            right_ox, 520, right_w, 120);
    renderer_draw_controls(right_ox, 650, right_w, 120, vs->paused, vs->speed_mult);

    renderer_draw_arena(&snap, arena_ox, arena_oy, tile_size);

    char epoch_info[128];
    snprintf(epoch_info, sizeof(epoch_info), "Agent: %s | Epoch: %d/%d | Step: %d | Reward: %.2f",
             s->name, s->epoch_count, vs->max_epochs, s->current_step, s->current_reward);
    DrawText(epoch_info, arena_ox, arena_oy + arena_h + 8, 14, (Color){200, 200, 220, 255});
}

static void draw_graphs_view(VizSession* vs, int screen_w, int screen_h) {
    AgentSession* s = viz_session_active(vs);
    if (!s) return;

    int panel_w = (screen_w - 32) / 2;
    int panel_h = (screen_h - 70) / 2;

    DrawRectangle(8, 30, panel_w, panel_h, (Color){20, 20, 30, 255});
    DrawRectangleLines(8, 30, panel_w, panel_h, (Color){100, 100, 120, 255});
    DrawText("Reward per Epoch", 16, 36, 14, WHITE);
    if (s->epoch_count > 1) {
        float min_v = 1e30f, max_v = -1e30f;
        for (int j = 0; j < s->epoch_count; j++) {
            if (s->epoch_rewards[j] < min_v) min_v = s->epoch_rewards[j];
            if (s->epoch_rewards[j] > max_v) max_v = s->epoch_rewards[j];
        }
        if (max_v - min_v < 0.001f) max_v = min_v + 1.0f;
        charts_draw_line(s->epoch_rewards, s->epoch_count, 16, 54, panel_w - 16, panel_h - 30,
                         min_v, max_v, (Color){100, 255, 100, 255});
    }

    DrawRectangle(16 + panel_w, 30, panel_w, panel_h, (Color){20, 20, 30, 255});
    DrawRectangleLines(16 + panel_w, 30, panel_w, panel_h, (Color){100, 100, 120, 255});
    DrawText("Running Average Reward", 24 + panel_w, 36, 14, WHITE);
    if (s->epoch_count > 1) {
        float min_v = 1e30f, max_v = -1e30f;
        for (int j = 0; j < s->epoch_count; j++) {
            if (s->epoch_avg_rewards[j] < min_v) min_v = s->epoch_avg_rewards[j];
            if (s->epoch_avg_rewards[j] > max_v) max_v = s->epoch_avg_rewards[j];
        }
        if (max_v - min_v < 0.001f) max_v = min_v + 1.0f;
        charts_draw_line(s->epoch_avg_rewards, s->epoch_count, 24 + panel_w, 54,
                         panel_w - 16, panel_h - 30,
                         min_v, max_v, (Color){100, 200, 255, 255});
    }

    renderer_draw_action_dist(s->dashboard.action_counts, s->dashboard.total_actions,
                              8, 40 + panel_h, panel_w, panel_h);

    draw_training_overview(vs, 16 + panel_w, 40 + panel_h, panel_w, panel_h);
}

int main(int argc, char** argv) {
    const char* agent_names[8];
    int agent_count = 0;
    uint64_t seed = 1337;
    int max_epochs = 500;
    const char* replay_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--agent") == 0 && i + 1 < argc) {
            if (agent_count < 8) agent_names[agent_count++] = argv[++i];
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (uint64_t)strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--epochs") == 0 && i + 1 < argc) {
            max_epochs = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--replay") == 0 && i + 1 < argc) {
            replay_file = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: bomber_viz [options]\n");
            printf("Options:\n");
            printf("  --agent <type>   Agent type (repeat for multiple): random, scripted, heuristic, greedy\n");
            printf("  --seed <n>       Random seed (default 1337)\n");
            printf("  --epochs <n>     Max epochs to run (default 500)\n");
            printf("  --replay <file>  Load replay file instead of live mode\n");
            printf("  --help           Show this help\n");
            printf("\nControls:\n");
            printf("  [SPACE]   Pause/Resume\n");
            printf("  [R]       Reset all sessions\n");
            printf("  [S]       Step once (when paused)\n");
            printf("  [+/-]     Speed up/down\n");
            printf("  [TAB]     Switch active agent\n");
            printf("  [1/2/3]   Switch view: Arena / Graphs / Comparison\n");
            printf("  [N]       New epoch for active agent\n");
            printf("  [ESC]     Quit\n");
            return 0;
        }
    }

    if (agent_count == 0) {
        agent_names[agent_count++] = "random";
        agent_names[agent_count++] = "heuristic";
        agent_names[agent_count++] = "greedy";
    }

    InitWindow(SCREEN_W, SCREEN_H, "AI Bomber - Visualizer");
    SetTargetFPS(60);
    renderer_init(SCREEN_W, SCREEN_H);

    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = (int)seed;

    VizSession vs;
    viz_session_init(&vs, max_epochs, seed);

    if (replay_file) {
        Replay* replay = (Replay*)calloc(1, sizeof(Replay));
        if (replay_load(replay, replay_file)) {
            cfg = replay->config;
            viz_session_add_agent(&vs, AGENT_RANDOM, "replay", &cfg);
            AgentSession* s = &vs.sessions[0];
            for (int i = 0; i < replay->action_count; i++) {
                env_step(&s->env, replay->actions[i]);
            }
        } else {
            fprintf(stderr, "Failed to load replay: %s\n", replay_file);
        }
        free(replay);
    } else {
        for (int i = 0; i < agent_count; i++) {
            AgentType type = agent_parse_type(agent_names[i]);
            viz_session_add_agent(&vs, type, agent_names[i], &cfg);
        }
    }

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_SPACE)) vs.paused = !vs.paused;
        if (IsKeyPressed(KEY_R)) viz_session_reset_all(&vs);
        if (IsKeyPressed(KEY_S) && vs.paused) vs.step_once = 1;
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
            vs.speed_mult = (vs.speed_mult * 2 > 16) ? 16 : vs.speed_mult * 2;
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
            vs.speed_mult = (vs.speed_mult / 2 < 1) ? 1 : vs.speed_mult / 2;
        if (IsKeyPressed(KEY_TAB)) {
            vs.active_session = (vs.active_session + 1) % vs.session_count;
        }
        if (IsKeyPressed(KEY_ONE)) vs.view_mode = VIEW_ARENA;
        if (IsKeyPressed(KEY_TWO)) vs.view_mode = VIEW_GRAPHS;
        if (IsKeyPressed(KEY_THREE)) vs.view_mode = VIEW_COMPARISON;
        if (IsKeyPressed(KEY_N)) {
            AgentSession* s = viz_session_active(&vs);
            if (s && s->epoch_count < vs.max_epochs) {
                /* Record current epoch and start new one */
                s->episode_done = 1;
                /* Manually record then reset */
                /* record_epoch is static, so we use the step mechanism */
            }
        }

        if (!vs.paused || vs.step_once) {
            vs.step_once = 0;
            viz_session_step(&vs);
        }

        BeginDrawing();
        ClearBackground((Color){15, 15, 20, 255});

        DrawText("AI Bomber - Training Sandbox", 8, 2, 16, (Color){200, 200, 220, 255});

        draw_agent_tabs(&vs, 8, 22, SCREEN_W - 16);

        const char* view_names[] = {"Arena", "Graphs", "Comparison"};
        DrawText(view_names[vs.view_mode], SCREEN_W - 100, 4, 12, (Color){150, 150, 170, 255});

        switch (vs.view_mode) {
            case VIEW_ARENA:       draw_arena_view(&vs, SCREEN_W, SCREEN_H); break;
            case VIEW_GRAPHS:      draw_graphs_view(&vs, SCREEN_W, SCREEN_H); break;
            case VIEW_COMPARISON:  draw_comparison_view(&vs, SCREEN_W, SCREEN_H); break;
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
