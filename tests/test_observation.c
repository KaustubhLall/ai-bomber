#include "env/env.h"
#include "env/bomber_observation.h"
#include "env/bomber_danger.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 1;

    BomberEnv env;
    env_init(&env, &cfg);

    Observation obs;
    env_observe(&env, 0, &obs);

    /* Agent info correct */
    assert(obs.agent_x == env.state.agents[0].x);
    assert(obs.agent_y == env.state.agents[0].y);
    assert(obs.agent_alive == 1);
    assert(obs.agent_bomb_ammo == 1);
    assert(obs.agent_blast_range == cfg.blast_range);

    /* Local obs size is correct */
    assert(LOCAL_OBS_SIZE == 11);

    /* Center of local obs should be agent's tile */
    int cx = LOCAL_OBS_HALF;
    int cy = LOCAL_OBS_HALF;
    /* Agent starts at (1,1) which is floor */
    assert(obs.local_tiles[cy][cx] == TILE_FLOOR);

    /* Valid actions: at least WAIT should be valid */
    assert(obs.valid_actions[ACTION_WAIT] == 1);

    /* Flat observation size is consistent */
    float flat[OBS_FLAT_SIZE];
    int flat_size;
    obs_to_flat(&obs, flat, &flat_size);
    assert(flat_size > 0);
    assert(flat_size <= OBS_FLAT_SIZE);

    /* Same state produces same observation */
    Observation obs2;
    env_observe(&env, 0, &obs2);
    assert(obs.agent_x == obs2.agent_x);
    assert(obs.agent_y == obs2.agent_y);
    assert(memcmp(obs.local_tiles, obs2.local_tiles, sizeof(obs.local_tiles)) == 0);

    /* Flat observation is deterministic */
    float flat2[OBS_FLAT_SIZE];
    int flat2_size;
    obs_to_flat(&obs2, flat2, &flat2_size);
    assert(flat_size == flat2_size);
    for (int i = 0; i < flat_size; i++) {
        assert(flat[i] == flat2[i]);
    }

    printf("test_observation: ALL PASSED (flat_size=%d)\n", flat_size);
    return 0;
}
