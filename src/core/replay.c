#include "core/replay.h"
#include "env/env.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void replay_init(Replay* replay, const BomberConfig* config, uint64_t seed) {
    memset(replay, 0, sizeof(Replay));
    replay->seed = seed;
    replay->config = *config;
}

void replay_record(Replay* replay, Action action, const BomberState* state,
                   float reward, TerminalReason terminal) {
    if (replay->action_count >= MAX_REPLAY_STEPS) return;
    replay->actions[replay->action_count] = action;

    if (replay->frame_count < MAX_REPLAY_STEPS) {
        ReplayFrame* f = &replay->frames[replay->frame_count];
        f->step = state->step;
        f->state = *state;
        f->action = action;
        f->reward = reward;
        f->terminal = terminal;
        replay->frame_count++;
    }
    replay->action_count++;
}

/* Simple binary save/load for speed and determinism */
int replay_save(const Replay* replay, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(replay, sizeof(Replay), 1, f);
    fclose(f);
    return 1;
}

int replay_load(Replay* replay, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(replay, sizeof(Replay), 1, f);
    fclose(f);
    return n == 1;
}

int replay_playback(Replay* replay, BomberEnv* env) {
    env_init(env, &replay->config);
    env_reset(env, replay->seed);

    for (int i = 0; i < replay->action_count; i++) {
        StepResult result = env_step(env, replay->actions[i]);
        if (result.done) break;
    }
    return 1;
}
