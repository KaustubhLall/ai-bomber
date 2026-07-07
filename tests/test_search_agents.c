#include "agents/agent.h"
#include "sim/evaluator.h"
#include <assert.h>
#include <stdio.h>
int main(void){BomberConfig c;config_battle(&c);c.agent_count=2;BomberEnv e={0};env_init(&e,&c);DebugSnapshot d;Observation o;env_observe(&e,0,&o);env_get_debug_snapshot(&e,&d);float safe=evaluator_score_state(&e,0);e.state.agents[0].alive=0;assert(evaluator_score_state(&e,0)<safe);Agent a;agent_init(&a,AGENT_ALPHABETA);Action x=agent_act(&a,&o,&d);assert(x>=0&&x<ACTION_COUNT&&a.diagnostics.nodes>0);agent_init(&a,AGENT_MCTS);x=agent_act(&a,&o,&d);assert(x>=0&&x<ACTION_COUNT&&a.diagnostics.simulations==48);puts("search agent tests passed");return 0;}
