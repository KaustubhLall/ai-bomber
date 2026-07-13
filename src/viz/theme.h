#ifndef BOMBER_THEME_H
#define BOMBER_THEME_H

#include "raylib.h"

/* Color palette */
typedef struct {
    /* Background colors */
    Color background;
    Color panel;
    Color panel_border;

    /* Text colors */
    Color text_primary;
    Color text_secondary;
    Color text_dim;

    /* Status colors */
    Color positive;
    Color danger;
    Color warning;

    /* Entity colors */
    Color agent;
    Color enemy;
    Color crate;

    /* Tile colors */
    Color floor;
    Color solid_wall;
    Color destructible_wall;

    /* Danger overlay colors */
    Color danger_now;
    Color danger_soon;
    Color safe_reachable;

    /* Observation window */
    Color observation_border;
    Color observation_fill;

    /* UI element colors */
    Color button;
    Color button_hover;
    Color button_active;

    /* Grid lines */
    Color grid;
} ThemeColors;

/* Font sizes */
typedef struct {
    int header;        /* 20-24 */
    int section;       /* 16-18 */
    int body;          /* 13-15 */
    int small;         /* 10-12 */
    int tiny;          /* 8-9 */
} ThemeFonts;

/* Spacing */
typedef struct {
    int padding_x;
    int padding_y;
    int margin_x;
    int margin_y;
    int gap_x;
    int gap_y;
    int panel_border;
} ThemeSpacing;

/* Complete theme */
typedef struct {
    ThemeColors colors;
    ThemeFonts fonts;
    ThemeSpacing spacing;
} Theme;

/* Global theme instance */
extern Theme g_theme;

/* Initialize theme with default values */
void theme_init(void);

/* Get theme colors */
const ThemeColors* theme_colors(void);

/* Get theme fonts */
const ThemeFonts* theme_fonts(void);

/* Get theme spacing */
const ThemeSpacing* theme_spacing(void);

#endif /* BOMBER_THEME_H */
