#ifndef BOMBER_DASHBOARD_H
#define BOMBER_DASHBOARD_H

#include "raylib.h"
#include "env/env.h"
#include "env/bomber_observation.h"

#define MAX_EVENTS 64
#define MAX_REWARD_HISTORY 500
#define MAX_EVENT_LEN 128

typedef struct {
    char events[MAX_EVENTS][MAX_EVENT_LEN];
    int event_count;
    float reward_history[MAX_REWARD_HISTORY];
    int reward_count;
    int action_counts[6];
    int total_actions;
} DashboardState;

void dashboard_init(DashboardState* ds);
void dashboard_add_event(DashboardState* ds, const char* event);
void dashboard_add_reward(DashboardState* ds, float reward);
void dashboard_add_action(DashboardState* ds, Action action);
void dashboard_draw(DashboardState* ds, const DebugSnapshot* snap, const Observation* obs,
                    int screen_w, int screen_h, int paused, int speed_mult);

#endif /* BOMBER_DASHBOARD_H */
