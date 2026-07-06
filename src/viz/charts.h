#ifndef BOMBER_CHARTS_H
#define BOMBER_CHARTS_H

#include "raylib.h"

void charts_draw_line(const float* data, int count, int ox, int oy, int w, int h,
                      float min_val, float max_val, Color color);
void charts_draw_bars(const int* data, int count, int ox, int oy, int w, int h,
                      const char* labels[], Color colors[]);

#endif /* BOMBER_CHARTS_H */
