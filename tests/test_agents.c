#include "env/env.h"
#include "agents/agent.h"
#include "env/bomber_map.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 1;
    cfg.max_steps = 50;

    /* Test random agent */
    BomberEnv env;
    env_init(&env, &cfg);

    Agent agent;
    agent_init(&agent, AGENT_RANDOM);
    agent_reset(&agent, 1);

    Observation obs;
    DebugSnapshot snap;

    int steps_survived = 0;
    for (int i = 0; i < 50; i++) {
        env_observe(&env, 0, &obs);
        env_get_debug_snapshot(&env, &snap);
        Action a = agent_act(&agent, &obs, &snap);
        StepResult r = env_step(&env, a);
        if (env.state.agents[0].alive) steps_survived++;
        if (r.done) break;
    }
    assert(steps_survived >= 0); /* smoke test: ran without crash */

    /* Test scripted agent */
    env_init(&env, &cfg);
    agent_init(&agent, AGENT_SCRIPTED);
    agent_reset(&agent, 1);

    steps_survived = 0;
    for (int i = 0; i < 50; i++) {
        env_observe(&env, 0, &obs);
        env_get_debug_snapshot(&env, &snap);
        Action a = agent_act(&agent, &obs, &snap);
        StepResult r = env_step(&env, a);
        if (env.state.agents[0].alive) steps_survived++;
        if (r.done) break;
    }
    assert(steps_survived >= 0);

    /* Test heuristic agent */
    env_init(&env, &cfg);
    agent_init(&agent, AGENT_HEURISTIC);
    agent_reset(&agent, 1);

    steps_survived = 0;
    for (int i = 0; i < 50; i++) {
        env_observe(&env, 0, &obs);
        env_get_debug_snapshot(&env, &snap);
        Action a = agent_act(&agent, &obs, &snap);
        StepResult r = env_step(&env, a);
        if (env.state.agents[0].alive) steps_survived++;
        if (r.done) break;
    }
    assert(steps_survived >= 0);

    /* Test greedy crate agent */
    env_init(&env, &cfg);
    agent_init(&agent, AGENT_GREEDY_CRATE);
    agent_reset(&agent, 1);

    steps_survived = 0;
    for (int i = 0; i < 50; i++) {
        env_observe(&env, 0, &obs);
        env_get_debug_snapshot(&env, &snap);
        Action a = agent_act(&agent, &obs, &snap);
        StepResult r = env_step(&env, a);
        if (env.state.agents[0].alive) steps_survived++;
        if (r.done) break;
    }
    assert(steps_survived >= 0);

    /* Test agent_parse_type */
    assert(agent_parse_type("random") == AGENT_RANDOM);
    assert(agent_parse_type("scripted") == AGENT_SCRIPTED);
    assert(agent_parse_type("heuristic") == AGENT_HEURISTIC);
    assert(agent_parse_type("greedy") == AGENT_GREEDY_CRATE);
    assert(agent_parse_type("greedy_crate") == AGENT_GREEDY_CRATE);

    printf("test_agents: ALL PASSED\n");
    return 0;
}
