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
    float reward;
    TerminalReason terminal;
} ReplayFrame;

typedef struct {
    uint64_t seed;
    BomberConfig config;
    int action_count;
    Action actions[MAX_REPLAY_STEPS];
    int frame_count;
    ReplayFrame frames[MAX_REPLAY_STEPS];
} Replay;

void replay_init(Replay* replay, const BomberConfig* config, uint64_t seed);
void replay_record(Replay* replay, Action action, const BomberState* state,
                   float reward, TerminalReason terminal);
int replay_save(const Replay* replay, const char* path);
int replay_load(Replay* replay, const char* path);
int replay_playback(Replay* replay, BomberEnv* env);

#endif /* BOMBER_REPLAY_H */
