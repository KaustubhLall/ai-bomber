#include "agents/search_agent.h"
#include "sim/evaluator.h"
#include "env/bomber_rules.h"
#include <float.h>
#include <string.h>

typedef struct { RNG rng; int budget; int depth; } SearchImpl;
static void reset_search(Agent* a, uint64_t seed) { SearchImpl* i=(SearchImpl*)a->impl; rng_init(&i->rng,seed); memset(&a->diagnostics,0,sizeof(a->diagnostics)); }
static void snapshot_env(const DebugSnapshot* d, BomberEnv* e) { memset(e,0,sizeof(*e)); e->state=d->state;e->config=d->config;e->rng=d->rng;e->danger=d->danger; }
static float max_search(BomberEnv* e,int depth,SearchDiagnostics* d) {
    d->nodes++; TerminalReason t=rules_check_terminal(&e->state,0,e->config.max_steps);
    if(depth<=0||t!=TERMINAL_NONE)return evaluator_score_state(e,0);
    Action legal[ACTION_COUNT];int count=0;env_legal_actions(e,0,legal,&count);float best=-FLT_MAX;
    for(int i=0;i<count;i++){BomberEnv c;env_copy(&c,e);Action joint[MAX_AGENTS]={ACTION_WAIT};joint[0]=legal[i];env_step_joint(&c,joint,c.state.agent_count);float v=max_search(&c,depth-1,d);if(v>best)best=v;if(best>9000.0f){d->prunes+=count-i-1;break;}}
    return best;
}
static Action alphabeta_act(Agent* a,const Observation* o,const DebugSnapshot* d0){(void)o;BomberEnv e;snapshot_env(d0,&e);SearchImpl*i=(SearchImpl*)a->impl;SearchDiagnostics*d=&a->diagnostics;memset(d,0,sizeof(*d));d->depth=i->depth;Action legal[ACTION_COUNT];int count=0;env_legal_actions(&e,0,legal,&count);float best=-FLT_MAX;Action chosen=ACTION_WAIT;for(int n=0;n<count;n++){BomberEnv c;env_copy(&c,&e);Action j[MAX_AGENTS]={ACTION_WAIT};j[0]=legal[n];env_step_joint(&c,j,c.state.agent_count);float v=max_search(&c,i->depth-1,d);d->action_values[legal[n]]=v;if(v>best){best=v;chosen=legal[n];}}d->selected_action=chosen;d->value=best;return chosen;}
static float rollout(BomberEnv*e,SearchImpl*i,int depth){for(int n=0;n<depth;n++){Action j[MAX_AGENTS]={ACTION_WAIT};for(int a=0;a<e->state.agent_count;a++){Action l[ACTION_COUNT];int c=0;env_legal_actions(e,a,l,&c);j[a]=c?l[rng_range(&i->rng,0,c)]:ACTION_WAIT;}StepResult r=env_step_joint(e,j,e->state.agent_count);if(r.done)break;}return evaluator_score_state(e,0);}
static Action mcts_act(Agent*a,const Observation*o,const DebugSnapshot*d0){(void)o;BomberEnv root;snapshot_env(d0,&root);SearchImpl*i=(SearchImpl*)a->impl;SearchDiagnostics*d=&a->diagnostics;memset(d,0,sizeof(*d));d->simulations=i->budget;Action legal[ACTION_COUNT];int count=0;env_legal_actions(&root,0,legal,&count);for(int s=0;s<i->budget&&count;s++){int p=s<count?s:rng_range(&i->rng,0,count);BomberEnv c;env_copy(&c,&root);Action j[MAX_AGENTS]={ACTION_WAIT};j[0]=legal[p];env_step_joint(&c,j,c.state.agent_count);float v=rollout(&c,i,i->depth);Action x=legal[p];d->visits[x]++;d->action_values[x]+=v;d->nodes+=i->depth+1;}Action chosen=ACTION_WAIT;float best=-FLT_MAX;for(int n=0;n<count;n++){Action x=legal[n];if(d->visits[x])d->action_values[x]/=d->visits[x];float u=d->action_values[x]+.01f*d->visits[x];if(u>best){best=u;chosen=x;}}d->selected_action=chosen;d->value=best;return chosen;}
static void init_common(Agent*a,const char*n,AgentActFn act){SearchImpl*i=(SearchImpl*)agent_impl_storage(a,sizeof(SearchImpl));a->impl=i;i->budget=48;i->depth=3;a->act=act;a->reset=reset_search;strncpy(a->name,n,sizeof(a->name)-1);reset_search(a,1);}
void alphabeta_agent_init(Agent*a){init_common(a,"alpha-beta",alphabeta_act);}void mcts_agent_init(Agent*a){init_common(a,"mcts",mcts_act);}
