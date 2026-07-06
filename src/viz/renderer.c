#include "viz/renderer.h"
#include "env/bomber_map.h"
#include "core/math_util.h"
#include <stdio.h>
#include <string.h>

static Font s_font;

void renderer_init(int screen_w, int screen_h) {
    (void)screen_w; (void)screen_h;
    s_font = GetFontDefault();
}

Color tile_color(TileType t) {
    switch (t) {
        case TILE_FLOOR:         return (Color){30, 30, 40, 255};
        case TILE_SOLID_WALL:    return (Color){80, 80, 90, 255};
        case TILE_CRATE:         return (Color){160, 100, 40, 255};
        case TILE_POWERUP_BOMB:  return (Color){255, 80, 80, 255};
        case TILE_POWERUP_RANGE: return (Color){80, 255, 80, 255};
        case TILE_POWERUP_SPEED: return (Color){80, 180, 255, 255};
        default:                 return (Color){0, 0, 0, 255};
    }
}

Color agent_color(int agent_id) {
    static Color colors[] = {
        {80, 180, 255, 255},
        {255, 100, 100, 255},
        {100, 255, 100, 255},
        {255, 255, 100, 255},
        {255, 100, 255, 255},
        {100, 255, 255, 255},
        {255, 180, 100, 255},
        {180, 100, 255, 255}
    };
    return colors[agent_id % 8];
}

void renderer_draw_arena(const DebugSnapshot* snap, int ox, int oy, int tile_size) {
    const BomberState* state = &snap->state;
    const DangerMap* dm = &snap->danger;

    /* Draw tiles */
    for (int y = 0; y < state->height; y++) {
        for (int x = 0; x < state->width; x++) {
            int px = ox + x * tile_size;
            int py = oy + y * tile_size;
            DrawRectangle(px, py, tile_size, tile_size, tile_color(state->tiles[y][x]));

            /* Draw grid lines */
            DrawRectangleLines(px, py, tile_size, tile_size, (Color){50, 50, 60, 128});

            /* Draw danger overlay */
            int tblast = dm->time_to_blast[y][x];
            if (tblast >= 0 && tblast <= 3) {
                int alpha = 200 - tblast * 50;
                Color dc = (tblast == 0) ? (Color){255, 50, 50, 200} : (Color){255, 100, 50, alpha};
                DrawRectangle(px, py, tile_size, tile_size, dc);
            }

            /* Draw safe tile overlay (subtle green) */
            if (dm->reachable_safe[y][x] && tblast < 0) {
                DrawRectangle(px + tile_size - 4, py + tile_size - 4, 4, 4,
                              (Color){50, 200, 50, 128});
            }
        }
    }

    /* Draw bombs */
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!state->bombs[i].active) continue;
        int px = ox + state->bombs[i].x * tile_size + tile_size / 2;
        int py = oy + state->bombs[i].y * tile_size + tile_size / 2;
        int radius = tile_size / 3;
        /* Pulsing effect based on timer */
        float pulse = 1.0f - (float)state->bombs[i].timer / 4.0f;
        radius = (int)(radius * (1.0f + pulse * 0.3f));
        DrawCircle(px, py, radius, (Color){40, 40, 40, 255});
        DrawCircle(px, py, radius - 2, (Color){200, 50, 50, 255});

        /* Timer label */
        char timer_str[8];
        snprintf(timer_str, sizeof(timer_str), "%d", state->bombs[i].timer);
        DrawText(timer_str, px - 4, py - 6, 12, WHITE);
    }

    /* Draw agents */
    for (int a = 0; a < state->agent_count; a++) {
        if (!state->agents[a].alive) continue;
        int px = ox + state->agents[a].x * tile_size + tile_size / 2;
        int py = oy + state->agents[a].y * tile_size + tile_size / 2;
        int radius = tile_size / 3;
        DrawCircle(px, py, radius, agent_color(a));
        DrawCircleLines(px, py, radius, BLACK);

        /* Agent label */
        char label[4];
        snprintf(label, sizeof(label), "%d", a);
        DrawText(label, px - 4, py - 6, 10, WHITE);
    }

    /* Draw local observation box around agent 0 */
    if (state->agent_count > 0 && state->agents[0].alive) {
        int ax = state->agents[0].x;
        int ay = state->agents[0].y;
        int half = LOCAL_OBS_HALF;
        int bx = ox + (ax - half) * tile_size;
        int by = oy + (ay - half) * tile_size;
        int bw = LOCAL_OBS_SIZE * tile_size;
        int bh = LOCAL_OBS_SIZE * tile_size;
        DrawRectangleLines(bx, by, bw, bh, (Color){255, 255, 100, 100});
    }
}

void renderer_draw_danger_map(const DebugSnapshot* snap, int ox, int oy, int panel_w, int panel_h) {
    const BomberState* state = &snap->state;
    const DangerMap* dm = &snap->danger;

    DrawRectangle(ox, oy, panel_w, panel_h, (Color){20, 20, 30, 255});
    DrawRectangleLines(ox, oy, panel_w, panel_h, (Color){100, 100, 120, 255});
    DrawText("Danger Map", ox + 8, oy + 4, 14, WHITE);

    int map_w = state->width * 8;
    int map_h = state->height * 8;
    int map_ox = ox + (panel_w - map_w) / 2;
    int map_oy = oy + 24;

    for (int y = 0; y < state->height; y++) {
        for (int x = 0; x < state->width; x++) {
            int px = map_ox + x * 8;
            int py = map_oy + y * 8;
            int tblast = dm->time_to_blast[y][x];
            if (tblast >= 0) {
                int intensity = 255 - tblast * 40;
                if (intensity < 60) intensity = 60;
                DrawRectangle(px, py, 8, 8, (Color){intensity, 50, 50, 255});
            } else if (dm->reachable_safe[y][x]) {
                DrawRectangle(px, py, 8, 8, (Color){40, 120, 40, 255});
            } else {
                TileType t = state->tiles[y][x];
                if (t == TILE_SOLID_WALL) {
                    DrawRectangle(px, py, 8, 8, (Color){60, 60, 70, 255});
                } else if (t == TILE_CRATE) {
                    DrawRectangle(px, py, 8, 8, (Color){100, 70, 30, 255});
                } else {
                    DrawRectangle(px, py, 8, 8, (Color){25, 25, 35, 255});
                }
            }
        }
    }

    /* Draw agent position */
    if (state->agent_count > 0 && state->agents[0].alive) {
        int px = map_ox + state->agents[0].x * 8;
        int py = map_oy + state->agents[0].y * 8;
        DrawRectangle(px, py, 8, 8, (Color){80, 180, 255, 255});
    }
}

void renderer_draw_local_obs(const Observation* obs, int ox, int oy, int cell_size) {
    DrawRectangle(ox, oy, LOCAL_OBS_SIZE * cell_size + 16, LOCAL_OBS_SIZE * cell_size + 24,
                  (Color){20, 20, 30, 255});
    DrawRectangleLines(ox, oy, LOCAL_OBS_SIZE * cell_size + 16, LOCAL_OBS_SIZE * cell_size + 24,
                       (Color){100, 100, 120, 255});
    DrawText("Local Observation", ox + 8, oy + 4, 14, WHITE);

    int grid_ox = ox + 8;
    int grid_oy = oy + 22;

    for (int y = 0; y < LOCAL_OBS_SIZE; y++) {
        for (int x = 0; x < LOCAL_OBS_SIZE; x++) {
            int px = grid_ox + x * cell_size;
            int py = grid_oy + y * cell_size;
            TileType t = (TileType)obs->local_tiles[y][x];
            DrawRectangle(px, py, cell_size, cell_size, tile_color(t));
            DrawRectangleLines(px, py, cell_size, cell_size, (Color){50, 50, 60, 128});

            if (obs->local_bombs[y][x]) {
                DrawCircle(px + cell_size/2, py + cell_size/2, cell_size/3,
                           (Color){200, 50, 50, 255});
            }

            int danger = obs->local_danger[y][x];
            if (danger >= 0) {
                int alpha = 200 - danger * 50;
                DrawRectangle(px, py, cell_size, cell_size, (Color){255, 100, 50, alpha});
            }

            if (x == LOCAL_OBS_HALF && y == LOCAL_OBS_HALF) {
                DrawCircle(px + cell_size/2, py + cell_size/2, cell_size/3,
                           (Color){80, 180, 255, 255});
            }
        }
    }
}

void renderer_draw_bomb_timeline(const DebugSnapshot* snap, int ox, int oy, int w) {
    const BomberState* state = &snap->state;
    DrawRectangle(ox, oy, w, 120, (Color){20, 20, 30, 255});
    DrawRectangleLines(ox, oy, w, 120, (Color){100, 100, 120, 255});
    DrawText("Bomb Timeline", ox + 8, oy + 4, 14, WHITE);

    int y = oy + 24;
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!state->bombs[i].active) continue;
        char buf[128];
        snprintf(buf, sizeof(buf), "Bomb[%d] owner=%d (%d,%d) timer=%d range=%d",
                 i, state->bombs[i].owner_id, state->bombs[i].x, state->bombs[i].y,
                 state->bombs[i].timer, state->bombs[i].range);
        DrawText(buf, ox + 8, y, 12, WHITE);
        y += 16;
        if (y > oy + 110) break;
    }
    if (y == oy + 24) {
        DrawText("No active bombs", ox + 8, y, 12, GRAY);
    }
}

void renderer_draw_reward_graph(const float* rewards, int count, int ox, int oy, int w, int h, float max_reward) {
    DrawRectangle(ox, oy, w, h, (Color){20, 20, 30, 255});
    DrawRectangleLines(ox, oy, w, h, (Color){100, 100, 120, 255});
    DrawText("Reward Graph", ox + 8, oy + 4, 14, WHITE);

    if (count < 2 || max_reward == 0) {
        DrawText("No data", ox + w/2 - 30, oy + h/2, 14, GRAY);
        return;
    }

    int graph_ox = ox + 8;
    int graph_oy = oy + 24;
    int graph_w = w - 16;
    int graph_h = h - 32;

    /* Draw zero line */
    int zero_y = graph_oy + graph_h / 2;
    DrawLine(graph_ox, zero_y, graph_ox + graph_w, zero_y, (Color){80, 80, 80, 255});

    /* Draw reward line */
    for (int i = 1; i < count; i++) {
        int x0 = graph_ox + (i - 1) * graph_w / (count - 1);
        int x1 = graph_ox + i * graph_w / (count - 1);
        int y0 = zero_y - (int)(rewards[i - 1] * graph_h / (2.0f * max_reward));
        int y1 = zero_y - (int)(rewards[i] * graph_h / (2.0f * max_reward));
        DrawLine(x0, y0, x1, y1, (Color){100, 255, 100, 255});
    }
}

void renderer_draw_action_dist(const int* counts, int total, int ox, int oy, int w, int h) {
    DrawRectangle(ox, oy, w, h, (Color){20, 20, 30, 255});
    DrawRectangleLines(ox, oy, w, h, (Color){100, 100, 120, 255});
    DrawText("Action Distribution", ox + 8, oy + 4, 14, WHITE);

    const char* names[] = {"UP", "DOWN", "LEFT", "RIGHT", "BOMB", "WAIT"};
    Color colors[] = {
        {100, 200, 255, 255},
        {100, 255, 200, 255},
        {255, 200, 100, 255},
        {255, 100, 200, 255},
        {255, 100, 100, 255},
        {150, 150, 150, 255}
    };

    int bar_h = (h - 30) / 6;
    for (int i = 0; i < 6; i++) {
        int y = oy + 24 + i * bar_h;
        DrawText(names[i], ox + 8, y + 2, 12, WHITE);
        float frac = total > 0 ? (float)counts[i] / total : 0.0f;
        int bar_w = (int)(frac * (w - 80));
        DrawRectangle(ox + 50, y, bar_w, bar_h - 4, colors[i]);
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", counts[i]);
        DrawText(buf, ox + w - 30, y + 2, 12, WHITE);
    }
}

void renderer_draw_event_log(const char* events[], int event_count, int ox, int oy, int w, int h) {
    DrawRectangle(ox, oy, w, h, (Color){20, 20, 30, 255});
    DrawRectangleLines(ox, oy, w, h, (Color){100, 100, 120, 255});
    DrawText("Event Log", ox + 8, oy + 4, 14, WHITE);

    int y = oy + 24;
    int max_events = (h - 30) / 16;
    int start = event_count > max_events ? event_count - max_events : 0;
    for (int i = start; i < event_count; i++) {
        DrawText(events[i], ox + 8, y, 12, (Color){200, 200, 200, 255});
        y += 16;
        if (y > oy + h - 16) break;
    }
}

void renderer_draw_decision_trace(const char* text, int ox, int oy, int w, int h) {
    DrawRectangle(ox, oy, w, h, (Color){20, 20, 30, 255});
    DrawRectangleLines(ox, oy, w, h, (Color){100, 100, 120, 255});
    DrawText("Decision Trace", ox + 8, oy + 4, 14, WHITE);
    DrawText(text, ox + 8, oy + 24, 12, (Color){200, 255, 200, 255});
}

void renderer_draw_status_panel(const DebugSnapshot* snap, int ox, int oy, int w, int h) {
    DrawRectangle(ox, oy, w, h, (Color){20, 20, 30, 255});
    DrawRectangleLines(ox, oy, w, h, (Color){100, 100, 120, 255});
    DrawText("Status", ox + 8, oy + 4, 14, WHITE);

    const BomberState* state = &snap->state;
    int y = oy + 24;
    char buf[256];

    snprintf(buf, sizeof(buf), "Step: %d/%d", state->step, 500);
    DrawText(buf, ox + 8, y, 12, WHITE); y += 16;

    snprintf(buf, sizeof(buf), "Reward: %.3f", snap->cumulative_reward);
    DrawText(buf, ox + 8, y, 12, (Color){100, 255, 100, 255}); y += 16;

    snprintf(buf, sizeof(buf), "Agent[0]: %s (%d,%d)",
             state->agents[0].alive ? "ALIVE" : "DEAD",
             state->agents[0].x, state->agents[0].y);
    DrawText(buf, ox + 8, y, 12, state->agents[0].alive ? WHITE : RED); y += 16;

    snprintf(buf, sizeof(buf), "Ammo: %d  Range: %d  Speed: %d",
             state->agents[0].bomb_ammo, state->agents[0].blast_range, state->agents[0].speed);
    DrawText(buf, ox + 8, y, 12, WHITE); y += 16;

    snprintf(buf, sizeof(buf), "Score: %d", state->agents[0].score);
    DrawText(buf, ox + 8, y, 12, WHITE); y += 16;

    if (state->agent_count > 1) {
        snprintf(buf, sizeof(buf), "Enemy[1]: %s (%d,%d)",
                 state->agents[1].alive ? "ALIVE" : "DEAD",
                 state->agents[1].x, state->agents[1].y);
        DrawText(buf, ox + 8, y, 12, state->agents[1].alive ? (Color){255,100,100,255} : GRAY);
    }
}

void renderer_draw_controls(int ox, int oy, int w, int h, int paused, int speed_mult) {
    DrawRectangle(ox, oy, w, h, (Color){20, 20, 30, 255});
    DrawRectangleLines(ox, oy, w, h, (Color){100, 100, 120, 255});
    DrawText("Controls", ox + 8, oy + 4, 14, WHITE);

    int y = oy + 24;
    DrawText("[SPACE] Pause/Resume", ox + 8, y, 12, WHITE); y += 16;
    DrawText("[R] Reset episode", ox + 8, y, 12, WHITE); y += 16;
    DrawText("[+/-] Speed", ox + 8, y, 12, WHITE); y += 16;
    DrawText("[S] Step (when paused)", ox + 8, y, 12, WHITE); y += 16;
    DrawText("[ESC] Quit", ox + 8, y, 12, WHITE); y += 16;

    char buf[64];
    snprintf(buf, sizeof(buf), "Speed: %dx | %s", speed_mult, paused ? "PAUSED" : "RUNNING");
    DrawText(buf, ox + 8, y, 12, paused ? YELLOW : GREEN);
}
