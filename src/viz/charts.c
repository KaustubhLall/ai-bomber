#include "viz/charts.h"

void charts_draw_line(const float* data, int count, int ox, int oy, int w, int h,
                      float min_val, float max_val, Color color) {
    if (count < 2) return;
    float range = max_val - min_val;
    if (range < 0.001f) range = 1.0f;

    for (int i = 1; i < count; i++) {
        int x0 = ox + (i - 1) * w / (count - 1);
        int x1 = ox + i * w / (count - 1);
        int y0 = oy + h - (int)((data[i - 1] - min_val) / range * h);
        int y1 = oy + h - (int)((data[i] - min_val) / range * h);
        DrawLine(x0, y0, x1, y1, color);
    }
}

void charts_draw_bars(const int* data, int count, int ox, int oy, int w, int h,
                      const char* labels[], Color colors[]) {
    int bar_w = w / count;
    int max_val = 1;
    for (int i = 0; i < count; i++) {
        if (data[i] > max_val) max_val = data[i];
    }

    for (int i = 0; i < count; i++) {
        int bar_h = (int)((float)data[i] / max_val * h);
        int x = ox + i * bar_w;
        int y = oy + h - bar_h;
        DrawRectangle(x + 2, y, bar_w - 4, bar_h, colors[i]);
        DrawRectangleLines(x + 2, y, bar_w - 4, bar_h, WHITE);
        if (labels) {
            DrawText(labels[i], x + 4, oy + h + 2, 10, WHITE);
        }
    }
}
