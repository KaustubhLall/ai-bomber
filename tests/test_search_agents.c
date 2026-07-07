#include "agents/agent.h"
#include "sim/evaluator.h"
#include <assert.h>
#include <stdio.h>

static int contains(const Action* actions, int count, Action wanted) {
    for (int i = 0; i < count; i++) if (actions[i] == wanted) return 1;
    return 0;
}

int main(void) {
    BomberConfig config;
    config_battle(&config);
    config.agent_count = 2;
    BomberEnv env = {0};
    env_init(&env, &config);

    float safe = evaluator_score_state(&env, 0);
    env.state.agents[0].alive = 0;
    assert(evaluator_score_state(&env, 0) < safe);
    env_reset(&env, 77);
    env.state.step = env.config.max_steps;
    assert(evaluator_score_state(&env, 0) < 0.0f);
    env_reset(&env, 77);

    /* The offensive evaluator distinguishes a forced blast from an escapable one. */
    for (int y = 0; y < env.state.height; y++) for (int x = 0; x < env.state.width; x++)
        env.state.tiles[y][x] = TILE_SOLID_WALL;
    env.state.agents[0].x = 3; env.state.agents[0].y = 2;
    env.state.agents[1].x = 3; env.state.agents[1].y = 3;
    env.state.tiles[2][3] = env.state.tiles[3][3] = TILE_FLOOR;
    env.state.bombs[0] = (BombState){3, 2, 0, 1, 2, 1};
    env.state.agents[0].bombs_active = 1;
    danger_compute(&env.danger, &env.state);
    assert(evaluator_opponent_escape_options(&env, 0) == 0);
    env.state.tiles[3][4] = TILE_FLOOR;
    danger_compute(&env.danger, &env.state);
    assert(evaluator_opponent_escape_options(&env, 0) > 0);
    env_reset(&env, 77);

    DebugSnapshot debug;
    Observation observation;
    Agent agent;
    env_observe(&env, 0, &observation);
    env_get_debug_snapshot(&env, &debug);
    agent_init(&agent, AGENT_ALPHABETA);
    Action action = agent_act(&agent, &observation, &debug);
    assert(action >= 0 && action < ACTION_COUNT && agent.diagnostics.nodes > 0);

    agent_init(&agent, AGENT_MCTS);
    action = agent_act(&agent, &observation, &debug);
    assert(action >= 0 && action < ACTION_COUNT && agent.diagnostics.simulations == 96);
    assert(agent.diagnostics.depth == 12 && agent.diagnostics.nodes > 1);

    /* Search agents acting as enemy 1 must choose from enemy 1's legal actions. */
    env_observe(&env, 1, &observation);
    assert(observation.agent_id == 1);
    /* Safety belongs to the observed agent, not the global agent-0 danger map. */
    BomberEnv enemy_view = env;
    DangerMap expected_danger;
    danger_compute(&expected_danger, &enemy_view.state);
    danger_compute_escape(&expected_danger, &enemy_view.state, 1);
    for (int i = 0; i < ACTION_COUNT; i++) assert(observation.safe_actions[i] == expected_danger.action_safe[i]);
    env_get_debug_snapshot(&env, &debug);
    Action legal[ACTION_COUNT]; int legal_count = 0;
    env_legal_actions(&env, 1, legal, &legal_count);
    agent_init(&agent, AGENT_MCTS);
    action = agent_act(&agent, &observation, &debug);
    assert(contains(legal, legal_count, action));

    puts("search agent tests passed");
    return 0;
}
