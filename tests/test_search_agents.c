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
    assert(action >= 0 && action < ACTION_COUNT && agent.diagnostics.simulations == 32);

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
