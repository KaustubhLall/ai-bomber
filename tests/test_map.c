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

    /* Every classic corner spawn has an L-shaped three-tile safe pocket. */
    int max_x = env.state.width - 2;
    int max_y = env.state.height - 2;
    int safe_tiles[12][2] = {
        {1, 1}, {2, 1}, {1, 2},
        {max_x, 1}, {max_x - 1, 1}, {max_x, 2},
        {1, max_y}, {2, max_y}, {1, max_y - 1},
        {max_x, max_y}, {max_x - 1, max_y}, {max_x, max_y - 1}
    };
    for (int i = 0; i < 12; i++) {
        assert(env.state.tiles[safe_tiles[i][1]][safe_tiles[i][0]] == TILE_FLOOR);
    }

    /* The safe pockets remain clear even at maximum crate density. */
    cfg.crate_density = 100;
    env.config = cfg;
    env_reset(&env, 100);
    for (int i = 0; i < 12; i++) {
        assert(env.state.tiles[safe_tiles[i][1]][safe_tiles[i][0]] == TILE_FLOOR);
    }

    /* Walkable check */
    assert(map_is_walkable(&env.state, 1, 1) == 1);
    assert(map_is_walkable(&env.state, 0, 0) == 0); /* wall */
    assert(map_in_bounds(&env.state, 0, 0) == 1);
    assert(map_in_bounds(&env.state, -1, 0) == 0);
    assert(map_in_bounds(&env.state, cfg.width, 0) == 0);

    /* All supported agents receive unique, crate-free spawn tiles. */
    config_battle(&cfg); cfg.agent_count = MAX_AGENTS; cfg.crate_density = 100;
    env.config = cfg; env_reset(&env, 100);
    for (int a = 0; a < MAX_AGENTS; a++) {
        assert(env.state.tiles[env.state.agents[a].y][env.state.agents[a].x] == TILE_FLOOR);
        for (int b = a + 1; b < MAX_AGENTS; b++)
            assert(env.state.agents[a].x != env.state.agents[b].x ||
                   env.state.agents[a].y != env.state.agents[b].y);
    }

    printf("test_map: ALL PASSED\n");
    return 0;
}
