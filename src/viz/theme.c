#include "theme.h"

Theme g_theme;

void theme_init(void) {
    /* Background colors - near-black, not pure black */
    g_theme.colors.background = (Color){15, 15, 20, 255};
    g_theme.colors.panel = (Color){25, 30, 40, 255};
    g_theme.colors.panel_border = (Color){60, 70, 85, 255};
    
    /* Text colors */
    g_theme.colors.text_primary = (Color){230, 230, 235, 255};
    g_theme.colors.text_secondary = (Color){160, 165, 175, 255};
    /* ~3.0-3.5:1 contrast against background/panel at (100,105,115) fails WCAG AA's 4.5:1 for
       normal text, yet this color is used for real informational text (event log, "no active
       bombs", decision trace) at 11-15px — small text needs MORE contrast, not less. Lightened
       to keep a dimmer feel than text_secondary while staying legible on the dark panels. */
    g_theme.colors.text_dim = (Color){140, 145, 155, 255};
    
    /* Status colors */
    g_theme.colors.positive = (Color){80, 200, 120, 255};
    g_theme.colors.danger = (Color){220, 80, 80, 255};
    g_theme.colors.warning = (Color){220, 180, 80, 255};
    
    /* Entity colors */
    g_theme.colors.agent = (Color){80, 140, 220, 255};
    g_theme.colors.enemy = (Color){220, 80, 80, 255};
    g_theme.colors.crate = (Color){180, 140, 100, 255};
    
    /* Tile colors */
    g_theme.colors.floor = (Color){40, 45, 55, 255};
    g_theme.colors.solid_wall = (Color){20, 25, 35, 255};
    g_theme.colors.destructible_wall = (Color){140, 110, 80, 255};
    
    /* Danger overlay colors */
    g_theme.colors.danger_now = (Color){200, 60, 60, 180};
    g_theme.colors.danger_soon = (Color){200, 140, 60, 120};
    g_theme.colors.safe_reachable = (Color){80, 180, 120, 100};
    
    /* Observation window */
    g_theme.colors.observation_border = (Color){220, 200, 80, 255};
    g_theme.colors.observation_fill = (Color){220, 200, 80, 30};
    
    /* UI element colors */
    g_theme.colors.button = (Color){60, 70, 90, 255};
    g_theme.colors.button_hover = (Color){80, 90, 110, 255};
    g_theme.colors.button_active = (Color){100, 110, 130, 255};
    
    /* Grid lines */
    g_theme.colors.grid = (Color){50, 55, 65, 100};
    
    /* Font sizes */
    g_theme.fonts.header = 22;
    g_theme.fonts.section = 17;
    g_theme.fonts.body = 14;
    g_theme.fonts.small = 11;
    g_theme.fonts.tiny = 9;
    
    /* Spacing */
    g_theme.spacing.padding_x = 12;
    g_theme.spacing.padding_y = 10;
    g_theme.spacing.margin_x = 8;
    g_theme.spacing.margin_y = 8;
    g_theme.spacing.gap_x = 8;
    g_theme.spacing.gap_y = 6;
    g_theme.spacing.panel_border = 1;
}

const ThemeColors* theme_colors(void) {
    return &g_theme.colors;
}

const ThemeFonts* theme_fonts(void) {
    return &g_theme.fonts;
}

const ThemeSpacing* theme_spacing(void) {
    return &g_theme.spacing;
}
