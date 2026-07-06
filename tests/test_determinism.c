#include "env/env.h"
#include "agents/agent.h"
#include "sim/runner.h"
#include "core/metrics.h"
#include "env/bomber_map.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 1337;
    cfg.max_steps = 100;

    /* Determinism: same seed + same actions -> same final state */
    BomberEnv env1, env2;
    env_init(&env1, &cfg);
    env_init(&env2, &cfg);

    env_reset(&env1, 999);
    env_reset(&env2, 999);

    /* Verify initial states match */
    assert(env1.state.agents[0].x == env2.state.agents[0].x);
    assert(env1.state.agents[0].y == env2.state.agents[0].y);
    for (int y = 0; y < cfg.height; y++) {
        for (int x = 0; x < cfg.width; x++) {
            assert(env1.state.tiles[y][x] == env2.state.tiles[y][x]);
        }
    }

    /* Run same action sequence on both */
    Action actions[] = {ACTION_RIGHT, ACTION_DOWN, ACTION_WAIT, ACTION_PLACE_BOMB,
                        ACTION_LEFT, ACTION_UP, ACTION_WAIT, ACTION_WAIT,
                        ACTION_WAIT, ACTION_WAIT};
    int n_actions = (int)(sizeof(actions) / sizeof(actions[0]));

    for (int i = 0; i < n_actions; i++) {
        StepResult r1 = env_step(&env1, actions[i]);
        StepResult r2 = env_step(&env2, actions[i]);
        assert(r1.reward == r2.reward);
        assert(r1.done == r2.done);
        assert(r1.terminal_reason == r2.terminal_reason);
    }

    /* Final states should be identical */
    assert(env1.state.step == env2.state.step);
    assert(env1.state.agents[0].x == env2.state.agents[0].x);
    assert(env1.state.agents[0].y == env2.state.agents[0].y);
    assert(env1.state.agents[0].alive == env2.state.agents[0].alive);
    assert(env1.state.agents[0].bomb_ammo == env2.state.agents[0].bomb_ammo);

    for (int y = 0; y < cfg.height; y++) {
        for (int x = 0; x < cfg.width; x++) {
            assert(env1.state.tiles[y][x] == env2.state.tiles[y][x]);
        }
    }

    for (int b = 0; b < MAX_BOMBS; b++) {
        assert(env1.state.bombs[b].active == env2.state.bombs[b].active);
        if (env1.state.bombs[b].active) {
            assert(env1.state.bombs[b].x == env2.state.bombs[b].x);
            assert(env1.state.bombs[b].y == env2.state.bombs[b].y);
            assert(env1.state.bombs[b].timer == env2.state.bombs[b].timer);
        }
    }

    /* Different seeds produce different maps (very likely) */
    env_reset(&env1, 111);
    env_reset(&env2, 222);
    int diff = 0;
    for (int y = 0; y < cfg.height; y++) {
        for (int x = 0; x < cfg.width; x++) {
            if (env1.state.tiles[y][x] != env2.state.tiles[y][x]) diff++;
        }
    }
    /* With crate_density=50, different seeds should produce different layouts */
    assert(diff > 0);

    printf("test_determinism: ALL PASSED\n");
    return 0;
}
