#include "raylib.h"
#include "env/env.h"
#include "env/bomber_observation.h"
#include "env/bomber_map.h"
#include "agents/agent.h"
#include "core/replay.h"
#include "viz/renderer.h"
#include "viz/dashboard.h"
#include "viz/ui_controls.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SCREEN_W 1280
#define SCREEN_H 800

int main(int argc, char** argv) {
    const char* agent_name = "heuristic";
    uint64_t seed = 1337;
    const char* replay_file = NULL;
    int live_mode = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--agent") == 0 && i + 1 < argc) {
            agent_name = argv[++i];
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (uint64_t)strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--replay") == 0 && i + 1 < argc) {
            replay_file = argv[++i];
            live_mode = 0;
        } else if (strcmp(argv[i], "--live") == 0) {
            live_mode = 1;
        }
    }

    InitWindow(SCREEN_W, SCREEN_H, "AI Bomber - Visualizer");
    SetTargetFPS(60);
    renderer_init(SCREEN_W, SCREEN_H);

    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = (int)seed;

    BomberEnv env;
    Agent agent;

    Replay* replay = (Replay*)calloc(1, sizeof(Replay));
    int replay_step = 0;
    int replay_loaded = 0;

    if (replay_file) {
        if (replay_load(replay, replay_file)) {
            replay_loaded = 1;
            cfg = replay->config;
            env_init(&env, &cfg);
            env_reset(&env, replay->seed);
            replay_step = 0;
        } else {
            fprintf(stderr, "Failed to load replay: %s\n", replay_file);
            live_mode = 1;
        }
    }

    if (live_mode || !replay_loaded) {
        agent_init(&agent, agent_parse_type(agent_name));
        env_init(&env, &cfg);
        env_reset(&env, seed);
        agent_reset(&agent, seed);
    }

    DashboardState ds;
    dashboard_init(&ds);

    int paused = 0;
    int speed_mult = 1;
    int step_once = 0;
    int episode = 0;

    Observation obs;
    DebugSnapshot snap;

    while (!WindowShouldClose()) {
        /* Input handling */
        if (IsKeyPressed(KEY_SPACE)) paused = !paused;
        if (IsKeyPressed(KEY_R)) {
            env_reset(&env, seed);
            dashboard_init(&ds);
            if (live_mode) agent_reset(&agent, seed);
            episode++;
        }
        if (IsKeyPressed(KEY_S) && paused) step_once = 1;
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) speed_mult = mini(speed_mult * 2, 16);
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) speed_mult = maxi(speed_mult / 2, 1);

        /* Simulation step */
        if (!paused || step_once) {
            step_once = 0;
            int steps_this_frame = speed_mult;

            for (int s = 0; s < steps_this_frame; s++) {
                if (replay_loaded && !live_mode) {
                    if (replay_step < replay->action_count) {
                        env_step(&env, replay->actions[replay_step]);
                        replay_step++;
                    }
                } else if (live_mode) {
                    env_observe(&env, 0, &obs);
                    env_get_debug_snapshot(&env, &snap);
                    Action action = agent_act(&agent, &obs, &snap);
                    StepResult result = env_step(&env, action);
                    dashboard_add_action(&ds, action);
                    dashboard_add_reward(&ds, result.reward);

                    if (result.done) {
                        episode++;
                        env_reset(&env, seed + (uint64_t)episode);
                        agent_reset(&agent, seed + (uint64_t)episode);
                    }
                }
            }
        }

        /* Get current state for rendering */
        env_observe(&env, 0, &obs);
        env_get_debug_snapshot(&env, &snap);

        /* Render */
        BeginDrawing();
        ClearBackground((Color){15, 15, 20, 255});

        dashboard_draw(&ds, &snap, &obs, SCREEN_W, SCREEN_H, paused, speed_mult);

        /* Title */
        DrawText("AI Bomber - Training Sandbox", 8, 0, 16, (Color){200, 200, 220, 255});

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
