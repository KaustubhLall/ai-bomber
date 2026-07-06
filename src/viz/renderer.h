#ifndef BOMBER_RENDERER_H
#define BOMBER_RENDERER_H

#include "raylib.h"
#include "env/env.h"
#include "env/bomber_observation.h"
#include "viz/viz_session.h"

#define TILE_SIZE 32

typedef struct {
    int x;
    int y;
    int width;
    int height;
} PanelRect;

void renderer_init(int screen_w, int screen_h);
void renderer_draw_arena(const DebugSnapshot* snap, int ox, int oy, int tile_size);
void renderer_draw_danger_map(const DebugSnapshot* snap, int ox, int oy, int panel_w, int panel_h);
void renderer_draw_local_obs(const Observation* obs, int ox, int oy, int cell_size);
void renderer_draw_bomb_timeline(const DebugSnapshot* snap, int ox, int oy, int w);
void renderer_draw_reward_graph(const float* rewards, int count, int ox, int oy, int w, int h, float max_reward);
void renderer_draw_action_dist(const int* counts, int total, int ox, int oy, int w, int h);
void renderer_draw_event_log(const char* events[], int event_count, int ox, int oy, int w, int h);
void renderer_draw_decision_trace(const char* text, int ox, int oy, int w, int h);
void renderer_draw_status_panel(const DebugSnapshot* snap, int ox, int oy, int w, int h);
void renderer_draw_controls(int ox, int oy, int w, int h, int paused, int speed_mult);

/* New: multi-epoch / multi-agent views */
void renderer_draw_epoch_graph(const AgentSession* s, int ox, int oy, int w, int h,
                               const char* title);
void renderer_draw_multi_epoch_graph(const VizSession* vs, int ox, int oy, int w, int h,
                                     const char* title, int show_avg);
void renderer_draw_winrate_graph(const VizSession* vs, int ox, int oy, int w, int h);
void renderer_draw_comparison_arenas(const VizSession* vs, int ox, int oy, int screen_w, int screen_h);
void renderer_draw_agent_selector(const VizSession* vs, int ox, int oy, int w);
void renderer_draw_session_stats(const AgentSession* s, int ox, int oy, int w, int h);
void renderer_draw_view_controls(const VizSession* vs, int ox, int oy, int w, int h);
void renderer_draw_mini_arena(const DebugSnapshot* snap, int ox, int oy, int cell_size);

Color tile_color(TileType t);
Color agent_color(int agent_id);
Color session_color(int session_idx);

#endif /* BOMBER_RENDERER_H */
