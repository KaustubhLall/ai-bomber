#ifndef BOMBER_LAYOUT_H
#define BOMBER_LAYOUT_H

#include "raylib.h"

/* Layout regions for the main arena view */
typedef struct {
    /* Top bar */
    Rectangle top_bar;
    
    /* Main arena area */
    Rectangle arena;
    
    /* Left panel (optional mini-maps) */
    Rectangle left_panel;
    
    /* Right inspector panel */
    Rectangle right_panel;
    
    /* Bottom timeline/log */
    Rectangle bottom_panel;
    
    /* Legend position */
    Rectangle legend;
} ArenaLayout;

/* Layout for comparison view */
typedef struct {
    Rectangle overview_panel;
    Rectangle arena_area;
    int cols;
    int rows;
} ComparisonLayout;

/* Layout for graphs view */
typedef struct {
    Rectangle reward_panel;
    Rectangle avg_reward_panel;
    Rectangle action_dist_panel;
    Rectangle overview_panel;
} GraphsLayout;

/* Layout for debug view */
typedef struct {
    Rectangle arena;
    Rectangle obs_panel;
    Rectangle danger_panel;
    Rectangle event_log;
} DebugLayout;

/* Initialize layout based on screen size */
void layout_init(int screen_w, int screen_h);

/* Get arena layout for current screen size */
ArenaLayout layout_get_arena(void);

/* Get comparison layout */
ComparisonLayout layout_get_comparison(int session_count);

/* Get graphs layout */
GraphsLayout layout_get_graphs(void);

/* Get debug layout */
DebugLayout layout_get_debug(void);

/* Update layout when window is resized */
void layout_update(int screen_w, int screen_h);

/* Helper: calculate centered rectangle */
Rectangle layout_center_rect(int w, int h, int container_w, int container_h);

/* Helper: calculate responsive tile size for arena */
int layout_calc_tile_size(int arena_w, int arena_h, int available_w, int available_h);

#endif /* BOMBER_LAYOUT_H */
