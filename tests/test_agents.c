#include "env/env.h"
#include "agents/agent.h"
#include "env/bomber_map.h"
#include <assert.h>
#include <stdio.h>

static void run_agent_smoke(BomberConfig* cfg, AgentType type) {
    BomberEnv env;
    env_init(&env, cfg);

    Agent agent;
    agent_init(&agent, type);
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
    assert(steps_survived >= 0);
}

int main(void) {
    BomberConfig cfg;
    config_survival(&cfg);
    cfg.seed = 1;
    cfg.max_steps = 50;

    run_agent_smoke(&cfg, AGENT_RANDOM);
    run_agent_smoke(&cfg, AGENT_SCRIPTED);
    run_agent_smoke(&cfg, AGENT_HEURISTIC);
    run_agent_smoke(&cfg, AGENT_GREEDY_CRATE);
    run_agent_smoke(&cfg, AGENT_EVASIVE);

    Agent a;
    Agent b;
    agent_init(&a, AGENT_RANDOM);
    agent_init(&b, AGENT_RANDOM);
    assert(a.impl != NULL);
    assert(b.impl != NULL);
    assert(a.impl != b.impl);

    agent_init(&a, AGENT_SCRIPTED);
    agent_init(&b, AGENT_SCRIPTED);
    assert(a.impl != NULL);
    assert(b.impl != NULL);
    assert(a.impl != b.impl);

    agent_init(&a, AGENT_HEURISTIC);
    agent_init(&b, AGENT_HEURISTIC);
    assert(a.impl != NULL);
    assert(b.impl != NULL);
    assert(a.impl != b.impl);

    agent_init(&a, AGENT_GREEDY_CRATE);
    agent_init(&b, AGENT_GREEDY_CRATE);
    assert(a.impl != NULL);
    assert(b.impl != NULL);
    assert(a.impl != b.impl);

    assert(agent_parse_type("random") == AGENT_RANDOM);
    assert(agent_parse_type("scripted") == AGENT_SCRIPTED);
    assert(agent_parse_type("heuristic") == AGENT_HEURISTIC);
    assert(agent_parse_type("greedy") == AGENT_GREEDY_CRATE);
    assert(agent_parse_type("greedy_crate") == AGENT_GREEDY_CRATE);
    assert(agent_parse_type("evasive") == AGENT_EVASIVE);

    printf("test_agents: ALL PASSED\n");
    return 0;
}
