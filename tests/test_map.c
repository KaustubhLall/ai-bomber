#include "env/env.h"
#include "env/bomber_map.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 100;

    BomberEnv env;
    env_init(&env, &cfg);

    /* Map dimensions correct */
    assert(env.state.width == cfg.width);
    assert(env.state.height == cfg.height);

    /* Borders are solid walls */
    for (int x = 0; x < env.state.width; x++) {
        assert(env.state.tiles[0][x] == TILE_SOLID_WALL);
        assert(env.state.tiles[env.state.height - 1][x] == TILE_SOLID_WALL);
    }
    for (int y = 0; y < env.state.height; y++) {
        assert(env.state.tiles[y][0] == TILE_SOLID_WALL);
        assert(env.state.tiles[y][env.state.width - 1] == TILE_SOLID_WALL);
    }

    /* Grid pattern walls at even intersections */
    for (int y = 2; y < env.state.height - 1; y += 2) {
        for (int x = 2; x < env.state.width - 1; x += 2) {
            assert(env.state.tiles[y][x] == TILE_SOLID_WALL);
        }
    }

    /* Same seed produces same map */
    BomberEnv env2;
    env_init(&env2, &cfg);
    env_reset(&env2, 100);
    env_reset(&env, 100);

    for (int y = 0; y < cfg.height; y++) {
        for (int x = 0; x < cfg.width; x++) {
            assert(env.state.tiles[y][x] == env2.state.tiles[y][x]);
        }
    }

    /* Spawn corners are clear (floor) */
    assert(env.state.tiles[1][1] == TILE_FLOOR);
    assert(env.state.tiles[1][2] == TILE_FLOOR);
    assert(env.state.tiles[2][1] == TILE_FLOOR);

    /* Walkable check */
    assert(map_is_walkable(&env.state, 1, 1) == 1);
    assert(map_is_walkable(&env.state, 0, 0) == 0); /* wall */
    assert(map_in_bounds(&env.state, 0, 0) == 1);
    assert(map_in_bounds(&env.state, -1, 0) == 0);
    assert(map_in_bounds(&env.state, cfg.width, 0) == 0);

    printf("test_map: ALL PASSED\n");
    return 0;
}
