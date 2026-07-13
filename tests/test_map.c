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

    /* Two-player battle spawns in diagonally opposite corners (top-left / bottom-right);
       each has an L-shaped three-tile safe pocket. (config_survival is a 2-agent game.) */
    int max_x = env.state.width - 2;
    int max_y = env.state.height - 2;
    int safe_tiles[6][2] = {
        {1, 1}, {2, 1}, {1, 2},
        {max_x, max_y}, {max_x - 1, max_y}, {max_x, max_y - 1}
    };
    for (int i = 0; i < 6; i++) {
        assert(env.state.tiles[safe_tiles[i][1]][safe_tiles[i][0]] == TILE_FLOOR);
    }

    /* The safe pockets remain clear even at maximum crate density. */
    cfg.crate_density = 100;
    env.config = cfg;
    env_reset(&env, 100);
    for (int i = 0; i < 6; i++) {
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

    /* Sudden death closes the arena inward and crushes agents caught on new walls. */
    {
        BomberConfig scfg;
        config_battle(&scfg);
        scfg.seed = 5;
        scfg.crate_density = 0;
        scfg.sudden_death_start = 10;
        scfg.shrink_interval = 1;
        BomberEnv senv;
        env_init(&senv, &scfg);
        senv.state.agents[0].x = 1; senv.state.agents[0].y = 2; senv.state.agents[0].alive = 1;
        senv.state.tiles[2][1] = TILE_FLOOR; /* border-adjacent interior tile, border-distance 1 */
        /* Before the threshold nothing closes. */
        senv.state.step = 5;
        map_apply_sudden_death(&senv.state, scfg.sudden_death_start, scfg.shrink_interval);
        assert(senv.state.tiles[2][1] == TILE_FLOOR);
        assert(senv.state.agents[0].alive == 1);
        /* At the threshold, ring 1 (border-distance 1) walls in and crushes the agent. */
        senv.state.step = 10;
        map_apply_sudden_death(&senv.state, scfg.sudden_death_start, scfg.shrink_interval);
        assert(senv.state.tiles[2][1] == TILE_SOLID_WALL);
        assert(senv.state.agents[0].alive == 0);
        assert(senv.state.death_owner[0] == -1);
    }

    printf("test_map: ALL PASSED\n");
    return 0;
}
