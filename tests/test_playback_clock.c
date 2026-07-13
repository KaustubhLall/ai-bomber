#include "core/playback_clock.h"
#include <assert.h>
#include <stdio.h>

static int simulate_one_second(int render_fps, int simulation_hz) {
    PlaybackClock clock = {0};
    int steps = 0;
    for (int frame = 0; frame < render_fps; frame++)
        steps += playback_clock_advance(&clock, 1.0 / render_fps, simulation_hz, 0);
    return steps;
}

int main(void) {
    assert(simulate_one_second(30, 1) == 1);
    assert(simulate_one_second(60, 1) == 1);
    assert(simulate_one_second(120, 1) == 1);
    assert(simulate_one_second(60, 2) == 2);
    assert(simulate_one_second(120, 3) == 3);
    PlaybackClock clock = {0};
    assert(playback_clock_advance(&clock, 10.0, 30.0, 0) == 7);
    playback_clock_reset(&clock);
    assert(playback_clock_advance(&clock, 1.0, 30.0, 1) == 0);
    puts("playback clock tests passed");
    return 0;
}
