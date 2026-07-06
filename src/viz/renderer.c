#include "viz/renderer.h"
#include "env/bomber_map.h"
#include "core/math_util.h"
#include <stdio.h>
#include <string.h>

static Font s_font;

static Color s_session_colors[] = {
    {80, 180, 255, 255},
    {255, 100, 100, 255},
    {100, 255, 100, 255},
    {255, 255, 100, 255},
    {255, 100, 255, 255},
    {100, 255, 255, 255},
    {255, 180, 100, 255},
    {180, 100, 255, 255}
};

Color session_color(int session_idx) {
    return s_session_colors[session_idx % 8];
}

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

/* ===== Multi-epoch / multi-agent views ===== */

void renderer_draw_epoch_graph(const AgentSession* s, int ox, int oy, int w, int h,
                               const char* title) {
    DrawRectangle(ox, oy, w, h, (Color){20, 20, 30, 255});
    DrawRectangleLines(ox, oy, w, h, (Color){100, 100, 120, 255});
    DrawText(title, ox + 8, oy + 4, 14, WHITE);

    if (s->epoch_count < 1) {
        DrawText("No epochs completed", ox + w/2 - 80, oy + h/2, 12, GRAY);
        return;
    }

    int graph_ox = ox + 8;
    int graph_oy = oy + 24;
    int graph_w = w - 16;
    int graph_h = h - 36;

    /* Find min/max */
    float min_v = 0, max_v = 0;
    for (int i = 0; i < s->epoch_count; i++) {
        if (s->epoch_rewards[i] < min_v) min_v = s->epoch_rewards[i];
        if (s->epoch_rewards[i] > max_v) max_v = s->epoch_rewards[i];
        if (s->epoch_avg_rewards[i] > max_v) max_v = s->epoch_avg_rewards[i];
    }
    if (max_v - min_v < 0.01f) { max_v = 1.0f; min_v = -1.0f; }
    float range = max_v - min_v;

    /* Zero line */
    if (min_v < 0 && max_v > 0) {
        int zero_y = graph_oy + graph_h - (int)((0 - min_v) / range * graph_h);
        DrawLine(graph_ox, zero_y, graph_ox + graph_w, zero_y, (Color){80, 80, 80, 255});
    }

    /* Per-epoch reward (thin bars) */
    Color sc = session_color(0);
    int bar_w = graph_w / s->epoch_count;
    if (bar_w < 1) bar_w = 1;
    for (int i = 0; i < s->epoch_count; i++) {
        int bx = graph_ox + i * graph_w / s->epoch_count;
        float v = s->epoch_rewards[i];
        int by = graph_oy + graph_h - (int)((v - min_v) / range * graph_h);
        int zero_y = graph_oy + graph_h - (int)((0 - min_v) / range * graph_h);
        if (v >= 0) {
            DrawRectangle(bx, by, bar_w, zero_y - by, (Color){sc.r, sc.g, sc.b, 120});
        } else {
            DrawRectangle(bx, zero_y, bar_w, by - zero_y, (Color){255, 80, 80, 120});
        }
    }

    /* Running average line */
    for (int i = 1; i < s->epoch_count; i++) {
        int x0 = graph_ox + (i - 1) * graph_w / (s->epoch_count > 1 ? s->epoch_count - 1 : 1);
        int x1 = graph_ox + i * graph_w / (s->epoch_count > 1 ? s->epoch_count - 1 : 1);
        int y0 = graph_oy + graph_h - (int)((s->epoch_avg_rewards[i-1] - min_v) / range * graph_h);
        int y1 = graph_oy + graph_h - (int)((s->epoch_avg_rewards[i] - min_v) / range * graph_h);
        DrawLine(x0, y0, x1, y1, (Color){255, 255, 100, 255});
    }

    /* Labels */
    char buf[64];
    snprintf(buf, sizeof(buf), "Epochs: %d  Avg: %.3f  Max: %.2f",
             s->epoch_count,
             s->epoch_count > 0 ? s->epoch_avg_rewards[s->epoch_count - 1] : 0,
             max_v);
    DrawText(buf, ox + 8, oy + h - 14, 10, (Color){180, 180, 200, 255});
}

void renderer_draw_multi_epoch_graph(const VizSession* vs, int ox, int oy, int w, int h,
                                     const char* title, int show_avg) {
    DrawRectangle(ox, oy, w, h, (Color){20, 20, 30, 255});
    DrawRectangleLines(ox, oy, w, h, (Color){100, 100, 120, 255});
    DrawText(title, ox + 8, oy + 4, 14, WHITE);

    int graph_ox = ox + 8;
    int graph_oy = oy + 24;
    int graph_w = w - 16;
    int graph_h = h - 36;

    /* Find max epoch count and value range across all sessions */
    int max_epochs = 0;
    float min_v = 0, max_v = 0;
    for (int s = 0; s < vs->session_count; s++) {
        const AgentSession* as = &vs->sessions[s];
        if (as->epoch_count > max_epochs) max_epochs = as->epoch_count;
        for (int i = 0; i < as->epoch_count; i++) {
            float v = show_avg ? as->epoch_avg_rewards[i] : as->epoch_rewards[i];
            if (v < min_v) min_v = v;
            if (v > max_v) max_v = v;
        }
    }

    if (max_epochs < 2) {
        DrawText("Waiting for more epochs...", ox + w/2 - 80, oy + h/2, 12, GRAY);
        return;
    }

    if (max_v - min_v < 0.01f) { max_v = 1.0f; min_v = -1.0f; }
    float range = max_v - min_v;

    /* Zero line */
    if (min_v < 0 && max_v > 0) {
        int zero_y = graph_oy + graph_h - (int)((0 - min_v) / range * graph_h);
        DrawLine(graph_ox, zero_y, graph_ox + graph_w, zero_y, (Color){80, 80, 80, 255});
    }

    /* Draw line per session */
    for (int si = 0; si < vs->session_count; si++) {
        const AgentSession* as = &vs->sessions[si];
        if (as->epoch_count < 2) continue;

        Color c = session_color(si);
        for (int i = 1; i < as->epoch_count; i++) {
            float v0 = show_avg ? as->epoch_avg_rewards[i-1] : as->epoch_rewards[i-1];
            float v1 = show_avg ? as->epoch_avg_rewards[i] : as->epoch_rewards[i];
            int x0 = graph_ox + (i - 1) * graph_w / (max_epochs - 1);
            int x1 = graph_ox + i * graph_w / (max_epochs - 1);
            int y0 = graph_oy + graph_h - (int)((v0 - min_v) / range * graph_h);
            int y1 = graph_oy + graph_h - (int)((v1 - min_v) / range * graph_h);
            DrawLine(x0, y0, x1, y1, c);
        }
    }

    /* Legend */
    int leg_x = graph_ox + 4;
    int leg_y = graph_oy + 4;
    for (int si = 0; si < vs->session_count; si++) {
        Color c = session_color(si);
        DrawRectangle(leg_x, leg_y, 10, 10, c);
        DrawText(vs->sessions[si].name, leg_x + 14, leg_y, 10, WHITE);
        leg_x += 14 + MeasureText(vs->sessions[si].name, 10) + 12;
    }
}

void renderer_draw_winrate_graph(const VizSession* vs, int ox, int oy, int w, int h) {
    DrawRectangle(ox, oy, w, h, (Color){20, 20, 30, 255});
    DrawRectangleLines(ox, oy, w, h, (Color){100, 100, 120, 255});
    DrawText("Win Rate Over Epochs", ox + 8, oy + 4, 14, WHITE);

    int graph_ox = ox + 8;
    int graph_oy = oy + 24;
    int graph_w = w - 16;
    int graph_h = h - 36;

    int max_epochs = 0;
    for (int s = 0; s < vs->session_count; s++) {
        if (vs->sessions[s].epoch_count > max_epochs)
            max_epochs = vs->sessions[s].epoch_count;
    }

    if (max_epochs < 2) {
        DrawText("Waiting for more epochs...", ox + w/2 - 80, oy + h/2, 12, GRAY);
        return;
    }

    /* Draw 50% line */
    int mid_y = graph_oy + graph_h / 2;
    DrawLine(graph_ox, mid_y, graph_ox + graph_w, mid_y, (Color){80, 80, 80, 128});

    /* Compute running win rate per session */
    for (int si = 0; si < vs->session_count; si++) {
        const AgentSession* as = &vs->sessions[si];
        if (as->epoch_count < 2) continue;

        Color c = session_color(si);
        int wins = 0;
        for (int i = 0; i < as->epoch_count; i++) {
            wins += as->epoch_wins[i];
            float wr = (float)wins / (i + 1);
            if (i > 0) {
                float prev_wr = (float)(wins - as->epoch_wins[i]) / i;
                int x0 = graph_ox + (i - 1) * graph_w / (max_epochs - 1);
                int x1 = graph_ox + i * graph_w / (max_epochs - 1);
                int y0 = graph_oy + graph_h - (int)(prev_wr * graph_h);
                int y1 = graph_oy + graph_h - (int)(wr * graph_h);
                DrawLine(x0, y0, x1, y1, c);
            }
        }
    }

    /* Legend */
    int leg_x = graph_ox + 4;
    int leg_y = graph_oy + 4;
    for (int si = 0; si < vs->session_count; si++) {
        Color c = session_color(si);
        DrawRectangle(leg_x, leg_y, 10, 10, c);
        DrawText(vs->sessions[si].name, leg_x + 14, leg_y, 10, WHITE);
        leg_x += 14 + MeasureText(vs->sessions[si].name, 10) + 12;
    }
}

void renderer_draw_mini_arena(const DebugSnapshot* snap, int ox, int oy, int cell_size) {
    const BomberState* state = &snap->state;

    for (int y = 0; y < state->height; y++) {
        for (int x = 0; x < state->width; x++) {
            int px = ox + x * cell_size;
            int py = oy + y * cell_size;
            DrawRectangle(px, py, cell_size, cell_size, tile_color(state->tiles[y][x]));
        }
    }

    /* Bombs */
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!state->bombs[i].active) continue;
        int px = ox + state->bombs[i].x * cell_size + cell_size / 2;
        int py = oy + state->bombs[i].y * cell_size + cell_size / 2;
        DrawCircle(px, py, cell_size / 2, (Color){200, 50, 50, 255});
    }

    /* Agents */
    for (int a = 0; a < state->agent_count; a++) {
        if (!state->agents[a].alive) continue;
        int px = ox + state->agents[a].x * cell_size + cell_size / 2;
        int py = oy + state->agents[a].y * cell_size + cell_size / 2;
        DrawCircle(px, py, cell_size / 2, agent_color(a));
    }
}

void renderer_draw_comparison_arenas(const VizSession* vs, int ox, int oy,
                                     int screen_w, int screen_h) {
    int n = vs->session_count;
    if (n == 0) return;

    /* Grid layout */
    int cols = (n <= 2) ? n : (n <= 4) ? 2 : (n <= 6) ? 3 : 4;
    int rows = (n + cols - 1) / cols;

    int margin = 8;
    int avail_w = screen_w - 2 * margin;
    int avail_h = screen_h - 40 - 2 * margin;

    int cell_w = avail_w / cols;
    int cell_h = avail_h / rows;

    for (int i = 0; i < n; i++) {
        int col = i % cols;
        int row = i / cols;
        int px = ox + margin + col * cell_w;
        int py = oy + 30 + margin + row * cell_h;

        /* Background */
        Color c = session_color(i);
        DrawRectangle(px, py, cell_w - margin, cell_h - margin, (Color){15, 15, 20, 255});
        DrawRectangleLines(px, py, cell_w - margin, cell_h - margin, c);

        /* Title */
        char title[64];
        snprintf(title, sizeof(title), "%s  E:%d  R:%.2f  W:%d  D:%d",
                 vs->sessions[i].name,
                 vs->sessions[i].epoch_count,
                 vs->sessions[i].current_reward,
                 vs->sessions[i].total_wins,
                 vs->sessions[i].total_deaths);
        DrawText(title, px + 4, py + 2, 11, c);

        /* Mini arena */
        DebugSnapshot snap;
        env_get_debug_snapshot(&vs->sessions[i].env, &snap);
        const BomberState* st = &snap.state;

        int arena_max_w = cell_w - margin - 8;
        int arena_max_h = cell_h - margin - 24;
        int cs = arena_max_w / st->width;
        if (st->height * cs > arena_max_h) cs = arena_max_h / st->height;
        if (cs < 4) cs = 4;

        int arena_w = st->width * cs;
        int arena_h = st->height * cs;
        int aox = px + (cell_w - margin - arena_w) / 2;
        int aoy = py + 20;

        renderer_draw_mini_arena(&snap, aox, aoy, cs);

        /* Highlight active */
        if (i == vs->active_session) {
            DrawRectangleLines(px, py, cell_w - margin, cell_h - margin,
                              (Color){255, 255, 100, 255});
        }
    }
}

void renderer_draw_agent_selector(const VizSession* vs, int ox, int oy, int w) {
    DrawText("Agents:", ox, oy, 12, (Color){200, 200, 220, 255});
    int x = ox + 60;
    for (int i = 0; i < vs->session_count; i++) {
        Color c = session_color(i);
        if (i == vs->active_session) {
            DrawRectangle(x - 4, oy - 2, MeasureText(vs->sessions[i].name, 12) + 8, 16,
                         (Color){60, 60, 80, 255});
        }
        DrawRectangle(x, oy + 2, 8, 8, c);
        DrawText(vs->sessions[i].name, x + 12, oy, 12,
                i == vs->active_session ? WHITE : (Color){160, 160, 180, 255});
        x += 12 + MeasureText(vs->sessions[i].name, 12) + 16;
    }
}

void renderer_draw_session_stats(const AgentSession* s, int ox, int oy, int w, int h) {
    DrawRectangle(ox, oy, w, h, (Color){20, 20, 30, 255});
    DrawRectangleLines(ox, oy, w, h, (Color){100, 100, 120, 255});

    Color sc = session_color(0);
    DrawText(s->name, ox + 8, oy + 4, 14, sc);

    int y = oy + 24;
    char buf[128];

    snprintf(buf, sizeof(buf), "Epochs: %d/%d", s->epoch_count, MAX_VIZ_EPOCHS);
    DrawText(buf, ox + 8, y, 12, WHITE); y += 16;

    snprintf(buf, sizeof(buf), "Current Reward: %.3f", s->current_reward);
    DrawText(buf, ox + 8, y, 12, (Color){100, 255, 100, 255}); y += 16;

    snprintf(buf, sizeof(buf), "Step: %d/%d", s->current_step, s->env.config.max_steps);
    DrawText(buf, ox + 8, y, 12, WHITE); y += 16;

    snprintf(buf, sizeof(buf), "Total Reward: %.2f", s->total_reward + s->current_reward);
    DrawText(buf, ox + 8, y, 12, WHITE); y += 16;

    snprintf(buf, sizeof(buf), "Wins: %d  Deaths: %d", s->total_wins, s->total_deaths);
    DrawText(buf, ox + 8, y, 12, WHITE); y += 16;

    if (s->epoch_count > 0) {
        float avg = s->epoch_avg_rewards[s->epoch_count - 1];
        float wr = (float)s->total_wins / s->epoch_count;
        snprintf(buf, sizeof(buf), "Avg R/ep: %.3f  Win%%: %.1f", avg, wr * 100);
        DrawText(buf, ox + 8, y, 12, (Color){255, 255, 100, 255}); y += 16;
    }

    /* Agent state */
    y += 4;
    const BomberState* st = &s->env.state;
    snprintf(buf, sizeof(buf), "Agent[0]: %s (%d,%d)",
             st->agents[0].alive ? "ALIVE" : "DEAD",
             st->agents[0].x, st->agents[0].y);
    DrawText(buf, ox + 8, y, 12, st->agents[0].alive ? WHITE : RED); y += 16;

    snprintf(buf, sizeof(buf), "Ammo:%d Range:%d Score:%d",
             st->agents[0].bomb_ammo, st->agents[0].blast_range, st->agents[0].score);
    DrawText(buf, ox + 8, y, 12, WHITE);
}

void renderer_draw_view_controls(const VizSession* vs, int ox, int oy, int w, int h) {
    DrawRectangle(ox, oy, w, h, (Color){20, 20, 30, 255});
    DrawRectangleLines(ox, oy, w, h, (Color){100, 100, 120, 255});
    DrawText("Controls", ox + 8, oy + 4, 14, WHITE);

    int y = oy + 24;
    DrawText("[SPACE] Pause/Resume", ox + 8, y, 11, WHITE); y += 14;
    DrawText("[TAB] Switch agent", ox + 8, y, 11, WHITE); y += 14;
    DrawText("[1/2/3] Arena/Graphs/Compare", ox + 8, y, 11, WHITE); y += 14;
    DrawText("[+/-] Speed", ox + 8, y, 11, WHITE); y += 14;
    DrawText("[S] Step (when paused)", ox + 8, y, 11, WHITE); y += 14;
    DrawText("[A] Toggle auto-advance", ox + 8, y, 11, WHITE); y += 14;
    DrawText("[D] Toggle danger overlay", ox + 8, y, 11, WHITE); y += 14;
    DrawText("[O] Toggle local obs", ox + 8, y, 11, WHITE); y += 14;
    DrawText("[R] Reset all sessions", ox + 8, y, 11, WHITE); y += 14;
    DrawText("[ESC] Quit", ox + 8, y, 11, WHITE); y += 14;

    y += 4;
    char buf[128];
    snprintf(buf, sizeof(buf), "Speed: %dx | %s", vs->speed_mult,
             vs->paused ? "PAUSED" : "RUNNING");
    DrawText(buf, ox + 8, y, 11, vs->paused ? YELLOW : GREEN); y += 14;

    const char* view_names[] = {"Arena", "Graphs", "Comparison"};
    snprintf(buf, sizeof(buf), "View: %s", view_names[vs->view_mode]);
    DrawText(buf, ox + 8, y, 11, (Color){100, 200, 255, 255}); y += 14;

    snprintf(buf, sizeof(buf), "Auto-advance: %s", vs->auto_advance_epoch ? "ON" : "OFF");
    DrawText(buf, ox + 8, y, 11, vs->auto_advance_epoch ? GREEN : (Color){200, 100, 100, 255});
}
