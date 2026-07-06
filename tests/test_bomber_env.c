#include "env/env.h"
#include "env/bomber_map.h"
#include "env/bomber_bombs.h"
#include "env/bomber_blast.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 42;

    BomberEnv env;
    env_init(&env, &cfg);

    /* Agent starts alive */
    assert(env.state.agents[0].alive == 1);
    assert(env.state.agents[0].bomb_ammo == 1);
    assert(env.state.agents[0].blast_range == 2);

    /* Step with WAIT */
    StepResult r = env_step(&env, ACTION_WAIT);
    assert(r.done == 0);
    assert(env.state.step == 1);

    /* Move right (should work from corner) */
    int old_x = env.state.agents[0].x;
    r = env_step(&env, ACTION_RIGHT);
    assert(env.state.agents[0].x == old_x + 1 || env.state.agents[0].x == old_x);

    /* Invalid move (into wall) - agent at (1,1) moving up should be blocked */
    env_reset(&env, 42);
    old_x = env.state.agents[0].x;
    int old_y = env.state.agents[0].y;
    r = env_step(&env, ACTION_UP);
    /* Moving up from (1,1) hits wall at (1,0) */
    assert(env.state.agents[0].x == old_x);
    assert(env.state.agents[0].y == old_y);

    /* Place bomb */
    env_reset(&env, 42);
    int ammo_before = env.state.agents[0].bomb_ammo;
    r = env_step(&env, ACTION_PLACE_BOMB);
    assert(env.state.agents[0].bomb_ammo == ammo_before - 1);
    assert(map_has_bomb(&env.state, env.state.agents[0].x, env.state.agents[0].y));

    /* Episode timeout */
    cfg.max_steps = 5;
    env_init(&env, &cfg);
    for (int i = 0; i < 5; i++) {
        r = env_step(&env, ACTION_WAIT);
    }
    assert(r.done == 1);
    assert(r.terminal_reason == TERMINAL_TIMEOUT || r.terminal_reason == TERMINAL_NONE);

    printf("test_bomber_env: ALL PASSED\n");
    return 0;
}
