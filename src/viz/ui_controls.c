#include "viz/ui_controls.h"
#include <string.h>

void button_init(Button* btn, int x, int y, int w, int h, const char* label) {
    btn->x = x; btn->y = y; btn->w = w; btn->h = h;
    btn->label = label;
    btn->hovered = 0;
    btn->pressed = 0;
}

int button_update(Button* btn) {
    Vector2 mouse = GetMousePosition();
    btn->hovered = (mouse.x >= btn->x && mouse.x <= btn->x + btn->w &&
                    mouse.y >= btn->y && mouse.y <= btn->y + btn->h);
    btn->pressed = btn->hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    return btn->pressed;
}

void button_draw(const Button* btn) {
    Color bg = btn->hovered ? (Color){60, 60, 80, 255} : (Color){40, 40, 50, 255};
    DrawRectangle(btn->x, btn->y, btn->w, btn->h, bg);
    DrawRectangleLines(btn->x, btn->y, btn->w, btn->h,
                       btn->hovered ? WHITE : (Color){100, 100, 120, 255});
    int text_w = MeasureText(btn->label, 14);
    DrawText(btn->label, btn->x + (btn->w - text_w) / 2, btn->y + (btn->h - 14) / 2, 14, WHITE);
}

void slider_init(Slider* s, int x, int y, int w, int h, int min_val, int max_val, int value) {
    s->x = x; s->y = y; s->w = w; s->h = h;
    s->min_val = min_val; s->max_val = max_val; s->value = value;
}

int slider_update(Slider* s) {
    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) &&
        mouse.x >= s->x && mouse.x <= s->x + s->w &&
        mouse.y >= s->y - 4 && mouse.y <= s->y + s->h + 4) {
        float frac = (float)(mouse.x - s->x) / s->w;
        if (frac < 0) frac = 0;
        if (frac > 1) frac = 1;
        int new_val = s->min_val + (int)(frac * (s->max_val - s->min_val));
        if (new_val != s->value) {
            s->value = new_val;
            return 1;
        }
    }
    return 0;
}

void slider_draw(const Slider* s) {
    DrawRectangle(s->x, s->y + s->h / 2 - 2, s->w, 4, (Color){60, 60, 70, 255});
    float frac = (float)(s->value - s->min_val) / (s->max_val - s->min_val);
    int knob_x = s->x + (int)(frac * s->w);
    DrawCircle(knob_x, s->y + s->h / 2, 8, (Color){100, 180, 255, 255});
    DrawCircleLines(knob_x, s->y + s->h / 2, 8, WHITE);
}
