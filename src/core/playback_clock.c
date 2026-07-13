#include "core/playback_clock.h"

void playback_clock_reset(PlaybackClock* clock) {
    if (clock) clock->accumulator = 0.0;
}

int playback_clock_advance(PlaybackClock* clock, double elapsed_seconds,
                           double simulation_steps_per_second, int paused) {
    if (!clock || paused || simulation_steps_per_second <= 0.0) return 0;
    if (elapsed_seconds < 0.0) elapsed_seconds = 0.0;
    if (elapsed_seconds > 0.25) elapsed_seconds = 0.25;
    clock->accumulator += elapsed_seconds * simulation_steps_per_second;
    int steps = (int)(clock->accumulator + 1e-9);
    if (steps > 8) steps = 8;
    clock->accumulator -= steps;
    if (clock->accumulator < 0.0) clock->accumulator = 0.0;
    return steps;
}
