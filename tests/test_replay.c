#include "env/env.h"
#include "core/replay.h"
#include "env/bomber_map.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 42;
    cfg.max_steps = 20;

    BomberEnv env;
    env_init(&env, &cfg);

    Replay* replay = (Replay*)calloc(1, sizeof(Replay));
    replay_init(replay, &cfg, 42);

    /* Run a short episode recording actions */
    for (int i = 0; i < 10; i++) {
        Action a = (i % 3 == 0) ? ACTION_RIGHT : (i % 3 == 1) ? ACTION_DOWN : ACTION_WAIT;
        StepResult r = env_step(&env, a);
        replay_record_env(replay, &env, r);
        if (r.done) break;
    }

    assert(replay->action_count > 0);
    assert(replay->seed == 42);

    /* Save and load */
    const char* path = "test_replay.bin";
    int saved = replay_save(replay, path);
    assert(saved == 1);

    Replay* loaded = (Replay*)calloc(1, sizeof(Replay));
    int loaded_ok = replay_load(loaded, path);
    assert(loaded_ok == 1);

    /* Verify loaded replay matches */
    assert(loaded->seed == replay->seed);
    assert(loaded->action_count == replay->action_count);
    for (int i = 0; i < replay->action_count; i++) {
        assert(loaded->actions[i] == replay->actions[i]);
    }
    assert(loaded->config.width == replay->config.width);
    assert(loaded->config.height == replay->config.height);

    /* Playback should produce same final state */
    BomberEnv pb_env;
    assert(replay_playback(loaded, &pb_env));

    /* The playback should have gone through same steps */
    assert(pb_env.state.step == env.state.step || pb_env.state.step == 10);
    uint64_t final_hash = 0;
    assert(replay_validate(loaded, &final_hash));
    assert(final_hash == loaded->frames[loaded->frame_count - 1].state_hash);

    free(replay);
    free(loaded);
    printf("test_replay: ALL PASSED\n");
    return 0;
}
