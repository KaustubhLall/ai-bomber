#ifndef BOMBER_MATH_UTIL_H
#define BOMBER_MATH_UTIL_H

static inline int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int absi(int v) {
    return v < 0 ? -v : v;
}

static inline int mini(int a, int b) {
    return a < b ? a : b;
}

static inline int maxi(int a, int b) {
    return a > b ? a : b;
}

static inline int dist_manhattan(int x0, int y0, int x1, int y1) {
    return absi(x1 - x0) + absi(y1 - y0);
}

#endif /* BOMBER_MATH_UTIL_H */
