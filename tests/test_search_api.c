#include "env/env.h"
#include "core/config.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    BomberConfig cfg;
    config_battle(&cfg);
    cfg.agent_count = 2;
    BomberEnv source, clone, replay;
    env_init(&source, &cfg);
    env_reset(&source, 4242);
    env_copy(&clone, &source);
    env_copy(&replay, &source);
    uint64_t original = env_state_hash(&source);

    Action legal[ACTION_COUNT];
    int count = 0;
    env_legal_actions(&clone, 0, legal, &count);
    assert(count >= 1 && count <= ACTION_COUNT);
    assert(legal[count - 1] == ACTION_WAIT);

    Action joint[2] = {ACTION_WAIT, ACTION_WAIT};
    StepResult a = env_step_joint(&clone, joint, 2);
    StepResult b = env_step_joint(&replay, joint, 2);
    assert(env_state_hash(&clone) == env_state_hash(&replay));
    assert(a.reward == b.reward && a.done == b.done);
    assert(env_state_hash(&source) == original);
    assert(env_state_hash(&clone) != original);
    puts("search API tests passed");
    return 0;
}
