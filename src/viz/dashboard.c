#include "viz/dashboard.h"
#include "viz/renderer.h"
#include "env/bomber_map.h"
#include "env/bomber_blast.h"
#include <stdio.h>
#include <string.h>

void dashboard_init(DashboardState* ds) {
    memset(ds, 0, sizeof(DashboardState));
}

void dashboard_add_event(DashboardState* ds, const char* event) {
    if (ds->event_count >= MAX_EVENTS) {
        /* Shift events */
        memmove(ds->events[0], ds->events[1], sizeof(ds->events[0]) * (MAX_EVENTS - 1));
        ds->event_count = MAX_EVENTS - 1;
    }
    strncpy(ds->events[ds->event_count], event, MAX_EVENT_LEN - 1);
    ds->events[ds->event_count][MAX_EVENT_LEN - 1] = '\0';
    ds->event_count++;
}

void dashboard_add_reward(DashboardState* ds, float reward) {
    if (ds->reward_count < MAX_REWARD_HISTORY) {
        ds->reward_history[ds->reward_count++] = reward;
    } else {
        memmove(ds->reward_history, ds->reward_history + 1,
                sizeof(float) * (MAX_REWARD_HISTORY - 1));
        ds->reward_history[MAX_REWARD_HISTORY - 1] = reward;
    }
}

void dashboard_add_action(DashboardState* ds, Action action) {
    if (action >= 0 && action < 6) {
        ds->action_counts[action]++;
        ds->total_actions++;
    }
}

static void check_events(DashboardState* ds, const DebugSnapshot* snap,
                         const DebugSnapshot* prev) {
    const BomberState* state = &snap->state;
    const BomberState* prev_state = &prev->state;
    char buf[MAX_EVENT_LEN];

    /* Bomb placed */
    int prev_bombs = 0, cur_bombs = 0;
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (prev_state->bombs[i].active) prev_bombs++;
        if (state->bombs[i].active) cur_bombs++;
    }
    if (cur_bombs > prev_bombs) {
        snprintf(buf, sizeof(buf), "[Step %d] Bomb placed", state->step);
        dashboard_add_event(ds, buf);
    }

    /* Bomb exploded (fewer active bombs) */
    if (cur_bombs < prev_bombs) {
        snprintf(buf, sizeof(buf), "[Step %d] Bomb exploded", state->step);
        dashboard_add_event(ds, buf);
    }

    /* Crate destroyed */
    int prev_crates = map_count_crates(prev_state);
    int cur_crates = map_count_crates(state);
    if (cur_crates < prev_crates) {
        snprintf(buf, sizeof(buf), "[Step %d] Crate(s) destroyed (%d)",
                 state->step, prev_crates - cur_crates);
        dashboard_add_event(ds, buf);
    }

    /* Agent died */
    if (prev_state->agents[0].alive && !state->agents[0].alive) {
        snprintf(buf, sizeof(buf), "[Step %d] Agent died!", state->step);
        dashboard_add_event(ds, buf);
    }

    /* Enemy died */
    for (int a = 1; a < state->agent_count; a++) {
        if (prev_state->agents[a].alive && !state->agents[a].alive) {
            snprintf(buf, sizeof(buf), "[Step %d] Enemy %d eliminated!", state->step, a);
            dashboard_add_event(ds, buf);
        }
    }

    /* Powerup collected */
    if (snap->last_reward.powerup > 0) {
        snprintf(buf, sizeof(buf), "[Step %d] Powerup collected", state->step);
        dashboard_add_event(ds, buf);
    }

    /* Terminal */
    if (snap->last_action >= 0 && snap->state.step > 0) {
        if (snap->state.agents[0].alive && state->agent_count > 1) {
            int alive_enemies = 0;
            for (int a = 1; a < state->agent_count; a++) {
                if (state->agents[a].alive) alive_enemies++;
            }
            if (alive_enemies == 0 && prev_state->agents[1].alive) {
                snprintf(buf, sizeof(buf), "[Step %d] MATCH WON!", state->step);
                dashboard_add_event(ds, buf);
            }
        }
    }
}

void dashboard_draw(DashboardState* ds, const DebugSnapshot* snap, const Observation* obs,
                    int screen_w, int screen_h, int paused, int speed_mult) {
    static DebugSnapshot prev_snap;
    static int initialized = 0;

    if (initialized) {
        check_events(ds, snap, &prev_snap);
    }
    prev_snap = *snap;
    initialized = 1;

    /* Layout: arena in center, panels on sides */
    int tile_size = TILE_SIZE;
    int arena_w = snap->state.width * tile_size;
    int arena_h = snap->state.height * tile_size;
    int arena_ox = (screen_w - arena_w) / 2;
    int arena_oy = 20;

    /* Left panel: status + local obs + danger map */
    int left_w = 240;
    int left_ox = 8;

    renderer_draw_status_panel(snap, left_ox, 20, left_w, 140);
    renderer_draw_local_obs(obs, left_ox, 170, 16);
    renderer_draw_danger_map(snap, left_ox, 400, left_w, 200);

    /* Right panel: reward graph + action dist + bomb timeline + decision trace + event log + controls */
    int right_w = 280;
    int right_ox = screen_w - right_w - 8;

    renderer_draw_reward_graph(ds->reward_history, ds->reward_count,
                               right_ox, 20, right_w, 120, 2.0f);
    renderer_draw_action_dist(ds->action_counts, ds->total_actions,
                              right_ox, 150, right_w, 130);
    renderer_draw_bomb_timeline(snap, right_ox, 290, right_w);
    renderer_draw_decision_trace(snap->decision_text, right_ox, 420, right_w, 60);
    renderer_draw_event_log((const char**)ds->events, ds->event_count,
                            right_ox, 490, right_w, 130);
    renderer_draw_controls(right_ox, 630, right_w, 120, paused, speed_mult);

    /* Central arena */
    renderer_draw_arena(snap, arena_ox, arena_oy, tile_size);
}
