#ifndef BOMBER_OBSERVATION_H
#define BOMBER_OBSERVATION_H

#include "env/bomber_state.h"
#include "env/types.h"
#include "env/bomber_danger.h"
#include "core/ring_buffer.h"

#ifndef LOCAL_OBS_SIZE
#define LOCAL_OBS_SIZE 11
#endif
#ifndef LOCAL_OBS_HALF
#define LOCAL_OBS_HALF (LOCAL_OBS_SIZE / 2)
#endif
#define OBS_FLAT_SIZE (LOCAL_OBS_SIZE * LOCAL_OBS_SIZE * 4 + 64)

typedef struct {
    /* Agent info */
    int agent_x;
    int agent_y;
    int agent_alive;
    int agent_bomb_ammo;
    int agent_blast_range;
    int agent_speed;
    int agent_score;

    /* Local grid: tile type, bomb presence, bomb timer, danger level */
    int local_tiles[LOCAL_OBS_SIZE][LOCAL_OBS_SIZE];
    int local_bombs[LOCAL_OBS_SIZE][LOCAL_OBS_SIZE];
    int local_bomb_timers[LOCAL_OBS_SIZE][LOCAL_OBS_SIZE];
    int local_danger[LOCAL_OBS_SIZE][LOCAL_OBS_SIZE];

    /* Enemy info (compact) */
    int enemy_count;
    int enemy_x[MAX_AGENTS];
    int enemy_y[MAX_AGENTS];
    int enemy_alive[MAX_AGENTS];

    /* Powerup positions in local view */
    int powerup_count;
    int powerup_x[MAX_AGENTS * 4];
    int powerup_y[MAX_AGENTS * 4];

    /* Valid and safe actions */
    int valid_actions[ACTION_COUNT];
    int safe_actions[ACTION_COUNT];

    /* Previous action */
    int prev_action;

    /* Danger info */
    int in_danger;
    int danger_timer;

    /* Flat observation vector for ML */
    float flat[OBS_FLAT_SIZE];
} Observation;

void obs_compute(const BomberState* state, const DangerMap* dm, int agent_id,
                 int prev_action, Observation* obs);
void obs_to_flat(const Observation* obs, float* out, int* out_size);

#endif /* BOMBER_OBSERVATION_H */
