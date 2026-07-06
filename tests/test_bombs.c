#include "env/env.h"
#include "env/bomber_map.h"
#include "env/bomber_bombs.h"
#include "env/bomber_blast.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 1;

    BomberEnv env;
    env_init(&env, &cfg);

    /* Agent starts with 1 bomb ammo */
    assert(env.state.agents[0].bomb_ammo == 1);

    /* Place bomb */
    int result = place_bomb(&env.state, 0);
    assert(result == 1);
    assert(env.state.agents[0].bomb_ammo == 0);
    assert(env.state.agents[0].bombs_active == 1);

    /* Can't place another without ammo */
    result = place_bomb(&env.state, 0);
    assert(result == 0);

    /* Can't place on same tile as existing bomb */
    env.state.agents[0].bomb_ammo = 1;
    result = place_bomb(&env.state, 0);
    assert(result == 0);
    /* Reset ammo to 0 since we have 1 active bomb */
    env.state.agents[0].bomb_ammo = 0;

    /* Bomb exists at agent position */
    assert(map_has_bomb(&env.state, env.state.agents[0].x, env.state.agents[0].y) == 1);

    /* Find the bomb and verify timer is 0 (set by rules_try_place_bomb) */
    /* place_bomb alone sets timer=0; rules_try_place_bomb sets the actual timer */
    BombState* bomb = map_bomb_at(&env.state, env.state.agents[0].x, env.state.agents[0].y);
    assert(bomb != NULL);
    assert(bomb->owner_id == 0);
    assert(bomb->active == 1);

    /* Set timer and tick */
    bomb->timer = 3;
    tick_bombs(&env.state);
    assert(bomb->timer == 2);
    assert(bomb->active == 1);

    tick_bombs(&env.state);
    assert(bomb->timer == 1);

    tick_bombs(&env.state);
    /* At timer 0, bomb should explode */
    assert(bomb->active == 0);

    /* Ammo should return after explosion */
    assert(env.state.agents[0].bomb_ammo == 1);
    assert(env.state.agents[0].bombs_active == 0);

    printf("test_bombs: ALL PASSED\n");
    return 0;
}
