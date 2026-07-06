#ifndef BOMBER_UI_CONTROLS_H
#define BOMBER_UI_CONTROLS_H

#include "raylib.h"

typedef struct {
    int x, y, w, h;
    const char* label;
    int hovered;
    int pressed;
} Button;

void button_init(Button* btn, int x, int y, int w, int h, const char* label);
int button_update(Button* btn);
void button_draw(const Button* btn);

typedef struct {
    int x, y, w, h;
    int min_val, max_val, value;
} Slider;

void slider_init(Slider* s, int x, int y, int w, int h, int min_val, int max_val, int value);
int slider_update(Slider* s);
void slider_draw(const Slider* s);

#endif /* BOMBER_UI_CONTROLS_H */
