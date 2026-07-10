#include "core/replay.h"
#include "env/env.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define REPLAY_MAGIC UINT32_C(0x4252504C)
/* v4: BomberState now carries persistent flame (flame_ttl/flame_owner) + sudden-death
   walls, so a v4 frame renders flames/closing arena correctly. The Replay struct embeds
   BomberState, so its size changed; older v3 files are binary-incompatible and are cleanly
   rejected by the version check on load (regenerate replays with the current binary). */
#define REPLAY_VERSION UINT32_C(4)

void replay_init(Replay* replay, const BomberConfig* config, uint64_t seed) {
    memset(replay, 0, sizeof(Replay));
    replay->magic = REPLAY_MAGIC;
    replay->version = REPLAY_VERSION;
    replay->seed = seed;
    replay->config = *config;
}

void replay_set_policies(Replay* replay, const char* agent_name, const char* opponent_name) {
    if (!replay) return;
    snprintf(replay->agent_name, sizeof(replay->agent_name), "%s", agent_name ? agent_name : "unknown");
    snprintf(replay->opponent_name, sizeof(replay->opponent_name), "%s", opponent_name ? opponent_name : "unknown");
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

void replay_record_env(Replay* replay, const BomberEnv* env, StepResult result) {
    if (!replay || !env || replay->frame_count >= MAX_REPLAY_STEPS) return;
    Action action = env->last_joint_action_count > 0 ? env->last_joint_actions[0] : ACTION_WAIT;
    replay_record(replay, action, &env->state, result.reward, result.terminal_reason);
    ReplayFrame* frame = &replay->frames[replay->frame_count - 1];
    frame->joint_action_count = env->last_joint_action_count;
    for (int a = 0; a < frame->joint_action_count; a++) frame->joint_actions[a] = env->last_joint_actions[a];
    frame->rng_before_joint = env->last_rng_before_joint;
    frame->state_hash = env_state_hash(env);
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
    return n == 1 && replay->magic == REPLAY_MAGIC && replay->version == REPLAY_VERSION;
}

int replay_playback(Replay* replay, BomberEnv* env) {
    env_init(env, &replay->config);
    env_reset(env, replay->seed);

    for (int i = 0; i < replay->frame_count; i++) {
        ReplayFrame* frame = &replay->frames[i];
        if (frame->joint_action_count > 0) env->rng = frame->rng_before_joint;
        StepResult result = frame->joint_action_count > 0
            ? env_step_joint(env, frame->joint_actions, frame->joint_action_count)
            : env_step(env, frame->action);
        if (frame->state_hash && env_state_hash(env) != frame->state_hash) return 0;
        if (result.done) break;
    }
    return 1;
}

int replay_validate(const Replay* replay, uint64_t* final_hash) {
    if (!replay || replay->frame_count <= 0) return 0;
    BomberEnv env = {0};
    int ok = replay_playback((Replay*)replay, &env);
    if (final_hash) *final_hash = env_state_hash(&env);
    return ok && env_state_hash(&env) == replay->frames[replay->frame_count - 1].state_hash;
}
