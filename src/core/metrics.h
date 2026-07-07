#ifndef BOMBER_METRICS_H
#define BOMBER_METRICS_H

#include <stdint.h>
#include "env/types.h"

typedef struct {
    int episodes;
    int total_steps;
    float total_reward;
    int wins;
    int losses;
    int draws;
    int timeouts;
    int deaths;
    int crates_destroyed;
    int powerups_collected;
    int invalid_actions;
    int enemies_killed;
    int self_kills;
    int bombs_placed;

    /* Action distribution */
    int action_counts[6]; /* ACTION_COUNT = 6 */

    /* Timing */
    double total_time_ms;
} Metrics;

void metrics_init(Metrics* m);
void metrics_update(Metrics* m, Action action, StepResult result,
                    int crates_destroyed, int powerups_collected);
void metrics_print(const Metrics* m);
void metrics_print_compact(const Metrics* m);

#endif /* BOMBER_METRICS_H */
