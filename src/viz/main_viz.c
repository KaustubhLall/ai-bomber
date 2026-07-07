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

static int next_fps(int current, int direction) {
    static const int choices[] = {15, 30, 60, 120, 240};
    int idx = 2;
    for (int i = 0; i < 5; i++) if (choices[i] == current) idx = i;
    idx += direction;
    if (idx < 0) idx = 0;
    if (idx > 4) idx = 4;
    return choices[idx];
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
    DrawText("+/- simulation steps per frame    [/] render FPS (15/30/60/120/240)", x + 24, y + 116, 15, tc->text_secondary);
    DrawText("1-4 views    D danger    O observation    G grid    L legend    P screenshot", x + 24, y + 142, 15, tc->text_secondary);
    DrawText("H close help    ESC exit cleanly", x + 24, y + 168, 15, tc->text_secondary);
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
    DrawText("Press H to return", x + w - 150, y + h - 34, 14, tc->text_dim);
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
    snprintf(status_buf, sizeof(status_buf), "Ep: %d/%d | Step: %d | Sim: %dx | FPS: %d | %s",
             s->epoch_count, vs->max_epochs, s->current_step, vs->speed_mult,
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
    int max_epochs = 500;
    int target_fps = 60;
    int start_paused = 0;
    const char* replay_file = NULL;
    const char* enemy_name = NULL;
    int arena_agent_count = 2;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--agent") == 0 && i + 1 < argc) {
            if (agent_count < 8) agent_names[agent_count++] = argv[++i];
        } else if (strcmp(argv[i], "--enemy") == 0 && i + 1 < argc) {
            enemy_name = argv[++i];
        } else if (strcmp(argv[i], "--agents") == 0 && i + 1 < argc) {
            arena_agent_count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (uint64_t)strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--epochs") == 0 && i + 1 < argc) {
            max_epochs = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--fps") == 0 && i + 1 < argc) {
            target_fps = atoi(argv[++i]);
            if (target_fps != 15 && target_fps != 30 && target_fps != 60 && target_fps != 120 && target_fps != 240) target_fps = 60;
        } else if (strcmp(argv[i], "--start-paused") == 0) {
            start_paused = 1;
        } else if (strcmp(argv[i], "--replay") == 0 && i + 1 < argc) {
            replay_file = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: bomber_viz [options]\n");
            printf("Options:\n");
            printf("  --agent <type>   Agent type: random, scripted, heuristic, greedy, alpha-beta, mcts\n");
            printf("  --enemy <type>   Shared opponent policy; omit for built-in-random\n");
            printf("  --agents <n>     Arena agent count, 1-%d (default 2)\n", MAX_AGENTS);
            printf("  --seed <n>       Random seed (default 1337)\n");
            printf("  --epochs <n>     Max epochs to run (default 500)\n");
            printf("  --fps <n>        Render FPS: 15, 30, 60, 120, or 240\n");
            printf("  --start-paused   Open paused for inspection\n");
            printf("  --replay <file>  Load replay file instead of live mode\n");
            printf("  --help           Show this help\n");
            printf("\nControls:\n");
            printf("  [SPACE]   Pause/Resume\n");
            printf("  [R]       Reset all sessions\n");
            printf("  [S]       Step once (when paused)\n");
            printf("  [+/-]     Speed up/down\n");
            printf("  [[/]]     Render FPS down/up\n");
            printf("  [H]       Help and powerup guide\n");
            printf("  [TAB]     Switch active agent\n");
            printf("  [1/2/3/4] Switch view: Arena / Compare / Graphs / Debug\n");
            printf("  [D/O/G/L] Toggle danger / observation / grid / legend\n");
            printf("  [P]       Save screenshots/ai-bomber-arena.png\n");
            printf("  [N]       New epoch for active agent\n");
            printf("  [ESC]     Quit\n");
            return 0;
        }
    }

    if (agent_count == 0) {
        agent_names[agent_count++] = "mcts";
        agent_names[agent_count++] = "heuristic";
        agent_names[agent_count++] = "greedy";
        agent_names[agent_count++] = "alpha-beta";
        agent_names[agent_count++] = "random";
    }

    InitWindow(SCREEN_W, SCREEN_H, "AI Bomber - Visualizer");
    SetExitKey(KEY_NULL);
    SetTargetFPS(target_fps);
    renderer_init(SCREEN_W, SCREEN_H);
    theme_init();
    layout_init(SCREEN_W, SCREEN_H);

    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = (int)seed;
    cfg.agent_count = arena_agent_count;
    config_normalize(&cfg);

    VizSession vs;
    viz_session_init(&vs, max_epochs, seed);
    vs.target_fps = target_fps;
    vs.paused = start_paused;

    if (replay_file) {
        Replay* replay = (Replay*)calloc(1, sizeof(Replay));
        if (replay_load(replay, replay_file)) {
            cfg = replay->config;
            viz_session_add_agent(&vs, AGENT_RANDOM, "replay", AGENT_RANDOM,
                                  "built-in-random", 0, &cfg);
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
            AgentType opponent_type = enemy_name ? agent_parse_type(enemy_name) : AGENT_RANDOM;
            viz_session_add_agent(&vs, type, agent_names[i], opponent_type,
                                  enemy_name ? enemy_name : "built-in-random",
                                  enemy_name != NULL, &cfg);
        }
    }

    int should_exit = 0;
    while (!should_exit && !WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE)) should_exit = 1;
        if (IsKeyPressed(KEY_SPACE)) vs.paused = !vs.paused;
        if (IsKeyPressed(KEY_R)) viz_session_reset_all(&vs);
        if (IsKeyPressed(KEY_S) && vs.paused) vs.step_once = 1;
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
            vs.speed_mult = (vs.speed_mult * 2 > 16) ? 16 : vs.speed_mult * 2;
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
            vs.speed_mult = (vs.speed_mult / 2 < 1) ? 1 : vs.speed_mult / 2;
        if (IsKeyPressed(KEY_LEFT_BRACKET)) { vs.target_fps = next_fps(vs.target_fps, -1); SetTargetFPS(vs.target_fps); }
        if (IsKeyPressed(KEY_RIGHT_BRACKET)) { vs.target_fps = next_fps(vs.target_fps, 1); SetTargetFPS(vs.target_fps); }
        if (IsKeyPressed(KEY_H)) vs.show_help = !vs.show_help;
        if (IsKeyPressed(KEY_TAB)) {
            vs.active_session = (vs.active_session + 1) % vs.session_count;
        }
        if (IsKeyPressed(KEY_ONE)) vs.view_mode = VIEW_ARENA;
        if (IsKeyPressed(KEY_TWO)) vs.view_mode = VIEW_COMPARE;
        if (IsKeyPressed(KEY_THREE)) vs.view_mode = VIEW_GRAPHS;
        if (IsKeyPressed(KEY_FOUR)) vs.view_mode = VIEW_DEBUG;
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

        if (!vs.paused || vs.step_once) {
            vs.step_once = 0;
            viz_session_step(&vs);
        }

        BeginDrawing();
        ClearBackground((Color){15, 15, 20, 255});

        if (vs.view_mode != VIEW_ARENA) draw_agent_tabs(&vs, 8, 4, SCREEN_W - 16);

        switch (vs.view_mode) {
            case VIEW_ARENA:       draw_arena_view(&vs, SCREEN_W, SCREEN_H); break;
            case VIEW_COMPARE:     draw_comparison_view(&vs, SCREEN_W, SCREEN_H); break;
            case VIEW_GRAPHS:      draw_graphs_view(&vs, SCREEN_W, SCREEN_H); break;
            case VIEW_DEBUG:       draw_graphs_view(&vs, SCREEN_W, SCREEN_H); break;
        }
        if (vs.show_help) draw_help_overlay(SCREEN_W, SCREEN_H);

        if (screenshot_toast_frames > 0) {
            DrawRectangle(SCREEN_W - 180, SCREEN_H - 48, 160, 30, (Color){18, 28, 38, 235});
            DrawText("Screenshot saved", SCREEN_W - 165, SCREEN_H - 40, 14, (Color){115, 225, 160, 255});
            screenshot_toast_frames--;
        }

        EndDrawing();
    }

    if (IsWindowReady()) CloseWindow();
    return 0;
}
