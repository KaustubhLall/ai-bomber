#include "layout.h"
#include "theme.h"
#include <math.h>

static int g_screen_w = 1400;
static int g_screen_h = 860;

void layout_init(int screen_w, int screen_h) {
    g_screen_w = screen_w;
    g_screen_h = screen_h;
}

void layout_update(int screen_w, int screen_h) {
    g_screen_w = screen_w;
    g_screen_h = screen_h;
}

ArenaLayout layout_get_arena(void) {
    ArenaLayout layout;
    const ThemeSpacing* sp = theme_spacing();
    
    /* Top bar */
    layout.top_bar = (Rectangle){
        0, 0,
        g_screen_w,
        58
    };
    
    /* Calculate available space after top bar */
    int available_h = g_screen_h - layout.top_bar.height - sp->margin_y;
    
    /* Right inspector panel (fixed width, responsive to screen) */
    int right_w = (g_screen_w < 1280) ? 260 : 320;
    layout.right_panel = (Rectangle){
        g_screen_w - right_w - sp->margin_x,
        layout.top_bar.height + sp->margin_y,
        right_w,
        available_h
    };
    
    /* Bottom timeline/log */
    int bottom_h = 62;
    layout.bottom_panel = (Rectangle){
        sp->margin_x,
        g_screen_h - bottom_h - sp->margin_y,
        g_screen_w - right_w - 2 * sp->margin_x,
        bottom_h
    };
    
    /* Left panel (optional mini-maps, can be toggled) */
    int left_w = 0;
    layout.left_panel = (Rectangle){
        sp->margin_x,
        layout.top_bar.height + sp->margin_y,
        left_w,
        available_h - bottom_h - sp->gap_y
    };
    
    /* Main arena - takes remaining space */
    int arena_x = sp->margin_x;
    int arena_y = layout.top_bar.height + sp->margin_y;
    int arena_w = g_screen_w - layout.right_panel.width - 3 * sp->margin_x;
    int arena_h = available_h - bottom_h - sp->gap_y;
    
    layout.arena = (Rectangle){
        arena_x,
        arena_y,
        arena_w,
        arena_h
    };
    
    /* Legend - small unobtrusive box in bottom-left of arena */
    layout.legend = (Rectangle){
        arena_x + sp->margin_x,
        arena_y + arena_h - 118,
        220,
        110
    };
    
    return layout;
}

ComparisonLayout layout_get_comparison(int session_count) {
    ComparisonLayout layout;
    const ThemeSpacing* sp = theme_spacing();
    
    /* Overview panel on the left */
    int overview_w = (g_screen_w < 1280) ? 350 : 400;
    layout.overview_panel = (Rectangle){
        sp->margin_x,
        40 + sp->margin_y,
        overview_w,
        g_screen_h - 40 - 2 * sp->margin_y
    };
    
    /* Arena area */
    layout.arena_area = (Rectangle){
        overview_w + 2 * sp->margin_x,
        40 + sp->margin_y,
        g_screen_w - overview_w - 3 * sp->margin_x,
        g_screen_h - 40 - 2 * sp->margin_y
    };
    
    /* Calculate grid for side-by-side arenas */
    layout.cols = (session_count > 2) ? 3 : 2;
    if (layout.cols < 1) layout.cols = 1;
    layout.rows = (session_count + layout.cols - 1) / layout.cols;
    if (layout.rows < 1) layout.rows = 1;
    
    return layout;
}

GraphsLayout layout_get_graphs(void) {
    GraphsLayout layout;
    const ThemeSpacing* sp = theme_spacing();
    
    int panel_w = (g_screen_w - 3 * sp->margin_x) / 2;
    int panel_h = (g_screen_h - 40 - 3 * sp->margin_y) / 2;
    
    /* Reward per epoch */
    layout.reward_panel = (Rectangle){
        sp->margin_x,
        40 + sp->margin_y,
        panel_w,
        panel_h
    };
    
    /* Running average reward */
    layout.avg_reward_panel = (Rectangle){
        panel_w + 2 * sp->margin_x,
        40 + sp->margin_y,
        panel_w,
        panel_h
    };
    
    /* Action distribution */
    layout.action_dist_panel = (Rectangle){
        sp->margin_x,
        40 + panel_h + 2 * sp->margin_y,
        panel_w,
        panel_h
    };
    
    /* Training overview */
    layout.overview_panel = (Rectangle){
        panel_w + 2 * sp->margin_x,
        40 + panel_h + 2 * sp->margin_y,
        panel_w,
        panel_h
    };
    
    return layout;
}

DebugLayout layout_get_debug(void) {
    DebugLayout layout;
    const ThemeSpacing* sp = theme_spacing();
    
    /* Main arena takes center */
    int arena_w = g_screen_w / 2;
    int arena_h = g_screen_h - 40 - 2 * sp->margin_y;
    
    layout.arena = (Rectangle){
        (g_screen_w - arena_w) / 2,
        40 + sp->margin_y,
        arena_w,
        arena_h
    };
    
    /* Observation panel (left) */
    layout.obs_panel = (Rectangle){
        sp->margin_x,
        40 + sp->margin_y,
        (g_screen_w - arena_w) / 2 - 2 * sp->margin_x,
        arena_h / 2 - sp->gap_y
    };
    
    /* Danger panel (left, below obs) */
    layout.danger_panel = (Rectangle){
        sp->margin_x,
        40 + sp->margin_y + arena_h / 2 + sp->gap_y,
        (g_screen_w - arena_w) / 2 - 2 * sp->margin_x,
        arena_h / 2 - sp->gap_y
    };
    
    /* Event log (right) */
    layout.event_log = (Rectangle){
        layout.arena.x + layout.arena.width + sp->margin_x,
        40 + sp->margin_y,
        (g_screen_w - arena_w) / 2 - 2 * sp->margin_x,
        arena_h
    };
    
    return layout;
}

Rectangle layout_center_rect(int w, int h, int container_w, int container_h) {
    return (Rectangle){
        (container_w - w) / 2,
        (container_h - h) / 2,
        w,
        h
    };
}

int layout_calc_tile_size(int arena_w, int arena_h, int available_w, int available_h) {
    int tile_w = available_w / arena_w;
    int tile_h = available_h / arena_h;
    int tile = (tile_w < tile_h) ? tile_w : tile_h;
    
    /* Clamp to reasonable range */
    if (tile < 16) tile = 16;
    if (tile > 64) tile = 64;
    
    return tile;
}
