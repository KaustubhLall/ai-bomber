#include "env/env.h"
#include "env/bomber_map.h"
#include "env/bomber_blast.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 1;
    cfg.crate_density = 0; /* clear map for predictable tests */

    BomberEnv env;
    env_init(&env, &cfg);

    /* Place a bomb manually at center */
    int bx = 5, by = 5;
    env.state.bombs[0].x = bx;
    env.state.bombs[0].y = by;
    env.state.bombs[0].owner_id = 0;
    env.state.bombs[0].timer = 1;
    env.state.bombs[0].range = 2;
    env.state.bombs[0].active = 1;

    /* Compute blast tiles */
    BlastResult blast;
    compute_blast_tiles(&env.state, bx, by, 2, &blast);

    /* Center should be in blast */
    int has_center = 0;
    for (int i = 0; i < blast.count; i++) {
        if (blast.tiles[i].x == bx && blast.tiles[i].y == by) has_center = 1;
    }
    assert(has_center == 1);

    /* Should extend 2 tiles in each direction */
    int has_up = 0, has_down = 0, has_left = 0, has_right = 0;
    for (int i = 0; i < blast.count; i++) {
        if (blast.tiles[i].x == bx && blast.tiles[i].y == by - 2) has_up = 1;
        if (blast.tiles[i].x == bx && blast.tiles[i].y == by + 2) has_down = 1;
        if (blast.tiles[i].x == bx - 2 && blast.tiles[i].y == by) has_left = 1;
        if (blast.tiles[i].x == bx + 2 && blast.tiles[i].y == by) has_right = 1;
    }
    assert(has_up && has_down && has_left && has_right);

    /* Solid walls block blast */
    /* Place a wall at (bx, by-1) and recompute */
    env.state.tiles[by - 1][bx] = TILE_SOLID_WALL;
    compute_blast_tiles(&env.state, bx, by, 2, &blast);
    has_up = 0;
    for (int i = 0; i < blast.count; i++) {
        if (blast.tiles[i].x == bx && blast.tiles[i].y == by - 2) has_up = 1;
    }
    assert(has_up == 0); /* blocked by wall */

    /* Crates are destroyed by blast */
    env_reset(&env, 1);
    cfg.crate_density = 0;
    env_init(&env, &cfg);
    env.state.tiles[5][6] = TILE_CRATE;
    env.state.bombs[0].x = 5; env.state.bombs[0].y = 5;
    env.state.bombs[0].owner_id = 0; env.state.bombs[0].timer = 1;
    env.state.bombs[0].range = 2; env.state.bombs[0].active = 1;

    /* Explode the bomb */
    explode_bomb(&env.state, 0, &env.rng, cfg.powerup_rate);

    /* Crate should be destroyed (turned to floor or powerup) */
    TileType t = env.state.tiles[5][6];
    assert(t == TILE_FLOOR || t == TILE_POWERUP_BOMB ||
           t == TILE_POWERUP_RANGE || t == TILE_POWERUP_SPEED);

    /* Agent dies in blast */
    env_reset(&env, 1);
    env_init(&env, &cfg);
    env.state.agents[0].x = 5;
    env.state.agents[0].y = 5;
    env.state.agents[0].alive = 1;
    env.state.bombs[0].x = 5; env.state.bombs[0].y = 5;
    env.state.bombs[0].owner_id = 0; env.state.bombs[0].timer = 1;
    env.state.bombs[0].range = 2; env.state.bombs[0].active = 1;

    explode_bomb(&env.state, 0, &env.rng, cfg.powerup_rate);
    assert(env.state.agents[0].alive == 0);

    printf("test_blast: ALL PASSED\n");
    return 0;
}
