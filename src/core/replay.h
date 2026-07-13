#ifndef BOMBER_REPLAY_H
#define BOMBER_REPLAY_H

#include "core/config.h"
#include "env/types.h"
#include "env/bomber_state.h"

/* Forward declaration */
typedef struct BomberEnv BomberEnv;

typedef struct {
    int step;
    BomberState state;
    Action action;
    Action joint_actions[MAX_AGENTS];
    int joint_action_count;
    float reward;
    TerminalReason terminal;
    uint64_t state_hash;
    RNG rng_before_joint;
} ReplayFrame;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t seed;
    BomberConfig config;
    char agent_name[32];
    char opponent_name[32];
    int action_count;
    Action actions[MAX_REPLAY_STEPS];
    int frame_count;
    ReplayFrame frames[MAX_REPLAY_STEPS];
} Replay;

void replay_init(Replay* replay, const BomberConfig* config, uint64_t seed);
void replay_record(Replay* replay, Action action, const BomberState* state,
                   float reward, TerminalReason terminal);
void replay_set_policies(Replay* replay, const char* agent_name, const char* opponent_name);
void replay_record_env(Replay* replay, const BomberEnv* env, StepResult result);
int replay_save(const Replay* replay, const char* path);
int replay_load(Replay* replay, const char* path);
int replay_playback(Replay* replay, BomberEnv* env);
int replay_validate(const Replay* replay, uint64_t* final_hash);

#endif /* BOMBER_REPLAY_H */
