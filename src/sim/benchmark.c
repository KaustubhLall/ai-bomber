#include "sim/benchmark.h"
#include "sim/runner.h"
#include "agents/agent.h"
#include <stdio.h>
#include <string.h>

void benchmark_run(const BomberConfig* cfg, int episodes, uint64_t seed, Metrics* metrics) {
    RunConfig rc;
    rc.config = *cfg;
    rc.agent_type = AGENT_HEURISTIC;
    rc.seed = seed;
    rc.episodes = episodes;
    rc.record_replay = 0;

    runner_run(&rc, metrics);

    printf("\n=== Benchmark Results ===\n");
    printf("Episodes: %d\n", episodes);
    metrics_print(metrics);
}

static void emit_row(FILE* f, int json, int* first, const char* suite, uint64_t seed,
                     AgentType a, AgentType b, const Metrics* m) {
    double eps=m->episodes?m->episodes:1, seconds=m->total_time_ms/1000.0;
    if(json){if(!*first)fprintf(f,",\n");*first=0;fprintf(f,"    {\"seed_suite\":\"%s\",\"seed\":%llu,\"agent\":\"%s\",\"opponent\":\"%s\",\"episodes\":%d,\"win_rate\":%.6f,\"death_rate\":%.6f,\"draw_timeout_rate\":%.6f,\"avg_reward\":%.6f,\"avg_length\":%.6f,\"crates_destroyed\":%d,\"owned_eliminations\":%d,\"self_kills\":%d,\"opponent_self_kills\":%d,\"opponent_kills\":%d,\"bombs_placed\":%d,\"powerups_collected\":%d,\"steps_per_sec\":%.3f}",suite,(unsigned long long)seed,agent_type_name(a),agent_type_name(b),m->episodes,m->wins/eps,m->deaths/eps,(m->draws+m->timeouts)/eps,m->total_reward/eps,m->total_steps/eps,m->crates_destroyed,m->enemies_killed,m->self_kills,m->opponent_self_kills,m->opponent_kills,m->bombs_placed,m->powerups_collected,seconds>0?m->total_steps/seconds:0.0);
    }else fprintf(f,"%s,%llu,%s,%s,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%d,%d,%d,%d,%d,%d,%.3f\n",suite,(unsigned long long)seed,agent_type_name(a),agent_type_name(b),m->episodes,m->wins/eps,m->deaths/eps,(m->draws+m->timeouts)/eps,m->total_reward/eps,m->total_steps/eps,m->crates_destroyed,m->enemies_killed,m->self_kills,m->opponent_self_kills,m->opponent_kills,m->bombs_placed,m->powerups_collected,seconds>0?m->total_steps/seconds:0.0);
}

int benchmark_matrix(const BomberConfig* cfg,int episodes,uint64_t seed,const char*suite,const char*path,int json){
    FILE*f=fopen(path,"w");if(!f)return 0;AgentType types[]={AGENT_RANDOM,AGENT_GREEDY_CRATE,AGENT_HEURISTIC,AGENT_ALPHABETA,AGENT_MCTS};int n=5,first=1;
    if(json)fprintf(f,"{\n  \"schema_version\": 2,\n  \"git_sha\": \"%s\",\n  \"run_config\": {\"episodes_per_matchup\":%d,\"seed\":%llu,\"seed_suite\":\"%s\",\"map\":{\"width\":%d,\"height\":%d,\"crate_density\":%d,\"bomb_timer\":%d,\"max_steps\":%d}},\n  \"hardware\": \"native-c17\",\n  \"policy_config\": {\"alpha_beta_depth\":2,\"mcts_variant\":\"root_ucb_adversarial_rollout\",\"mcts_simulations\":32,\"mcts_rollout_depth\":4},\n  \"results\": [\n",AI_BOMBER_GIT_SHA,episodes,(unsigned long long)seed,suite,cfg->width,cfg->height,cfg->crate_density,cfg->bomb_timer,cfg->max_steps);else fprintf(f,"seed_suite,seed,agent,opponent,episodes,win_rate,death_rate,draw_timeout_rate,avg_reward,avg_length,crates_destroyed,owned_eliminations,self_kills,opponent_self_kills,opponent_kills,bombs_placed,powerups_collected,steps_per_sec\n");
    for(int i=0;i<n;i++)for(int j=0;j<n;j++){RunConfig rc={0};rc.config=*cfg;rc.agent_type=types[i];rc.enemy_type=types[j];rc.seed=seed;rc.episodes=episodes;Metrics m;runner_run(&rc,&m);emit_row(f,json,&first,suite,seed,types[i],types[j],&m);}
    if(json)fprintf(f,"\n  ]\n}\n");fclose(f);return 1;
}
