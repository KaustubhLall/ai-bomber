#include "viz/renderer.h"
#include "viz/theme.h"
#include "env/bomber_map.h"
#include "core/math_util.h"
#include <stdio.h>
#include <string.h>

static Font s_font;
static int s_show_danger = 1;
static int s_show_grid = 0;

void renderer_set_arena_options(int show_danger, int show_grid) {
    s_show_danger = show_danger;
    s_show_grid = show_grid;
}

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

static void draw_block_sprite(TileType tile, int px, int py, int size, Color base) {
    int inset = size > 12 ? 2 : 1;
    DrawRectangle(px + inset, py + inset, size - inset * 2, size - inset * 2, base);

    if (tile == TILE_SOLID_WALL) {
        Color mortar = (Color){24, 34, 48, 255};
        Color shine = (Color){105, 122, 145, 255};
        int half = size / 2;
        DrawLine(px + inset, py + half, px + size - inset - 1, py + half, mortar);
        DrawLine(px + half, py + inset, px + half, py + half, mortar);
        DrawLine(px + size / 4, py + half, px + size / 4, py + size - inset - 1, mortar);
        DrawLine(px + inset + 1, py + inset + 1, px + size - inset - 2, py + inset + 1, shine);
    } else if (tile == TILE_CRATE) {
        Color plank = (Color){104, 61, 31, 255};
        Color nail = (Color){43, 31, 24, 255};
        DrawRectangleLines(px + inset, py + inset, size - inset * 2, size - inset * 2, plank);
        DrawLine(px + inset + 2, py + inset + 2, px + size - inset - 3, py + size - inset - 3, plank);
        DrawLine(px + size - inset - 3, py + inset + 2, px + inset + 2, py + size - inset - 3, plank);
        DrawCircle(px + inset + 3, py + inset + 3, size > 20 ? 2.0f : 1.0f, nail);
        DrawCircle(px + size - inset - 4, py + size - inset - 4, size > 20 ? 2.0f : 1.0f, nail);
    }
}

static void draw_powerup_sprite(TileType tile, int px, int py, int size) {
    int cx = px + size / 2;
    int cy = py + size / 2;
    int radius = size / 3;
    Color color = tile == TILE_POWERUP_BOMB ? (Color){244, 83, 83, 255} :
                  tile == TILE_POWERUP_RANGE ? (Color){255, 184, 62, 255} :
                                               (Color){78, 190, 255, 255};
    DrawCircle(cx + 1, cy + 2, radius + 2, (Color){7, 12, 20, 180});
    DrawCircle(cx, cy, radius, color);
    DrawCircleLines(cx, cy, radius, (Color){245, 248, 255, 255});

    if (tile == TILE_POWERUP_BOMB) {
        DrawCircle(cx, cy + 1, radius / 2, (Color){20, 24, 31, 255});
        DrawLine(cx + 2, cy - radius / 2, cx + radius / 2, cy - radius, (Color){255, 230, 120, 255});
    } else if (tile == TILE_POWERUP_RANGE) {
        DrawLine(cx - radius / 2, cy, cx + radius / 2, cy, WHITE);
        DrawLine(cx, cy - radius / 2, cx, cy + radius / 2, WHITE);
    } else {
        DrawLine(cx - radius / 2, cy + radius / 3, cx, cy - radius / 3, WHITE);
        DrawLine(cx, cy - radius / 3, cx + radius / 2, cy + radius / 3, WHITE);
    }
}

static void draw_agent_sprite(int agent_id, int px, int py, int size, Color body) {
    int unit = size / 8;
    if (unit < 1) unit = 1;
    int cx = px + size / 2;
    int top = py + unit;
    Color outline = (Color){8, 13, 22, 255};
    Color face = (Color){246, 202, 164, 255};

    DrawEllipse(cx + unit / 2, py + size - unit, size * 0.30f, unit * 0.8f, (Color){5, 8, 14, 130});
    DrawRectangle(cx - 3 * unit, top + 2 * unit, 6 * unit, 4 * unit, outline);
    DrawRectangle(cx - 2 * unit, top + 3 * unit, 4 * unit, 3 * unit, body);
    DrawCircle(cx, top + 2 * unit, 2.4f * unit, outline);
    DrawCircle(cx, top + 2 * unit, 1.8f * unit, face);
    DrawRectangle(cx - 2 * unit, top, 4 * unit, 2 * unit, body);
    DrawRectangle(cx - 3 * unit, top + unit, unit, 2 * unit, body);
    DrawRectangle(cx + 2 * unit, top + unit, unit, 2 * unit, body);
    DrawRectangle(cx - 2 * unit, top + 6 * unit, 2 * unit, unit, outline);
    DrawRectangle(cx + unit, top + 6 * unit, 2 * unit, unit, outline);
    DrawCircle(cx - unit, top + 2 * unit, size > 20 ? 1.5f : 1.0f, outline);
    DrawCircle(cx + unit, top + 2 * unit, size > 20 ? 1.5f : 1.0f, outline);

    if (size >= 28) {
        char label[4];
        snprintf(label, sizeof(label), "%d", agent_id);
        DrawText(label, cx - 3, top + 3 * unit, 9, WHITE);
    }
}

void renderer_draw_arena(const DebugSnapshot* snap, int ox, int oy, int tile_size) {
    const BomberState* state = &snap->state;
    const DangerMap* dm = &snap->danger;
    const ThemeColors* tc = theme_colors();
    /* Floor first: a quiet checkerboard gives the arena shape without noise. */
    for (int y = 0; y < state->height; y++) {
        for (int x = 0; x < state->width; x++) {
            int px = ox + x * tile_size;
            int py = oy + y * tile_size;

            Color floor = tc->floor;
            if ((x + y) & 1) floor = (Color){floor.r + 4, floor.g + 5, floor.b + 7, floor.a};
            DrawRectangle(px, py, tile_size, tile_size, floor);

            /* Blocks sit on top of the floor with highlight and shadow. */
            Color tile_c;
            switch (state->tiles[y][x]) {
                case TILE_FLOOR:         tile_c = floor; break;
                case TILE_SOLID_WALL:    tile_c = tc->solid_wall; break;
                case TILE_CRATE:         tile_c = tc->destructible_wall; break;
                case TILE_POWERUP_BOMB:  tile_c = (Color){255, 80, 80, 255}; break;
                case TILE_POWERUP_RANGE: tile_c = (Color){80, 255, 80, 255}; break;
                case TILE_POWERUP_SPEED: tile_c = (Color){80, 180, 255, 255}; break;
                default:                 tile_c = BLACK; break;
            }
            if (state->tiles[y][x] == TILE_CRATE || state->tiles[y][x] == TILE_SOLID_WALL)
                draw_block_sprite(state->tiles[y][x], px, py, tile_size, tile_c);
            else if (state->tiles[y][x] >= TILE_POWERUP_BOMB)
                draw_powerup_sprite(state->tiles[y][x], px, py, tile_size);

            if (s_show_grid) DrawRectangleLines(px, py, tile_size, tile_size, tc->grid);

            /* Draw danger overlay using theme colors */
            int tblast = dm->time_to_blast[y][x];
            if (s_show_danger && tblast >= 0 && tblast <= 3) {
                Color dc = (tblast == 0) ? tc->danger_now : tc->danger_soon;
                DrawRectangle(px, py, tile_size, tile_size, dc);
            }

            /* Draw safe tile overlay */
            if (s_show_danger && dm->reachable_safe[y][x] && tblast < 0) {
                DrawCircle(px + tile_size / 2, py + tile_size / 2, 2.0f, tc->safe_reachable);
            }
        }
    }

    /* Draw bombs with improved visuals */
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!state->bombs[i].active) continue;
        int px = ox + state->bombs[i].x * tile_size + tile_size / 2;
        int py = oy + state->bombs[i].y * tile_size + tile_size / 2;
        int radius = tile_size / 3;

        /* Pulsing effect based on timer */
        float pulse = 1.0f - (float)state->bombs[i].timer / 4.0f;
        radius = (int)(radius * (1.0f + pulse * 0.3f));

        /* Bomb shadow */
        DrawCircle(px + 2, py + 2, radius, (Color){20, 20, 20, 128});
        DrawCircle(px, py, radius, (Color){8, 10, 14, 255});
        DrawCircle(px - radius / 3, py - radius / 3, radius / 4, (Color){74, 80, 89, 255});
        DrawLine(px + radius / 3, py - radius + 1, px + radius / 2, py - radius - 4, (Color){218, 155, 48, 255});
        DrawCircle(px + radius / 2, py - radius - 4, 2.0f, (Color){255, 193, 61, 255});
        DrawCircleLines(px, py, radius + 1, (Color){255, 181, 48, 255});

        /* Timer label */
        char timer_str[8];
        snprintf(timer_str, sizeof(timer_str), "%d", state->bombs[i].timer);
        int tw = MeasureText(timer_str, 12);
        DrawText(timer_str, px - tw / 2, py - 6, 12, WHITE);
    }

    /* Draw agents */
    for (int a = 0; a < state->agent_count; a++) {
        if (!state->agents[a].alive) continue;
        int px = ox + state->agents[a].x * tile_size;
        int py = oy + state->agents[a].y * tile_size;
        Color body = a == 0 ? tc->agent : tc->enemy;
        draw_agent_sprite(a, px, py, tile_size, body);
    }

}

void renderer_draw_observation_overlay(const DebugSnapshot* snap, int ox, int oy, int tile_size) {
    const BomberState* state = &snap->state;
    if (state->agent_count > 0 && state->agents[0].alive) {
        int ax = state->agents[0].x;
        int ay = state->agents[0].y;
        int half = LOCAL_OBS_HALF;
        int bx = ox + (ax - half) * tile_size;
        int by = oy + (ay - half) * tile_size;
        int bw = LOCAL_OBS_SIZE * tile_size;
        int bh = LOCAL_OBS_SIZE * tile_size;
        Color c = theme_colors()->observation_border;
        int dash = 8;
        for (int x = 0; x < bw; x += dash * 2) {
            DrawLine(bx + x, by, bx + (x + dash < bw ? x + dash : bw), by, c);
            DrawLine(bx + x, by + bh, bx + (x + dash < bw ? x + dash : bw), by + bh, c);
        }
        for (int y = 0; y < bh; y += dash * 2) {
            DrawLine(bx, by + y, bx, by + (y + dash < bh ? y + dash : bh), c);
            DrawLine(bx + bw, by + y, bx + bw, by + (y + dash < bh ? y + dash : bh), c);
        }
        DrawRectangle(bx + 4, by + 4, 118, 18, (Color){12, 19, 29, 220});
        DrawText("11x11 Observation", bx + 8, by + 7, 11, c);
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
    DrawText("[1/2/3] Arena/Compare/Graphs", ox + 8, y, 11, WHITE); y += 14;
    DrawText("[+/-] Speed", ox + 8, y, 11, WHITE); y += 14;
    DrawText("[S] Step (when paused)", ox + 8, y, 11, WHITE); y += 14;
    DrawText("[A] Toggle auto-advance", ox + 8, y, 11, WHITE); y += 14;
    DrawText("[D] Toggle danger overlay", ox + 8, y, 11, WHITE); y += 14;
    DrawText("[O] Toggle local obs", ox + 8, y, 11, WHITE); y += 14;
    DrawText("[R] Reset all sessions", ox + 8, y, 11, WHITE); y += 14;
    DrawText("[ESC] Quit", ox + 8, y, 11, WHITE); y += 14;

    y += 4;
    char buf[128];
    snprintf(buf, sizeof(buf), "Game: %d step/s | %s", vs->simulation_hz,
             vs->paused ? "PAUSED" : "RUNNING");
    DrawText(buf, ox + 8, y, 11, vs->paused ? YELLOW : GREEN); y += 14;

    const char* view_names[] = {"Arena", "Comparison", "Graphs", "Debug", "History"};
    snprintf(buf, sizeof(buf), "View: %s", view_names[vs->view_mode]);
    DrawText(buf, ox + 8, y, 11, (Color){100, 200, 255, 255}); y += 14;

    snprintf(buf, sizeof(buf), "Auto-advance: %s", vs->auto_advance_epoch ? "ON" : "OFF");
    DrawText(buf, ox + 8, y, 11, vs->auto_advance_epoch ? GREEN : (Color){200, 100, 100, 255});
}

void renderer_draw_legend(int ox, int oy, int w, int h) {
    const ThemeColors* tc = theme_colors();
    const ThemeFonts* tf = theme_fonts();
    const ThemeSpacing* ts = theme_spacing();

    /* Draw legend background */
    DrawRectangle(ox, oy, w, h, (Color){20, 25, 35, 220});
    DrawRectangleLines(ox, oy, w, h, tc->panel_border);

    int x = ox + ts->padding_x;
    int y = oy + ts->padding_y;
    int item_h = tf->small + 4;
    int col_w = (w - 2 * ts->padding_x) / 2;

    /* Title */
    DrawText("Legend", x, y, tf->section, tc->text_primary);
    y += tf->section + ts->gap_y;

    /* Column 1 */
    DrawCircle(x + 6, y + 6, 6, tc->agent);
    DrawText("Agent", x + 16, y, tf->small, tc->text_secondary); y += item_h;

    DrawRectangle(x, y, 12, 12, tc->solid_wall);
    DrawText("Solid Wall", x + 16, y, tf->small, tc->text_secondary); y += item_h;

    DrawRectangle(x, y, 12, 12, tc->destructible_wall);
    DrawText("Crate", x + 16, y, tf->small, tc->text_secondary); y += item_h;

    DrawCircle(x + 6, y + 6, 6, tc->enemy);
    DrawText("Enemy", x + 16, y, tf->small, tc->text_secondary); y += item_h;

    DrawCircle(x + 6, y + 6, 5, (Color){92, 224, 138, 255});
    DrawText("Powerup", x + 16, y, tf->small, tc->text_secondary); y += item_h;

    /* Column 2 */
    x = ox + ts->padding_x + col_w;
    y = oy + ts->padding_y + tf->section + ts->gap_y;

    DrawCircle(x + 6, y + 6, 6, (Color){8, 10, 14, 255});
    DrawCircleLines(x + 6, y + 6, 7, (Color){255, 181, 48, 255});
    DrawText("Bomb", x + 16, y, tf->small, tc->text_secondary); y += item_h;

    DrawRectangle(x, y, 12, 12, tc->danger_now);
    DrawText("Danger Now", x + 16, y, tf->small, tc->text_secondary); y += item_h;

    DrawRectangle(x, y, 12, 12, tc->danger_soon);
    DrawText("Danger Soon", x + 16, y, tf->small, tc->text_secondary); y += item_h;

    DrawRectangle(x, y, 12, 12, tc->observation_border);
    DrawRectangle(x + 2, y + 2, 8, 8, tc->observation_fill);
    DrawText("Observation", x + 16, y, tf->small, tc->text_secondary);
}
