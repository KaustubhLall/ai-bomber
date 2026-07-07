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
#include "viz/theme.h"
#include "viz/layout.h"
#include "core/playback_clock.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define SCREEN_W 1400
#define SCREEN_H 860

static const char* action_name(Action action) {
    static const char* names[] = {"UP", "DOWN", "LEFT", "RIGHT", "BOMB", "WAIT"};
    return action >= 0 && action < ACTION_COUNT ? names[action] : "NONE";
}

static const AgentType selectable_policies[] = {
    AGENT_RANDOM, AGENT_SCRIPTED, AGENT_GREEDY_CRATE,
    AGENT_HEURISTIC, AGENT_EVASIVE, AGENT_ALPHABETA, AGENT_MCTS
};

static AgentType cycle_policy(AgentType current, int direction) {
    int count = (int)(sizeof(selectable_policies) / sizeof(selectable_policies[0]));
    int index = 0;
    for (int i = 0; i < count; i++) if (selectable_policies[i] == current) index = i;
    index = (index + direction + count) % count;
    return selectable_policies[index];
}

static const char* outcome_name(TerminalReason outcome) {
    switch (outcome) {
        case TERMINAL_WIN: return "BLUE WIN";
        case TERMINAL_AGENT_DEAD: return "BLUE DEFEAT";
        case TERMINAL_DRAW: return "DRAW";
        case TERMINAL_TIMEOUT: return "TIMEOUT";
        default: return "IN PROGRESS";
    }
}

static void draw_matchup_overlay(const VizSession* vs) {
    const ThemeColors* tc = theme_colors();
    static const char* map_names[] = {"Open (30% crates)", "Standard (50% crates)", "Dense (70% crates)"};
    int x = 420, y = 120, w = 560, h = 620;
    DrawRectangle(x - 5, y - 5, w + 10, h + 10, (Color){0,0,0,190});
    DrawRectangle(x, y, w, h, (Color){19,26,38,252});
    DrawRectangleLines(x, y, w, h, tc->agent);
    DrawText("Live Policy Arena", x + 24, y + 22, 26, tc->text_primary);
    DrawText("Choose both policies, seed, then start a recorded match.", x + 24, y + 58, 14, tc->text_secondary);
    DrawText("Blue", x + 40, y + 112, 18, tc->agent);
    DrawRectangle(x + 130, y + 100, 290, 36, tc->panel);
    DrawText(agent_type_name(vs->matchup_blue), x + 205, y + 109, 18, tc->text_primary);
    DrawRectangle(x + 88, y + 100, 34, 36, tc->button); DrawText("<", x + 100, y + 109, 18, WHITE);
    DrawRectangle(x + 430, y + 100, 34, 36, tc->button); DrawText(">", x + 442, y + 109, 18, WHITE);
    DrawText("Red", x + 40, y + 174, 18, tc->enemy);
    DrawRectangle(x + 130, y + 162, 290, 36, tc->panel);
    DrawText(agent_type_name(vs->matchup_red), x + 205, y + 171, 18, tc->text_primary);
    DrawRectangle(x + 88, y + 162, 34, 36, tc->button); DrawText("<", x + 100, y + 171, 18, WHITE);
    DrawRectangle(x + 430, y + 162, 34, 36, tc->button); DrawText(">", x + 442, y + 171, 18, WHITE);
    char seed[80]; snprintf(seed, sizeof(seed), "Seed: %llu", (unsigned long long)vs->matchup_seed);
    DrawText(seed, x + 190, y + 232, 18, tc->text_primary);
    DrawRectangle(x + 88, y + 222, 70, 34, tc->button); DrawText("- seed", x + 99, y + 231, 14, WHITE);
    DrawRectangle(x + 430, y + 222, 70, 34, tc->button); DrawText("+ seed", x + 440, y + 231, 14, WHITE);
    DrawText("Map", x + 40, y + 294, 18, tc->text_primary);
    DrawRectangle(x + 130, y + 282, 290, 36, tc->panel);
    DrawText(map_names[vs->matchup_map_preset], x + 180, y + 291, 16, tc->text_primary);
    DrawRectangle(x + 88, y + 282, 34, 36, tc->button); DrawText("<", x + 100, y + 291, 18, WHITE);
    DrawRectangle(x + 430, y + 282, 34, 36, tc->button); DrawText(">", x + 442, y + 291, 18, WHITE);
    char matches[80]; snprintf(matches, sizeof(matches), "Matches: %d", vs->matchup_matches);
    DrawText(matches, x + 210, y + 354, 18, tc->text_primary);
    DrawRectangle(x + 88, y + 342, 70, 34, tc->button); DrawText("- match", x + 96, y + 351, 14, WHITE);
    DrawRectangle(x + 430, y + 342, 70, 34, tc->button); DrawText("+ match", x + 438, y + 351, 14, WHITE);
    DrawRectangle(x + 150, y + 422, 260, 52, (Color){45,115,86,255});
    DrawText("START RECORDED MATCH", x + 178, y + 438, 18, WHITE);
    DrawText("M closes | History: key 5", x + 170, y + 545, 14, tc->text_dim);
}

static void draw_history_view(VizSession* vs, int screen_w, int screen_h) {
    const ThemeColors* tc = theme_colors();
    ClearBackground(tc->background);
    DrawText("Match History & Replay", 24, 24, 24, tc->text_primary);
    if (vs->history.count == 0 && (!vs->history_replay || vs->history_replay->frame_count <= 0)) {
        DrawText("No recorded matches yet. Press M to launch one.", 24, 70, 18, tc->text_secondary);
        return;
    }
    int selected = vs->history.selected;
    if (selected < 0) selected = vs->history.count - 1;
    MatchHistoryEntry* entry = selected >= 0 ? &vs->history.entries[selected] : NULL;
    int list_y = 72;
    for (int i = vs->history.count - 1; i >= 0 && i >= vs->history.count - 12; i--) {
        MatchHistoryEntry* item = &vs->history.entries[i];
        char row[160];
        snprintf(row, sizeof(row), "#%d %s vs %s  seed %llu  %s", item->id, item->agent,
                 item->opponent, (unsigned long long)item->seed, outcome_name(item->outcome));
        DrawText(row, 24, list_y, 13, i == selected ? tc->agent : tc->text_secondary);
        list_y += 23;
    }
    if (!vs->history_replay || vs->history_replay->frame_count <= 0) {
        DrawText("Select with PageUp/PageDown", 24, 370, 15, tc->warning);
        return;
    }
    ReplayFrame* frame = &vs->history_replay->frames[vs->history_frame];
    DebugSnapshot snap; memset(&snap, 0, sizeof(snap)); snap.state = frame->state;
    danger_compute(&snap.danger, &snap.state); danger_compute_escape(&snap.danger, &snap.state, 0);
    int tile = layout_calc_tile_size(snap.state.width, snap.state.height, 680, 590);
    renderer_draw_arena(&snap, 390, 105, tile);
    char info[256];
    if (entry) snprintf(info, sizeof(info), "#%d  %s vs %s  seed %llu  map %dx%d/%d%%", entry->id,
                        entry->agent, entry->opponent, (unsigned long long)entry->seed,
                        entry->map_width, entry->map_height, entry->crate_density);
    else snprintf(info, sizeof(info), "Loaded replay  %s vs %s  seed %llu", vs->history_replay->agent_name,
                  vs->history_replay->opponent_name, (unsigned long long)vs->history_replay->seed);
    DrawText(info, 390, 70, 18, tc->text_primary);
    snprintf(info, sizeof(info), "Frame %d/%d | Step %d | %s | hash %llu", vs->history_frame + 1,
             vs->history_replay->frame_count, frame->step,
             outcome_name(entry ? entry->outcome : frame->terminal),
             (unsigned long long)frame->state_hash);
    DrawText(info, 390, 720, 15, tc->text_secondary);
    int owned = 0, self_kills = 0, opponent_self = 0, opponent_kills = 0;
    if (entry) {
        owned = entry->owned_eliminations; self_kills = entry->self_kills;
        opponent_self = entry->opponent_self_kills; opponent_kills = entry->opponent_kills;
    } else {
        const BomberState* state = &frame->state;
        if (!state->agents[0].alive) {
            if (state->death_owner[0] == 0) self_kills++;
            else if (state->death_owner[0] > 0) opponent_kills++;
        }
        for (int a = 1; a < state->agent_count; a++) if (!state->agents[a].alive) {
            if (state->death_owner[a] == 0) owned++;
            else if (state->death_owner[a] == a) opponent_self++;
        }
    }
    snprintf(info, sizeof(info), "Owned kills %d | self %d | opponent self %d | opponent kills %d",
             owned, self_kills, opponent_self, opponent_kills);
    DrawText(info, 390, 748, 15, tc->text_secondary);
    DrawText("PageUp/PageDown match | Left/Right frame | Home restart | Space play/pause | M new match",
             390, 790, 13, tc->text_dim);
    (void)screen_w; (void)screen_h;
}

static int next_render_fps(int current, int direction) {
    static const int choices[] = {15, 30, 60, 120, 240};
    int idx = 2;
    for (int i = 0; i < 5; i++) if (choices[i] == current) idx = i;
    idx += direction;
    if (idx < 0) idx = 0;
    if (idx > 4) idx = 4;
    return choices[idx];
}

static void apply_render_fps(VizSession* vs, int fps) {
    if (fps < 5) fps = 5;
    if (fps > 1000) fps = 1000;
    vs->target_fps = fps;
    SetTargetFPS(fps);
}

static int next_simulation_hz(int current, int direction) {
    static const int choices[] = {1, 2, 3, 5, 10, 30};
    int idx = 0;
    for (int i = 0; i < 6; i++) if (choices[i] == current) idx = i;
    idx += direction;
    if (idx < 0) idx = 0;
    if (idx > 5) idx = 5;
    return choices[idx];
}

static void apply_simulation_hz(VizSession* vs, PlaybackClock* clock, int hz) {
    if (hz < 1) hz = 1;
    if (hz > 60) hz = 60;
    vs->simulation_hz = hz;
    playback_clock_reset(clock);
}

static void draw_runtime_controls(const VizSession* vs, int editing, const char* entry) {
    const ThemeColors* tc = theme_colors();
    const char* labels[] = {"Game -", "Game +", "Render -", "Render +", "Set Game"};
    DrawRectangle(570, 58, 465, 28, (Color){18, 24, 34, 245});
    for (int i = 0; i < 5; i++) {
        int x = 575 + i * 90;
        DrawRectangle(x, 61, 82, 22, tc->button);
        DrawRectangleLines(x, 61, 82, 22, tc->panel_border);
        DrawText(labels[i], x + 9, 66, 12, tc->text_primary);
    }
    char status[96];
    snprintf(status, sizeof(status), "Game %d/s | Render %d", vs->simulation_hz, vs->target_fps);
    DrawText(status, 1044, 66, 12, tc->text_secondary);
    if (editing) {
        DrawRectangle(520, 360, 360, 120, (Color){15, 21, 31, 250});
        DrawRectangleLines(520, 360, 360, 120, tc->agent);
        DrawText("Set game steps/second (1-60)", 548, 382, 20, tc->text_primary);
        DrawRectangle(548, 418, 300, 34, (Color){8, 12, 18, 255});
        DrawText(entry[0] ? entry : "type a number", 560, 427, 18, entry[0] ? tc->positive : tc->text_dim);
        DrawText("ENTER apply | ESC cancel", 585, 458, 12, tc->text_secondary);
    }
}

static void draw_help_overlay(int screen_w, int screen_h) {
    const ThemeColors* tc = theme_colors();
    int w = 760, h = 520, x = (screen_w - w) / 2, y = (screen_h - h) / 2;
    DrawRectangle(x - 4, y - 4, w + 8, h + 8, (Color){0, 0, 0, 190});
    DrawRectangle(x, y, w, h, (Color){20, 27, 38, 250});
    DrawRectangleLines(x, y, w, h, tc->panel_border);
    DrawText("Help & Powerups", x + 24, y + 20, 24, tc->text_primary);
    DrawText("Controls", x + 24, y + 62, 18, tc->agent);
    DrawText("SPACE pause/resume    S single-step    R reset    TAB next policy", x + 24, y + 90, 15, tc->text_secondary);
    DrawText("+/- game rate (1/2/3/5/10/30 steps/sec)    [/] render FPS", x + 24, y + 116, 15, tc->text_secondary);
    DrawText("1-4 views    5 history    M live matchup    F7 exact game rate", x + 24, y + 142, 15, tc->text_secondary);
    DrawText("D danger    O observation    G grid    L legend    P screenshot    ESC clean exit", x + 24, y + 168, 15, tc->text_secondary);
    DrawText("Powerups", x + 24, y + 214, 18, tc->agent);
    DrawCircle(x + 34, y + 254, 10, (Color){244, 83, 83, 255});
    DrawText("Bomb capacity", x + 58, y + 244, 16, tc->text_primary);
    DrawText("Adds one reusable bomb slot. Ammo returns when that bomb explodes.", x + 58, y + 266, 14, tc->text_secondary);
    DrawCircle(x + 34, y + 320, 10, (Color){255, 184, 62, 255});
    DrawText("Blast range", x + 58, y + 310, 16, tc->text_primary);
    DrawText("Extends future bomb flames by one tile in each open direction.", x + 58, y + 332, 14, tc->text_secondary);
    DrawCircle(x + 34, y + 386, 10, (Color){78, 190, 255, 255});
    DrawText("Speed level", x + 58, y + 376, 16, tc->text_primary);
    DrawText("Currently tracked in state/observations, but does not yet change grid movement.", x + 58, y + 398, 14, tc->warning);
    DrawText("Powerups have a 30% default chance to replace a destroyed crate.", x + 24, y + 452, 14, tc->text_secondary);
    DrawRectangle(x + w - 150, y + h - 44, 126, 28, tc->button);
    DrawRectangleLines(x + w - 150, y + h - 44, 126, 28, tc->panel_border);
    DrawText("Close help (H)", x + w - 139, y + h - 37, 14, tc->text_primary);
}

static void ensure_screenshot_dir(void) {
#ifdef _WIN32
    (void)_mkdir("screenshots");
#else
    (void)mkdir("screenshots", 0755);
#endif
}

static int action_is_valid(const DebugSnapshot* snap, Action action) {
    const BomberAgentState* a = &snap->state.agents[0];
    int x = a->x, y = a->y;
    if (action == ACTION_UP) y--;
    else if (action == ACTION_DOWN) y++;
    else if (action == ACTION_LEFT) x--;
    else if (action == ACTION_RIGHT) x++;
    if (action >= ACTION_UP && action <= ACTION_RIGHT)
        return map_in_bounds(&snap->state, x, y) && map_is_walkable(&snap->state, x, y);
    if (action == ACTION_PLACE_BOMB) return a->bomb_ammo > 0;
    return action == ACTION_WAIT;
}

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

    const ThemeColors* tc = theme_colors();
    const ThemeFonts* tf = theme_fonts();
    const ThemeSpacing* ts = theme_spacing();
    ArenaLayout layout = layout_get_arena();

    Observation obs;
    DebugSnapshot snap;
    env_observe(&s->env, 0, &obs);
    env_get_debug_snapshot(&s->env, &snap);

    /* Draw top bar */
    DrawRectangle(layout.top_bar.x, layout.top_bar.y, layout.top_bar.width, layout.top_bar.height, tc->panel);
    DrawRectangleLines(layout.top_bar.x, layout.top_bar.y, layout.top_bar.width, layout.top_bar.height, tc->panel_border);

    int tx = layout.top_bar.x + ts->padding_x;
    int ty = layout.top_bar.y + (layout.top_bar.height - tf->header) / 2;
    DrawText("AI Bomber", tx, ty, tf->header, tc->text_primary);

    tx += MeasureText("AI Bomber", tf->header) + ts->gap_x * 4;
    DrawText("Arena Inspector", tx, ty, tf->section, tc->text_secondary);

    tx += MeasureText("Arena Inspector", tf->section) + ts->gap_x * 4;
    char policy_buf[160];
    snprintf(policy_buf, sizeof(policy_buf), "Agent Policy: %s   Opponent Policy: %s%s",
             s->name, s->opponent_name,
             snap.state.agent_count > 2 && s->has_opponent_policy ? " shared by enemies 1..N" : "");
    DrawText(policy_buf, tx, ty, tf->section, tc->agent);

    /* Right side status in top bar */
    char status_buf[128];
    snprintf(status_buf, sizeof(status_buf), "Ep: %d/%d | Step: %d | Game: %d/s | Render: %d | %s",
             s->epoch_count, vs->max_epochs, s->current_step, vs->simulation_hz,
             vs->target_fps, vs->paused ? "PAUSED" : "RUNNING");
    int status_w = MeasureText(status_buf, tf->body);
    DrawText(status_buf, layout.top_bar.x + layout.top_bar.width - status_w - ts->padding_x, ty,
             tf->body, vs->paused ? tc->warning : tc->positive);

    /* Calculate tile size to fit arena in available space */
    int tile_size = layout_calc_tile_size(snap.state.width, snap.state.height,
                                          layout.arena.width - 2 * ts->padding_x,
                                          layout.arena.height - 2 * ts->padding_y);
    int arena_pixel_w = snap.state.width * tile_size;
    int arena_pixel_h = snap.state.height * tile_size;
    int arena_ox = layout.arena.x + (layout.arena.width - arena_pixel_w) / 2;
    int arena_oy = layout.arena.y + (layout.arena.height - arena_pixel_h) / 2;

    /* Draw arena background */
    DrawRectangle(layout.arena.x, layout.arena.y, layout.arena.width, layout.arena.height, tc->background);

    /* Draw arena */
    renderer_set_arena_options(vs->show_danger, vs->show_grid);
    renderer_draw_arena(&snap, arena_ox, arena_oy, tile_size);

    /* Draw legend if enabled */
    if (vs->show_legend) {
        renderer_draw_legend(layout.legend.x, layout.legend.y, layout.legend.width, layout.legend.height);
    }

    /* Draw right inspector panel */
    DrawRectangle(layout.right_panel.x, layout.right_panel.y, layout.right_panel.width, layout.right_panel.height, tc->panel);
    DrawRectangleLines(layout.right_panel.x, layout.right_panel.y, layout.right_panel.width, layout.right_panel.height, tc->panel_border);

    int rx = layout.right_panel.x + ts->padding_x;
    int ry = layout.right_panel.y + ts->padding_y;

    /* Section: Current Decision */
    DrawText("Current Decision", rx, ry, tf->section, tc->text_primary);
    ry += tf->section + ts->gap_y;

    char decision_buf[128];
    snprintf(decision_buf, sizeof(decision_buf), "Last action: %s", action_name(snap.last_action));
    DrawText(decision_buf, rx, ry, tf->body, tc->text_secondary); ry += tf->body + ts->gap_y;

    if (snap.decision_text[0] != '\0') {
        DrawText("Reason: No decision trace emitted by this policy.", rx, ry, tf->small, tc->text_dim); ry += tf->small + ts->gap_y;
    } else {
        DrawText("No decision trace emitted by this policy.", rx, ry, tf->small, tc->text_dim); ry += tf->small + ts->gap_y;
    }

    /* Danger status */
    const BomberAgentState* agent = &snap.state.agents[0];
    int in_danger = (snap.danger.time_to_blast[agent->y][agent->x] >= 0 &&
                     snap.danger.time_to_blast[agent->y][agent->x] <= 3);
    snprintf(decision_buf, sizeof(decision_buf), "Danger: %s", in_danger ? "IMMEDIATE" : "None");
    DrawText(decision_buf, rx, ry, tf->body, in_danger ? tc->danger : tc->positive); ry += tf->body + ts->gap_y;

    char valid[128] = "Valid actions:";
    char safe[128] = "Safe actions:";
    for (int a = 0; a < ACTION_COUNT; a++) {
        if (action_is_valid(&snap, (Action)a)) { strncat(valid, " ", sizeof(valid) - strlen(valid) - 1); strncat(valid, action_name((Action)a), sizeof(valid) - strlen(valid) - 1); }
        if (snap.danger.action_safe[a]) { strncat(safe, " ", sizeof(safe) - strlen(safe) - 1); strncat(safe, action_name((Action)a), sizeof(safe) - strlen(safe) - 1); }
    }
    DrawText(valid, rx, ry, tf->small, tc->text_secondary); ry += tf->small + ts->gap_y;
    DrawText(safe, rx, ry, tf->small, tc->positive); ry += tf->small + ts->gap_y;

    if (s->type == AGENT_ALPHABETA) {
        const SearchDiagnostics* sd = &s->agent.diagnostics;
        snprintf(decision_buf, sizeof(decision_buf), "Alpha-beta  depth %d  nodes %d  prunes %d", sd->depth, sd->nodes, sd->prunes);
        DrawText(decision_buf, rx, ry, tf->small, tc->agent); ry += tf->small + ts->gap_y;
        snprintf(decision_buf, sizeof(decision_buf), "Chosen %s  eval %.2f", action_name(sd->selected_action), sd->value);
        DrawText(decision_buf, rx, ry, tf->small, tc->text_secondary); ry += tf->small + ts->gap_y;
    } else if (s->type == AGENT_MCTS) {
        const SearchDiagnostics* sd = &s->agent.diagnostics;
        snprintf(decision_buf, sizeof(decision_buf), "MCTS  simulations %d  nodes %d", sd->simulations, sd->nodes);
        DrawText(decision_buf, rx, ry, tf->small, tc->agent); ry += tf->small + ts->gap_y;
        snprintf(decision_buf, sizeof(decision_buf), "Chosen %s  value %.2f  visits %d", action_name(sd->selected_action), sd->value, sd->visits[sd->selected_action]);
        DrawText(decision_buf, rx, ry, tf->small, tc->text_secondary); ry += tf->small + ts->gap_y;
    } else if (s->type == AGENT_EXTERNAL) {
        DrawText("Training: external checkpoint | eval via holdout matrix", rx, ry, tf->small, tc->agent); ry += tf->small + ts->gap_y;
    }

    ry += ts->gap_y;

    /* Section: Agent Stats */
    DrawText("Agent Stats", rx, ry, tf->section, tc->text_primary);
    ry += tf->section + ts->gap_y;

    snprintf(decision_buf, sizeof(decision_buf), "Position: (%d, %d)", agent->x, agent->y);
    DrawText(decision_buf, rx, ry, tf->body, tc->text_secondary); ry += tf->body + ts->gap_y;

    snprintf(decision_buf, sizeof(decision_buf), "Status: %s", agent->alive ? "ALIVE" : "DEAD");
    DrawText(decision_buf, rx, ry, tf->body, agent->alive ? tc->positive : tc->danger); ry += tf->body + ts->gap_y;

    snprintf(decision_buf, sizeof(decision_buf), "Bomb Ammo: %d", agent->bomb_ammo);
    DrawText(decision_buf, rx, ry, tf->body, tc->text_secondary); ry += tf->body + ts->gap_y;

    snprintf(decision_buf, sizeof(decision_buf), "Bomb Range: %d", agent->blast_range);
    DrawText(decision_buf, rx, ry, tf->body, tc->text_secondary); ry += tf->body + ts->gap_y;

    snprintf(decision_buf, sizeof(decision_buf), "Score: %d", agent->score);
    DrawText(decision_buf, rx, ry, tf->body, tc->text_secondary); ry += tf->body + ts->gap_y;

    snprintf(decision_buf, sizeof(decision_buf), "Last reward: %.2f", snap.last_reward.total);
    DrawText(decision_buf, rx, ry, tf->body, tc->positive); ry += tf->body + ts->gap_y;

    ry += ts->gap_y;

    /* Section: Enemy Stats */
    if (snap.state.agent_count > 1) {
        const BomberAgentState* enemy = &snap.state.agents[1];
        DrawText("Enemy Stats", rx, ry, tf->section, tc->text_primary);
        ry += tf->section + ts->gap_y;

        snprintf(decision_buf, sizeof(decision_buf), "Position: (%d, %d)", enemy->x, enemy->y);
        DrawText(decision_buf, rx, ry, tf->body, tc->text_secondary); ry += tf->body + ts->gap_y;

        snprintf(decision_buf, sizeof(decision_buf), "Status: %s", enemy->alive ? "ALIVE" : "DEAD");
        DrawText(decision_buf, rx, ry, tf->body, enemy->alive ? tc->danger : tc->positive); ry += tf->body + ts->gap_y;
    }

    ry += ts->gap_y;

    /* Section: Active Bombs */
    DrawText("Active Bombs", rx, ry, tf->section, tc->text_primary);
    ry += tf->section + ts->gap_y;

    int bomb_count = 0;
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (snap.state.bombs[i].active) bomb_count++;
    }

    if (bomb_count == 0) {
        DrawText("No active bombs", rx, ry, tf->body, tc->text_dim); ry += tf->body + ts->gap_y;
    } else {
        snprintf(decision_buf, sizeof(decision_buf), "%d active bomb(s)", bomb_count);
        DrawText(decision_buf, rx, ry, tf->body, tc->warning); ry += tf->body + ts->gap_y;
    }

    ry += ts->gap_y;
    DrawText("Last Reward Breakdown", rx, ry, tf->section, tc->text_primary);
    ry += tf->section + ts->gap_y;
    snprintf(decision_buf, sizeof(decision_buf), "Survival %+.2f   Crate %+.2f",
             snap.last_reward.survival, snap.last_reward.crate_destroyed);
    DrawText(decision_buf, rx, ry, tf->small, tc->text_secondary); ry += tf->small + ts->gap_y;
    snprintf(decision_buf, sizeof(decision_buf), "Powerup %+.2f   Enemy %+.2f",
             snap.last_reward.powerup,
             snap.last_reward.enemy_damage + snap.last_reward.enemy_elimination);
    DrawText(decision_buf, rx, ry, tf->small, tc->text_secondary); ry += tf->small + ts->gap_y;
    snprintf(decision_buf, sizeof(decision_buf), "Penalties %+.2f   Total %+.2f",
             snap.last_reward.invalid_action_penalty + snap.last_reward.suicidal_bomb_penalty +
             snap.last_reward.stall_penalty + snap.last_reward.death_penalty + snap.last_reward.timeout_penalty,
             snap.last_reward.total);
    DrawText(decision_buf, rx, ry, tf->small, tc->text_secondary);

    /* Draw bottom timeline/log panel */
    DrawRectangle(layout.bottom_panel.x, layout.bottom_panel.y, layout.bottom_panel.width, layout.bottom_panel.height, tc->panel);
    DrawRectangleLines(layout.bottom_panel.x, layout.bottom_panel.y, layout.bottom_panel.width, layout.bottom_panel.height, tc->panel_border);

    int bx = layout.bottom_panel.x + ts->padding_x;
    int by = layout.bottom_panel.y + ts->padding_y;

    DrawText("Recent Events", bx, by, tf->section, tc->text_primary);
    by += tf->section + ts->gap_y;

    if (s->dashboard.event_count == 0) {
        DrawText("No events recorded yet. Press SPACE to run.", bx, by, tf->body, tc->text_dim);
    } else {
        int show_count = (layout.bottom_panel.height - tf->section - 2 * ts->padding_y) / (tf->body + ts->gap_y);
        if (show_count > s->dashboard.event_count) show_count = s->dashboard.event_count;

        for (int i = 0; i < show_count; i++) {
            int idx = s->dashboard.event_count - show_count + i;
            DrawText(s->dashboard.events[idx], bx, by, tf->body, tc->text_secondary);
            by += tf->body + ts->gap_y;
        }
    }

    /* Draw observation window if enabled */
    if (vs->show_observation_window) {
        renderer_draw_observation_overlay(&snap, arena_ox, arena_oy, tile_size);
    }
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
    int max_epochs = 1;
    int simulation_hz = 1;
    int target_fps = 60;
    int start_paused = 0;
    const char* replay_file = NULL;
    const char* smoke_screenshot = NULL;
    const char* smoke_view = "matchup";
    const char* smoke_blue = "mcts";
    const char* smoke_red = "random";
    int open_matchup = 0;
    int open_history = 0;
    const char* enemy_name = "heuristic";
    int arena_agent_count = 2;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--agent") == 0 && i + 1 < argc) {
            if (agent_count < 8) agent_names[agent_count++] = argv[++i];
        } else if (strcmp(argv[i], "--enemy") == 0 && i + 1 < argc) {
            enemy_name = argv[++i];
            if (strcmp(enemy_name, "builtin-random") == 0) enemy_name = NULL;
        } else if (strcmp(argv[i], "--agents") == 0 && i + 1 < argc) {
            arena_agent_count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (uint64_t)strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--epochs") == 0 && i + 1 < argc) {
            max_epochs = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            simulation_hz = atoi(argv[++i]);
            if (simulation_hz < 1) simulation_hz = 1;
            if (simulation_hz > 60) simulation_hz = 60;
        } else if (strcmp(argv[i], "--render-fps") == 0 && i + 1 < argc) {
            target_fps = atoi(argv[++i]);
            if (target_fps < 5) target_fps = 5;
            if (target_fps > 1000) target_fps = 1000;
        } else if (strcmp(argv[i], "--start-paused") == 0) {
            start_paused = 1;
        } else if (strcmp(argv[i], "--replay") == 0 && i + 1 < argc) {
            replay_file = argv[++i];
        } else if (strcmp(argv[i], "--smoke-screenshot") == 0 && i + 1 < argc) {
            smoke_screenshot = argv[++i];
        } else if (strcmp(argv[i], "--smoke-view") == 0 && i + 1 < argc) {
            smoke_view = argv[++i];
        } else if (strcmp(argv[i], "--smoke-blue") == 0 && i + 1 < argc) {
            smoke_blue = argv[++i];
        } else if (strcmp(argv[i], "--smoke-red") == 0 && i + 1 < argc) {
            smoke_red = argv[++i];
        } else if (strcmp(argv[i], "--matchup") == 0) {
            open_matchup = 1;
        } else if (strcmp(argv[i], "--history") == 0) {
            open_history = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: bomber_viz [options]\n");
            printf("Options:\n");
            printf("  --agent <type>   Agent: random, scripted, heuristic, greedy, evasive, alpha-beta, mcts\n");
            printf("  --enemy <type>   Opponent policy (default heuristic; use builtin-random explicitly)\n");
            printf("  --agents <n>     Arena agent count, 1-%d (default 2)\n", MAX_AGENTS);
            printf("  --seed <n>       Random seed (default 1337)\n");
            printf("  --epochs <n>     Match count (default 1, then pauses)\n");
            printf("  --fps <n>        Game simulation steps/second (default 1)\n");
            printf("  --render-fps <n> Window render FPS (default 60)\n");
            printf("  --start-paused   Open paused for inspection\n");
            printf("  --replay <file>  Load replay file instead of live mode\n");
            printf("  --smoke-screenshot <png>  Render, capture, and exit (verification)\n");
            printf("  --smoke-view <matchup|history|arena|compare|graphs>  Surface captured by smoke mode\n");
            printf("  --smoke-blue/--smoke-red <policy>  Verification matchup\n");
            printf("  --matchup        Open the live policy arena picker\n");
            printf("  --history        Open the latest match replay/history\n");
            printf("  --help           Show this help\n");
            printf("\nControls:\n");
            printf("  [SPACE]   Pause/Resume\n");
            printf("  [R]       Reset all sessions\n");
            printf("  [S]       Step once (when paused)\n");
            printf("  [+/-]     Game rate preset down/up (1/2/3/5/10/30)\n");
            printf("  [F2/F3]   Game rate down/up\n");
            printf("  [F5/F6]   Render FPS down/up\n");
            printf("  [F7]      Type exact game steps/second\n");
            printf("  [H]       Help and powerup guide\n");
            printf("  [TAB]     Switch active agent\n");
            printf("  [1/2/3/4/5] Arena / Compare / Graphs / Debug / History\n");
            printf("  [M]       Open live policy arena picker\n");
            printf("  [D/O/G/L] Toggle danger / observation / grid / legend\n");
            printf("  [P]       Save screenshots/ai-bomber-arena.png\n");
            printf("  [N]       New epoch for active agent\n");
            printf("  [ESC]     Quit\n");
            return 0;
        }
    }

    if (agent_count == 0) {
        agent_names[agent_count++] = "mcts";
    }

    if (smoke_screenshot) SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(SCREEN_W, SCREEN_H, "AI Bomber - Visualizer");
    SetExitKey(KEY_NULL);
    SetTargetFPS(target_fps);
    renderer_init(SCREEN_W, SCREEN_H);
    theme_init();
    layout_init(SCREEN_W, SCREEN_H);

    BomberConfig cfg;
    config_battle(&cfg);
    cfg.seed = (int)seed;
    cfg.agent_count = arena_agent_count;
    config_normalize(&cfg);

    /* Keep the multi-session dashboards and replay buffers off the Windows
       thread stack; live MCTS adds its own search tree and recursive safety
       proof on the same thread. */
    static VizSession vs;
    viz_session_init(&vs, max_epochs, seed);
    vs.target_fps = target_fps;
    vs.simulation_hz = simulation_hz;
    vs.paused = start_paused;

    if (replay_file) {
        Replay* replay = (Replay*)calloc(1, sizeof(Replay));
        if (replay && replay_load(replay, replay_file)) {
            vs.history.count = 0;
            vs.history.selected = -1;
            vs.history_replay = replay;
            vs.history_frame = 0;
            vs.history_playing = 0;
            vs.view_mode = VIEW_HISTORY;
            vs.paused = 1;
        } else {
            fprintf(stderr, "Failed to load replay: %s\n", replay_file);
            free(replay);
        }
    } else {
        for (int i = 0; i < agent_count; i++) {
            AgentType type = agent_parse_type(agent_names[i]);
            AgentType opponent_type = enemy_name ? agent_parse_type(enemy_name) : AGENT_RANDOM;
            viz_session_add_agent(&vs, type, agent_names[i], opponent_type,
                                  enemy_name ? enemy_name : "built-in-random",
                                  enemy_name != NULL, &cfg);
        }
    }

    if (smoke_screenshot) {
        vs.show_help = 0;
        if (replay_file && vs.history_replay && vs.history_replay->frame_count > 0) {
            vs.show_matchup = 0;
            vs.view_mode = VIEW_HISTORY;
            vs.history_frame = vs.history_replay->frame_count - 1;
            vs.history_playing = 0;
            vs.paused = 1;
        } else if (strcmp(smoke_view, "history") == 0) {
            (void)viz_session_start_match(&vs, agent_parse_type(smoke_blue), agent_parse_type(smoke_red), seed);
            apply_simulation_hz(&vs, &(PlaybackClock){0}, 60);
        } else if (strcmp(smoke_view, "arena") == 0 ||
                   strcmp(smoke_view, "compare") == 0 ||
                   strcmp(smoke_view, "graphs") == 0) {
            vs.show_matchup = 0;
            vs.view_mode = strcmp(smoke_view, "arena") == 0 ? VIEW_ARENA :
                           strcmp(smoke_view, "compare") == 0 ? VIEW_COMPARE : VIEW_GRAPHS;
            vs.paused = 0;
            vs.simulation_hz = 60;
        } else {
            vs.show_matchup = 1;
            vs.paused = 1;
        }
    } else if (open_history) {
        if (vs.history.count > 0) (void)viz_session_load_history(&vs, vs.history.count - 1);
        else { vs.view_mode = VIEW_HISTORY; vs.paused = 1; }
        vs.show_help = 0;
    } else if (open_matchup) {
        vs.show_matchup = 1;
        vs.show_help = 0;
        vs.paused = 1;
    }

    int should_exit = 0;
    int smoke_failed = 0;
    PlaybackClock playback_clock = {0};
    int smoke_ready_frames = 0;
    int fps_editing = 0;
    char fps_entry[8] = {0};
    int fps_entry_len = 0;
    while (!should_exit && !WindowShouldClose()) {
        if (fps_editing) {
            int ch;
            while ((ch = GetCharPressed()) > 0) if (ch >= '0' && ch <= '9' && fps_entry_len < 7) {
                fps_entry[fps_entry_len++] = (char)ch; fps_entry[fps_entry_len] = '\0';
            }
            if (IsKeyPressed(KEY_BACKSPACE) && fps_entry_len > 0) fps_entry[--fps_entry_len] = '\0';
            if (IsKeyPressed(KEY_ENTER) && fps_entry_len > 0) { apply_simulation_hz(&vs, &playback_clock, atoi(fps_entry)); fps_editing = 0; }
            if (IsKeyPressed(KEY_ESCAPE)) fps_editing = 0;
        } else if (IsKeyPressed(KEY_ESCAPE)) should_exit = 1;
        if (IsKeyPressed(KEY_SPACE)) {
            if (vs.view_mode == VIEW_HISTORY) vs.history_playing = !vs.history_playing;
            else vs.paused = !vs.paused;
        }
        if (IsKeyPressed(KEY_R)) viz_session_reset_all(&vs);
        if (IsKeyPressed(KEY_S)) {
            if (vs.view_mode == VIEW_HISTORY) viz_session_history_step(&vs, 1);
            else if (vs.paused) vs.step_once = 1;
        }
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT) || IsKeyPressed(KEY_F2))
            apply_simulation_hz(&vs, &playback_clock, next_simulation_hz(vs.simulation_hz, -1));
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD) || IsKeyPressed(KEY_F3))
            apply_simulation_hz(&vs, &playback_clock, next_simulation_hz(vs.simulation_hz, 1));
        if (IsKeyPressed(KEY_LEFT_BRACKET) || IsKeyPressed(KEY_F5)) apply_render_fps(&vs, next_render_fps(vs.target_fps, -1));
        if (IsKeyPressed(KEY_RIGHT_BRACKET) || IsKeyPressed(KEY_F6)) apply_render_fps(&vs, next_render_fps(vs.target_fps, 1));
        if (IsKeyPressed(KEY_F7)) { fps_editing = 1; fps_entry_len = 0; fps_entry[0] = '\0'; }
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mouse = GetMousePosition();
            if (vs.show_matchup && CheckCollisionPointRec(mouse, (Rectangle){508,220,34,36})) vs.matchup_blue = cycle_policy(vs.matchup_blue, -1);
            else if (vs.show_matchup && CheckCollisionPointRec(mouse, (Rectangle){850,220,34,36})) vs.matchup_blue = cycle_policy(vs.matchup_blue, 1);
            else if (vs.show_matchup && CheckCollisionPointRec(mouse, (Rectangle){508,282,34,36})) vs.matchup_red = cycle_policy(vs.matchup_red, -1);
            else if (vs.show_matchup && CheckCollisionPointRec(mouse, (Rectangle){850,282,34,36})) vs.matchup_red = cycle_policy(vs.matchup_red, 1);
            else if (vs.show_matchup && CheckCollisionPointRec(mouse, (Rectangle){508,342,70,34})) { if (vs.matchup_seed > 0) vs.matchup_seed--; }
            else if (vs.show_matchup && CheckCollisionPointRec(mouse, (Rectangle){850,342,70,34})) vs.matchup_seed++;
            else if (vs.show_matchup && CheckCollisionPointRec(mouse, (Rectangle){508,402,34,36})) { vs.matchup_map_preset = (vs.matchup_map_preset + 2) % 3; }
            else if (vs.show_matchup && CheckCollisionPointRec(mouse, (Rectangle){850,402,34,36})) { vs.matchup_map_preset = (vs.matchup_map_preset + 1) % 3; }
            else if (vs.show_matchup && CheckCollisionPointRec(mouse, (Rectangle){508,462,70,34})) { if (vs.matchup_matches > 1) vs.matchup_matches--; }
            else if (vs.show_matchup && CheckCollisionPointRec(mouse, (Rectangle){850,462,70,34})) { if (vs.matchup_matches < 20) vs.matchup_matches++; }
            else if (vs.show_matchup && CheckCollisionPointRec(mouse, (Rectangle){570,542,260,52})) {
                if (viz_session_start_match(&vs, vs.matchup_blue, vs.matchup_red, vs.matchup_seed)) {
                    playback_clock_reset(&playback_clock);
                    vs.view_mode = VIEW_ARENA;
                    vs.show_matchup = 0;
                    vs.show_help = 0;
                }
            }
            else if (CheckCollisionPointRec(mouse, (Rectangle){575,61,82,22})) apply_simulation_hz(&vs, &playback_clock, next_simulation_hz(vs.simulation_hz, -1));
            else if (CheckCollisionPointRec(mouse, (Rectangle){665,61,82,22})) apply_simulation_hz(&vs, &playback_clock, next_simulation_hz(vs.simulation_hz, 1));
            else if (CheckCollisionPointRec(mouse, (Rectangle){755,61,82,22})) apply_render_fps(&vs, next_render_fps(vs.target_fps, -1));
            else if (CheckCollisionPointRec(mouse, (Rectangle){845,61,82,22})) apply_render_fps(&vs, next_render_fps(vs.target_fps, 1));
            else if (CheckCollisionPointRec(mouse, (Rectangle){935,61,82,22})) { fps_editing = 1; fps_entry_len = 0; fps_entry[0] = '\0'; }
            else if (vs.show_help && CheckCollisionPointRec(mouse, (Rectangle){910,646,126,28})) vs.show_help = 0;
        }
        if (IsKeyPressed(KEY_H)) vs.show_help = !vs.show_help;
        if (IsKeyPressed(KEY_M)) vs.show_matchup = !vs.show_matchup;
        if (IsKeyPressed(KEY_TAB) && vs.session_count > 0) {
            vs.active_session = (vs.active_session + 1) % vs.session_count;
        }
        if (IsKeyPressed(KEY_ONE)) vs.view_mode = VIEW_ARENA;
        if (IsKeyPressed(KEY_TWO)) vs.view_mode = VIEW_COMPARE;
        if (IsKeyPressed(KEY_THREE)) vs.view_mode = VIEW_GRAPHS;
        if (IsKeyPressed(KEY_FOUR)) vs.view_mode = VIEW_DEBUG;
        if (IsKeyPressed(KEY_FIVE)) {
            int selected = vs.history.selected >= 0 ? vs.history.selected : vs.history.count - 1;
            if (selected >= 0) (void)viz_session_load_history(&vs, selected);
            else vs.view_mode = VIEW_HISTORY;
        }
        if (vs.view_mode == VIEW_HISTORY) {
            if (IsKeyPressed(KEY_PAGE_UP) && vs.history.count > 0) {
                int selected = vs.history.selected < 0 ? vs.history.count - 1 : vs.history.selected - 1;
                if (selected < 0) selected = 0;
                (void)viz_session_load_history(&vs, selected);
            }
            if (IsKeyPressed(KEY_PAGE_DOWN) && vs.history.count > 0) {
                int selected = vs.history.selected < 0 ? vs.history.count - 1 : vs.history.selected + 1;
                if (selected >= vs.history.count) selected = vs.history.count - 1;
                (void)viz_session_load_history(&vs, selected);
            }
            if (IsKeyPressed(KEY_LEFT)) viz_session_history_step(&vs, -1);
            if (IsKeyPressed(KEY_RIGHT)) viz_session_history_step(&vs, 1);
            if (IsKeyPressed(KEY_HOME)) vs.history_frame = 0;
        }
        if (IsKeyPressed(KEY_L)) vs.show_legend = !vs.show_legend;
        if (IsKeyPressed(KEY_O)) vs.show_observation_window = !vs.show_observation_window;
        if (IsKeyPressed(KEY_D)) vs.show_danger = !vs.show_danger;
        if (IsKeyPressed(KEY_G)) vs.show_grid = !vs.show_grid;
        static int screenshot_toast_frames = 0;
        if (IsKeyPressed(KEY_P)) {
            ensure_screenshot_dir();
            TakeScreenshot("screenshots/ai-bomber-arena.png");
            screenshot_toast_frames = 150;
        }
        if (IsKeyPressed(KEY_N)) {
            AgentSession* s = viz_session_active(&vs);
            if (s && s->epoch_count < vs.max_epochs) {
                /* Record current epoch and start new one */
                s->episode_done = 1;
                /* Manually record then reset */
                /* record_epoch is static, so we use the step mechanism */
            }
        }

        int simulation_steps = 0;
        if (vs.step_once) {
            simulation_steps = 1;
            vs.step_once = 0;
            playback_clock_reset(&playback_clock);
        } else {
            simulation_steps = playback_clock_advance(&playback_clock, GetFrameTime(),
                                                      vs.simulation_hz, vs.paused);
        }
        for (int step = 0; step < simulation_steps; step++) {
            if (vs.view_mode == VIEW_HISTORY && vs.history_playing) viz_session_history_step(&vs, 1);
            else if (vs.view_mode != VIEW_HISTORY) viz_session_step(&vs);
        }
        if (smoke_screenshot && strcmp(smoke_view, "history") == 0 && vs.view_mode != VIEW_HISTORY) {
            AgentSession* smoke_session = viz_session_active(&vs);
            if (smoke_session && smoke_session->episode_done && smoke_session->history_recorded && vs.history.count > 0) {
                if (viz_session_load_history(&vs, vs.history.count - 1) && vs.history_replay) {
                    uint64_t replay_hash = 0;
                    uint64_t expected_hash = vs.history.entries[vs.history.count - 1].terminal_hash;
                    if (!replay_validate(vs.history_replay, &replay_hash) || replay_hash != expected_hash) {
                        fprintf(stderr, "Smoke replay validation failed: expected %llu, got %llu\n",
                                (unsigned long long)expected_hash, (unsigned long long)replay_hash);
                        smoke_failed = 1;
                        should_exit = 1;
                    } else {
                        vs.history_frame = vs.history_replay->frame_count / 2;
                    }
                }
            }
        }

        BeginDrawing();
        ClearBackground((Color){15, 15, 20, 255});

        if (vs.view_mode != VIEW_ARENA && vs.view_mode != VIEW_HISTORY && vs.session_count > 0)
            draw_agent_tabs(&vs, 8, 4, SCREEN_W - 16);

        switch (vs.view_mode) {
            case VIEW_ARENA:       draw_arena_view(&vs, SCREEN_W, SCREEN_H); break;
            case VIEW_COMPARE:     draw_comparison_view(&vs, SCREEN_W, SCREEN_H); break;
            case VIEW_GRAPHS:      draw_graphs_view(&vs, SCREEN_W, SCREEN_H); break;
            case VIEW_DEBUG:       draw_graphs_view(&vs, SCREEN_W, SCREEN_H); break;
            case VIEW_HISTORY:     draw_history_view(&vs, SCREEN_W, SCREEN_H); break;
        }
        if (vs.show_help) draw_help_overlay(SCREEN_W, SCREEN_H);
        if (vs.show_matchup) draw_matchup_overlay(&vs);
        if (vs.view_mode != VIEW_HISTORY) draw_runtime_controls(&vs, fps_editing, fps_entry);

        if (screenshot_toast_frames > 0) {
            DrawRectangle(SCREEN_W - 180, SCREEN_H - 48, 160, 30, (Color){18, 28, 38, 235});
            DrawText("Screenshot saved", SCREEN_W - 165, SCREEN_H - 40, 14, (Color){115, 225, 160, 255});
            screenshot_toast_frames--;
        }

        EndDrawing();
        if (smoke_screenshot) {
            int live_view_smoke = strcmp(smoke_view, "arena") == 0 ||
                                  strcmp(smoke_view, "compare") == 0 ||
                                  strcmp(smoke_view, "graphs") == 0;
            AgentSession* smoke_active = viz_session_active(&vs);
            int ready = live_view_smoke
                ? (smoke_active && smoke_active->current_step >= 3)
                : (strcmp(smoke_view, "history") != 0 ||
                   (vs.view_mode == VIEW_HISTORY && vs.history_replay && vs.history_replay->frame_count > 0));
            if (ready && ++smoke_ready_frames >= 3) {
                TakeScreenshot(smoke_screenshot);
                should_exit = 1;
            }
        }
    }

    viz_session_shutdown(&vs);
    if (IsWindowReady()) CloseWindow();
    return smoke_failed ? 1 : 0;
}
