#ifndef BOMBER_SEARCH_AGENT_H
#define BOMBER_SEARCH_AGENT_H
#include "agents/agent.h"
void alphabeta_agent_init(Agent* agent);
void mcts_agent_init(Agent* agent);
int mcts_agent_configure(Agent* agent, int simulations, int rollout_depth);
int search_bomb_is_robustly_safe(const DebugSnapshot* debug, int actor);
Action search_robust_escape_action(const DebugSnapshot* debug, int actor, int* found);
#endif
