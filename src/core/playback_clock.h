#ifndef BOMBER_PLAYBACK_CLOCK_H
#define BOMBER_PLAYBACK_CLOCK_H

typedef struct {
    double accumulator;
} PlaybackClock;

void playback_clock_reset(PlaybackClock* clock);
int playback_clock_advance(PlaybackClock* clock, double elapsed_seconds,
                           double simulation_steps_per_second, int paused);

#endif
