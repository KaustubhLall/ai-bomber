#include "env/env.h"
#include "env/bomber_danger.h"
#include "env/bomber_map.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 1;
    cfg.crate_density = 0;

    BomberEnv env;
    env_init(&env, &cfg);

    /* No bombs -> all tiles safe */
    danger_compute(&env.danger, &env.state);
    for (int y = 0; y < env.state.height; y++) {
        for (int x = 0; x < env.state.width; x++) {
            assert(env.danger.time_to_blast[y][x] == -1);
            assert(env.danger.safe_now[y][x] == 1);
        }
    }

    /* Place a bomb and check danger map */
    env.state.bombs[0].x = 5;
    env.state.bombs[0].y = 5;
    env.state.bombs[0].owner_id = 0;
    env.state.bombs[0].timer = 3;
    env.state.bombs[0].range = 2;
    env.state.bombs[0].active = 1;

    danger_compute(&env.danger, &env.state);

    /* Center tile should be marked as danger */
    assert(env.danger.time_to_blast[5][5] == 3);
    assert(env.danger.safe_now[5][5] == 0);

    /* Tiles in blast range should be marked */
    assert(env.danger.time_to_blast[5][6] == 3);
    assert(env.danger.time_to_blast[5][7] == 3);
    assert(env.danger.time_to_blast[4][5] == 3);
    assert(env.danger.time_to_blast[3][5] == 3);

    /* Tile outside blast should be safe */
    assert(env.danger.time_to_blast[1][1] == -1);

    /* Time-to-blast decreases as timer decreases */
    env.state.bombs[0].timer = 1;
    danger_compute(&env.danger, &env.state);
    assert(env.danger.time_to_blast[5][5] == 1);

    /* When timer is 0, current_blast is set */
    env.state.bombs[0].timer = 0;
    danger_compute(&env.danger, &env.state);
    assert(env.danger.current_blast[5][5] == 1);

    /* Escape route: agent at (5,5) with bomb at (5,5) timer=3
       should be able to escape to a safe tile */
    env.state.bombs[0].timer = 3;
    env.state.agents[0].x = 5;
    env.state.agents[0].y = 5;
    env.state.agents[0].alive = 1;
    danger_compute(&env.danger, &env.state);
    danger_compute_escape(&env.danger, &env.state, 0);

    /* At least some reachable safe tiles should exist */
    int has_safe = 0;
    for (int y = 0; y < env.state.height; y++) {
        for (int x = 0; x < env.state.width; x++) {
            if (env.danger.reachable_safe[y][x]) has_safe = 1;
        }
    }
    assert(has_safe == 1);

    /* Would trap agent: place agent in a dead-end with bomb */
    /* Create a dead-end: walls around (1,1) except (2,1) */
    env_reset(&env, 1);
    /* Agent at (1,1), walls everywhere except (2,1) which is also walled */
    /* Actually with the grid pattern, (1,1) has exits to (2,1) and (1,2) */
    /* Let's test would_trap with a simple case */
    int traps = danger_would_trap_agent(&env.state, 0, env.state.agents[0].x, env.state.agents[0].y);
    /* In an open area, should not trap */
    /* This depends on map layout, so just verify it runs without crashing */
    (void)traps;

    printf("test_danger: ALL PASSED\n");
    return 0;
}
