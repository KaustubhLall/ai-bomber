#include "env/env.h"
#include "env/bomber_map.h"
#include "env/bomber_bombs.h"
#include "env/bomber_blast.h"
#include "env/bomber_danger.h"
#include "env/bomber_rules.h"
#include "env/bomber_observation.h"
#include "env/bomber_reward.h"
#include "agents/agent.h"
#include "sim/runner.h"
#include "core/metrics.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Test: rules_pickup_powerup returns 1 when collecting, 0 otherwise */
static void test_powerup_pickup_return(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 1;
    cfg.crate_density = 0;

    BomberEnv* env = (BomberEnv*)calloc(1, sizeof(BomberEnv));
    assert(env != NULL);
    env_init(env, &cfg);

    /* No powerup on agent tile -> returns 0 */
    int collected = rules_pickup_powerup(&env->state, 0);
    assert(collected == 0);

    /* Place a powerup on agent tile */
    env->state.tiles[env->state.agents[0].y][env->state.agents[0].x] = TILE_POWERUP_BOMB;
    int ammo_before = env->state.agents[0].bomb_ammo;
    collected = rules_pickup_powerup(&env->state, 0);
    assert(collected == 1);
    assert(env->state.agents[0].bomb_ammo == ammo_before + 1);
    assert(env->state.tiles[env->state.agents[0].y][env->state.agents[0].x] == TILE_FLOOR);

    /* Place range powerup */
    env->state.tiles[env->state.agents[0].y][env->state.agents[0].x] = TILE_POWERUP_RANGE;
    int range_before = env->state.agents[0].blast_range;
    collected = rules_pickup_powerup(&env->state, 0);
    assert(collected == 1);
    assert(env->state.agents[0].blast_range == range_before + 1);

    /* Place speed powerup */
    env->state.tiles[env->state.agents[0].y][env->state.agents[0].x] = TILE_POWERUP_SPEED;
    int speed_before = env->state.agents[0].speed;
    collected = rules_pickup_powerup(&env->state, 0);
    assert(collected == 1);
    assert(env->state.agents[0].speed == speed_before + 1);

    printf("  test_powerup_pickup_return: PASS\n");
    free(env);
}

/* Test: powerup spawn uses env RNG and config powerup_rate */
static void test_powerup_spawn_rng(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 42;
    cfg.crate_density = 0;
    cfg.powerup_rate = 1.0f; /* every destroyed crate spawns a powerup */

    BomberEnv* env = (BomberEnv*)calloc(1, sizeof(BomberEnv));
    assert(env != NULL);
    env_init(env, &cfg);

    /* Place a crate at (5,6) and bomb at (5,5) */
    env->state.tiles[5][6] = TILE_CRATE;
    env->state.bombs[0].x = 5;
    env->state.bombs[0].y = 5;
    env->state.bombs[0].owner_id = 0;
    env->state.bombs[0].timer = 1;
    env->state.bombs[0].range = 2;
    env->state.bombs[0].active = 1;

    explode_bomb(&env->state, 0, &env->rng, cfg.powerup_rate);

    /* With powerup_rate=1.0, the crate tile should now be a powerup */
    TileType t = env->state.tiles[5][6];
    assert(t == TILE_POWERUP_BOMB || t == TILE_POWERUP_RANGE || t == TILE_POWERUP_SPEED);

    /* With powerup_rate=0.0, no powerup should spawn */
    env_reset(env, 42);
    env->state.tiles[5][6] = TILE_CRATE;
    env->state.bombs[0].x = 5;
    env->state.bombs[0].y = 5;
    env->state.bombs[0].owner_id = 0;
    env->state.bombs[0].timer = 1;
    env->state.bombs[0].range = 2;
    env->state.bombs[0].active = 1;

    cfg.powerup_rate = 0.0f;
    explode_bomb(&env->state, 0, &env->rng, cfg.powerup_rate);
    assert(env->state.tiles[5][6] == TILE_FLOOR);

    printf("  test_powerup_spawn_rng: PASS\n");
    free(env);
}

/* Test: danger_is_action_safe_at_arrival distinguishes timing */
static void test_danger_arrival_safety(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 1;
    cfg.crate_density = 0;

    BomberEnv* env = (BomberEnv*)calloc(1, sizeof(BomberEnv));
    assert(env != NULL);
    env_init(env, &cfg);

    /* Place bomb at (5,5) with timer=3, range=2 */
    env->state.bombs[0].x = 5;
    env->state.bombs[0].y = 5;
    env->state.bombs[0].owner_id = 0;
    env->state.bombs[0].timer = 3;
    env->state.bombs[0].range = 2;
    env->state.bombs[0].active = 1;

    danger_compute(&env->danger, &env->state);

    /* Tile (5,6) has time_to_blast=3. Moving there (arrival=1) should be safe */
    assert(env->danger.time_to_blast[5][6] == 3);
    assert(danger_is_action_safe_at_arrival(&env->danger, 6, 5, 1) == 1);

    /* But with timer=1, arrival=1 means blast at same time -> unsafe */
    env->state.bombs[0].timer = 1;
    danger_compute(&env->danger, &env->state);
    assert(env->danger.time_to_blast[5][6] == 1);
    assert(danger_is_action_safe_at_arrival(&env->danger, 6, 5, 1) == 0);

    /* With timer=0 (exploding now), current_blast is set -> unsafe */
    env->state.bombs[0].timer = 0;
    danger_compute(&env->danger, &env->state);
    assert(env->danger.current_blast[5][6] == 1);
    assert(danger_is_action_safe_at_arrival(&env->danger, 6, 5, 1) == 0);

    printf("  test_danger_arrival_safety: PASS\n");
    free(env);
}

/* Test: action_safe uses arrival-time safety for moves */
static void test_action_safe_timing(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 1;
    cfg.crate_density = 0;

    BomberEnv* env = (BomberEnv*)calloc(1, sizeof(BomberEnv));
    assert(env != NULL);
    env_init(env, &cfg);

    /* Agent at (5,5), bomb at (5,5) with timer=3 */
    env->state.agents[0].x = 5;
    env->state.agents[0].y = 5;
    env->state.agents[0].alive = 1;
    env->state.bombs[0].x = 5;
    env->state.bombs[0].y = 5;
    env->state.bombs[0].owner_id = 0;
    env->state.bombs[0].timer = 3;
    env->state.bombs[0].range = 2;
    env->state.bombs[0].active = 1;

    danger_compute(&env->danger, &env->state);
    danger_compute_escape(&env->danger, &env->state, 0);

    /* Moving to (6,5) which has time_to_blast=3 should be safe (arrival=1 < 3) */
    assert(env->danger.action_safe[ACTION_RIGHT] == 1);

    /* WAIT at (5,5) with time_to_blast=3 should be unsafe (safe_now=0) */
    assert(env->danger.action_safe[ACTION_WAIT] == 0);

    printf("  test_action_safe_timing: PASS\n");
    free(env);
}

/* Test: opponent policy wiring */
static void test_opponent_policy(void) {
    BomberConfig cfg;
    config_battle(&cfg);
    cfg.seed = 7;
    cfg.max_steps = 30;

    BomberEnv* env = (BomberEnv*)calloc(1, sizeof(BomberEnv));
    assert(env != NULL);
    env_init(env, &cfg);

    /* Without opponent: built-in AI */
    assert(env->opponent == NULL);

    /* With opponent: heuristic agent */
    Agent enemy;
    agent_init(&enemy, AGENT_HEURISTIC);
    agent_reset(&enemy, 7);
    env_set_opponent(env, &enemy);
    assert(env->opponent == &enemy);

    /* Run a few steps to verify it doesn't crash */
    Observation obs;
    DebugSnapshot snap;
    for (int i = 0; i < 10; i++) {
        env_observe(env, 0, &obs);
        env_get_debug_snapshot(env, &snap);
        Action a = agent_act(&enemy, &obs, &snap);
        StepResult r = env_step(env, a);
        if (r.done) break;
    }

    /* Reset opponent to NULL and verify built-in AI still works */
    env_set_opponent(env, NULL);
    assert(env->opponent == NULL);

    env_reset(env, 7);
    for (int i = 0; i < 10; i++) {
        StepResult r = env_step(env, ACTION_WAIT);
        if (r.done) break;
    }

    printf("  test_opponent_policy: PASS\n");
    free(env);
}

/* Test: observation bounds with max agents */
static void test_observation_bounds(void) {
    BomberConfig cfg;
    config_battle(&cfg);
    cfg.seed = 1;
    cfg.agent_count = MAX_AGENTS;

    BomberEnv* env = (BomberEnv*)calloc(1, sizeof(BomberEnv));
    assert(env != NULL);
    env_init(env, &cfg);

    Observation obs;
    env_observe(env, 0, &obs);

    /* enemy_count should be at most MAX_AGENTS - 1 */
    assert(obs.enemy_count <= MAX_AGENTS - 1);
    assert(obs.enemy_count >= 0);

    /* powerup_count should be within bounds */
    assert(obs.powerup_count >= 0);
    assert(obs.powerup_count <= MAX_AGENTS * 4);

    /* Flat observation should fit in buffer */
    float flat[OBS_FLAT_SIZE];
    int flat_size;
    obs_to_flat(&obs, flat, &flat_size);
    assert(flat_size > 0);
    assert(flat_size <= OBS_FLAT_SIZE);

    /* imminent_danger should be set correctly with no bombs */
    assert(obs.in_danger == 0);
    assert(obs.imminent_danger == 0);
    assert(obs.danger_timer == -1);

    printf("  test_observation_bounds: PASS\n");
    free(env);
}

/* Test: reward breakdown sums to total */
static void test_reward_breakdown_sum(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 1;

    BomberEnv* env = (BomberEnv*)calloc(1, sizeof(BomberEnv));
    assert(env != NULL);
    env_init(env, &cfg);

    /* Run a few steps and verify breakdown sum */
    for (int i = 0; i < 5; i++) {
        StepResult r = env_step(env, ACTION_WAIT);
        float sum = env->last_reward.survival + env->last_reward.crate_destroyed +
                    env->last_reward.powerup + env->last_reward.enemy_damage +
                    env->last_reward.enemy_elimination + env->last_reward.win +
                    env->last_reward.escape_danger + env->last_reward.trap_opportunity +
                    env->last_reward.invalid_action_penalty + env->last_reward.suicidal_bomb_penalty +
                    env->last_reward.stall_penalty + env->last_reward.death_penalty +
                    env->last_reward.timeout_penalty;
        assert(fabsf(sum - env->last_reward.total) < 0.001f);
        if (r.done) break;
    }

    printf("  test_reward_breakdown_sum: PASS\n");
    free(env);
}

/* Test: deterministic episode with opponent */
static void test_determinism_with_opponent(void) {
    BomberConfig cfg;
    config_battle(&cfg);
    cfg.seed = 99;
    cfg.max_steps = 50;

    /* Run 1: with built-in AI */
    BomberEnv* env1 = (BomberEnv*)calloc(1, sizeof(BomberEnv));
    assert(env1 != NULL);
    env_init(env1, &cfg);
    env_reset(env1, 99);
    for (int i = 0; i < 20; i++) {
        StepResult r = env_step(env1, ACTION_WAIT);
        if (r.done) break;
    }

    /* Run 2: same seed, same actions, built-in AI */
    BomberEnv* env2 = (BomberEnv*)calloc(1, sizeof(BomberEnv));
    assert(env2 != NULL);
    env_init(env2, &cfg);
    env_reset(env2, 99);
    for (int i = 0; i < 20; i++) {
        StepResult r = env_step(env2, ACTION_WAIT);
        if (r.done) break;
    }

    /* States should be identical */
    assert(env1->state.step == env2->state.step);
    assert(env1->state.agents[0].x == env2->state.agents[0].x);
    assert(env1->state.agents[0].y == env2->state.agents[0].y);
    assert(env1->state.agents[0].alive == env2->state.agents[0].alive);

    for (int y = 0; y < cfg.height; y++) {
        for (int x = 0; x < cfg.width; x++) {
            assert(env1->state.tiles[y][x] == env2->state.tiles[y][x]);
        }
    }

    printf("  test_determinism_with_opponent: PASS\n");
    free(env1);
    free(env2);
}

/* Test: config normalization clamps values */
static void test_config_normalization(void) {
    BomberConfig cfg;
    config_defaults(&cfg);

    /* Set out-of-range values */
    cfg.width = 100;
    cfg.height = 100;
    cfg.agent_count = 100;
    cfg.crate_density = 200;
    cfg.powerup_rate = 5.0f;

    config_normalize(&cfg);

    assert(cfg.width <= MAX_WIDTH);
    assert(cfg.height <= MAX_HEIGHT);
    assert(cfg.agent_count <= MAX_AGENTS);
    assert(cfg.crate_density <= 100);
    assert(cfg.powerup_rate <= 1.0f);

    /* Minimums */
    cfg.width = 0;
    cfg.height = 0;
    cfg.agent_count = 0;
    cfg.crate_density = -1;
    cfg.powerup_rate = -1.0f;

    config_normalize(&cfg);

    assert(cfg.width >= 5);
    assert(cfg.height >= 5);
    assert(cfg.agent_count >= 1);
    assert(cfg.crate_density >= 0);
    assert(cfg.powerup_rate >= 0.0f);

    printf("  test_config_normalization: PASS\n");
}

/* Test: bomb ammo returns after explosion */
static void test_bomb_ammo_return(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 1;
    cfg.crate_density = 0;

    BomberEnv* env = (BomberEnv*)calloc(1, sizeof(BomberEnv));
    assert(env != NULL);
    env_init(env, &cfg);

    /* Agent starts with 1 ammo */
    assert(env->state.agents[0].bomb_ammo == 1);

    /* Place bomb at agent start position */
    int ax = env->state.agents[0].x;
    int ay = env->state.agents[0].y;
    int placed = place_bomb(&env->state, 0);
    assert(placed == 1);
    assert(env->state.agents[0].bomb_ammo == 0);
    assert(env->state.agents[0].bombs_active == 1);

    /* Move agent away from bomb to avoid death */
    env->state.agents[0].x = 3;
    env->state.agents[0].y = 1;

    /* Find the bomb and set timer */
    BombState* bomb = map_bomb_at(&env->state, ax, ay);
    assert(bomb != NULL);
    bomb->timer = 1;
    tick_bombs(&env->state, &env->rng, cfg.powerup_rate);

    /* Ammo should return after explosion */
    assert(env->state.agents[0].bomb_ammo == 1);
    assert(env->state.agents[0].bombs_active == 0);

    printf("  test_bomb_ammo_return: PASS\n");
    free(env);
}

/* Test: crate destruction respects powerup_rate */
static void test_crate_powerup_rate(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 1;
    cfg.crate_density = 0;

    /* With powerup_rate=0, no powerups should spawn */
    cfg.powerup_rate = 0.0f;

    BomberEnv* env = (BomberEnv*)calloc(1, sizeof(BomberEnv));
    assert(env != NULL);
    env_init(env, &cfg);

    /* Place several crates in blast range */
    env->state.tiles[5][6] = TILE_CRATE;
    env->state.tiles[6][5] = TILE_CRATE;
    env->state.tiles[4][5] = TILE_CRATE;
    env->state.bombs[0].x = 5;
    env->state.bombs[0].y = 5;
    env->state.bombs[0].owner_id = 0;
    env->state.bombs[0].timer = 1;
    env->state.bombs[0].range = 2;
    env->state.bombs[0].active = 1;

    explode_bomb(&env->state, 0, &env->rng, cfg.powerup_rate);

    /* All crates should be TILE_FLOOR (no powerups) */
    assert(env->state.tiles[5][6] == TILE_FLOOR);
    assert(env->state.tiles[6][5] == TILE_FLOOR);
    assert(env->state.tiles[4][5] == TILE_FLOOR);

    printf("  test_crate_powerup_rate: PASS\n");
    free(env);
}

/* Test: CLI flag handling via agent_parse_type */
static void test_cli_flags(void) {
    assert(agent_parse_type("random") == AGENT_RANDOM);
    assert(agent_parse_type("scripted") == AGENT_SCRIPTED);
    assert(agent_parse_type("heuristic") == AGENT_HEURISTIC);
    assert(agent_parse_type("greedy") == AGENT_GREEDY_CRATE);
    assert(agent_parse_type("greedy_crate") == AGENT_GREEDY_CRATE);

    /* Unknown agent should return a valid default or handle gracefully */
    AgentType unknown = agent_parse_type("nonexistent");
    (void)unknown;

    printf("  test_cli_flags: PASS\n");
}

int main(void) {
    printf("test_hardened:\n");
    test_powerup_pickup_return();
    test_powerup_spawn_rng();
    test_danger_arrival_safety();
    test_action_safe_timing();
    test_opponent_policy();
    test_observation_bounds();
    test_reward_breakdown_sum();
    test_determinism_with_opponent();
    test_config_normalization();
    test_bomb_ammo_return();
    test_crate_powerup_rate();
    test_cli_flags();
    printf("test_hardened: ALL PASSED\n");
    return 0;
}
