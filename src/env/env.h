#ifndef BOMBER_ENV_H
#define BOMBER_ENV_H

#include "core/config.h"
#include "core/rng.h"
#include "core/ring_buffer.h"
#include "env/types.h"
#include "env/bomber_state.h"
#include "env/bomber_danger.h"
#include "env/bomber_observation.h"
#include "env/bomber_reward.h"

/* Forward declaration to avoid circular include with agents/agent.h */
struct Agent;

/* Generic environment API */

typedef struct BomberEnv {
    BomberState state;
    BomberConfig config;
    RNG rng;
    DangerMap danger;
    RewardBreakdown last_reward;
    IntRingBuffer action_history;
    int prev_agent_x;
    int prev_agent_y;
    int steps_since_progress;
    struct Agent* opponent; /* Optional opponent policy; NULL = built-in AI */
} BomberEnv;

void env_init(BomberEnv* env, const BomberConfig* config);
void env_reset(BomberEnv* env, uint64_t seed);
StepResult env_step(BomberEnv* env, Action action);
void env_observe(const BomberEnv* env, int agent_id, Observation* obs);
void env_set_opponent(BomberEnv* env, struct Agent* opponent);

/* Debug snapshot for visualizer */
typedef struct {
    BomberState state;
    DangerMap danger;
    RewardBreakdown last_reward;
    Action last_action;
    char decision_text[256];
    int episode;
    int total_episodes;
    float cumulative_reward;
    int determinism_ok;
} DebugSnapshot;

void env_get_debug_snapshot(const BomberEnv* env, DebugSnapshot* out);

#endif /* BOMBER_ENV_H */
