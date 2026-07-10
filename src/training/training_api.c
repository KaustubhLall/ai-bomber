#include "training/training_api.h"

#include "core/config.h"
#include "agents/agent.h"
#include "agents/search_agent.h"
#include "env/env.h"
#include "env/bomber_rules.h"
#include "sim/evaluator.h"

#include <stdlib.h>
#include <string.h>

struct BomberTrainingEnv {
    BomberEnv env;
};

struct BomberTrainingAgent {
    Agent agent;
};

int bomber_training_abi_version(void) { return BOMBER_TRAINING_ABI_VERSION; }
int bomber_training_channels(void) { return BOMBER_TRAINING_CHANNELS; }
int bomber_training_actions(void) { return BOMBER_TRAINING_ACTIONS; }

BomberTrainingEnv* bomber_training_create(int width, int height, int max_steps,
                                          int crate_density, uint64_t seed) {
    BomberTrainingEnv* result = (BomberTrainingEnv*)calloc(1, sizeof(*result));
    if (!result) return NULL;
    BomberConfig config;
    config_battle(&config);
    config.width = width;
    config.height = height;
    config.max_steps = max_steps;
    config.crate_density = crate_density;
    config.seed = (int)(seed & UINT64_C(0x7fffffff));
    config_normalize(&config);
    env_init(&result->env, &config);
    env_reset(&result->env, seed);
    return result;
}

BomberTrainingEnv* bomber_training_clone(const BomberTrainingEnv* source) {
    if (!source) return NULL;
    BomberTrainingEnv* result = (BomberTrainingEnv*)malloc(sizeof(*result));
    if (!result) return NULL;
    env_copy(&result->env, &source->env);
    result->env.opponent = NULL;
    return result;
}

void bomber_training_destroy(BomberTrainingEnv* training_env) { free(training_env); }

void bomber_training_reset(BomberTrainingEnv* training_env, uint64_t seed) {
    if (training_env) env_reset(&training_env->env, seed);
}

int bomber_training_width(const BomberTrainingEnv* training_env) {
    return training_env ? training_env->env.state.width : 0;
}

int bomber_training_height(const BomberTrainingEnv* training_env) {
    return training_env ? training_env->env.state.height : 0;
}

int bomber_training_step_count(const BomberTrainingEnv* training_env) {
    return training_env ? training_env->env.state.step : 0;
}

int bomber_training_observation_size(const BomberTrainingEnv* training_env) {
    return training_env ? BOMBER_TRAINING_VIEW_SIZE * BOMBER_TRAINING_VIEW_SIZE *
                              BOMBER_TRAINING_CHANNELS
                        : 0;
}

int bomber_training_encode(const BomberTrainingEnv* training_env, int perspective,
                           float* output, int output_count) {
    return training_env ? bomber_training_encode_env(&training_env->env, perspective,
                                                      output, output_count) : 0;
}

int bomber_training_legal_actions(const BomberTrainingEnv* training_env, int perspective,
                                  int* output_mask, int output_count) {
    if (!training_env || !output_mask || output_count < BOMBER_TRAINING_ACTIONS ||
        perspective < 0 || perspective >= 2) return 0;
    memset(output_mask, 0, (size_t)BOMBER_TRAINING_ACTIONS * sizeof(*output_mask));
    Action legal[ACTION_COUNT];
    int count = 0;
    env_legal_actions(&training_env->env, perspective, legal, &count);
    for (int i = 0; i < count; i++) output_mask[(int)legal[i]] = 1;
    return count;
}

int bomber_training_safe_actions(const BomberTrainingEnv* training_env, int perspective,
                                 int* output_mask, int output_count) {
    return training_env ? bomber_training_safe_actions_env(&training_env->env, perspective,
                                                            output_mask, output_count) : 0;
}

int bomber_training_step_joint(BomberTrainingEnv* training_env, int player_zero_action,
                               int player_one_action) {
    if (!training_env || player_zero_action < 0 || player_zero_action >= ACTION_COUNT ||
        player_one_action < 0 || player_one_action >= ACTION_COUNT) return -1;
    Action actions[2] = {(Action)player_zero_action, (Action)player_one_action};
    StepResult result = env_step_joint(&training_env->env, actions, 2);
    return result.done ? 1 : 0;
}

int bomber_training_outcome(const BomberTrainingEnv* training_env, int perspective) {
    if (!training_env || perspective < 0 || perspective >= 2) return 0;
    const BomberState* state = &training_env->env.state;
    int opponent = perspective == 0 ? 1 : 0;
    int self_alive = state->agents[perspective].alive;
    int other_alive = state->agents[opponent].alive;
    if (self_alive && !other_alive) return 1;
    if (!self_alive && other_alive) return -1;
    return 0;
}

uint64_t bomber_training_state_hash(const BomberTrainingEnv* training_env) {
    return training_env ? env_state_hash(&training_env->env) : 0;
}

float bomber_training_tactical_value(const BomberTrainingEnv* training_env,
                                     int perspective) {
    if (!training_env || perspective < 0 || perspective >= 2) return 0.0f;
    BomberEnv copy;
    env_copy(&copy, &training_env->env);
    /* Preserve a meaningful positional score at the time limit. The public
       evaluator intentionally maps timeouts to a flat loss-like score, which
       is right for benchmark claims but useless as a draw tiebreak target. */
    if (copy.state.step >= copy.config.max_steps)
        copy.config.max_steps = copy.state.step + 1;
    return evaluator_score_state(&copy, perspective);
}

BomberTrainingAgent* bomber_training_agent_create(int agent_type, uint64_t seed) {
    if (agent_type < AGENT_RANDOM || agent_type > AGENT_EVASIVE ||
        agent_type == AGENT_EXTERNAL) return NULL;
    BomberTrainingAgent* result = (BomberTrainingAgent*)calloc(1, sizeof(*result));
    if (!result) return NULL;
    agent_init(&result->agent, (AgentType)agent_type);
    agent_reset(&result->agent, seed);
    return result;
}

void bomber_training_agent_destroy(BomberTrainingAgent* agent) { free(agent); }

int bomber_training_agent_action(BomberTrainingAgent* agent,
                                 const BomberTrainingEnv* training_env,
                                 int perspective) {
    if (!agent || !training_env || perspective < 0 || perspective >= 2) return -1;
    Observation observation;
    DebugSnapshot snapshot;
    env_observe(&training_env->env, perspective, &observation);
    env_get_debug_snapshot(&training_env->env, &snapshot);
    return (int)agent_act(&agent->agent, &observation, &snapshot);
}

int bomber_training_agent_configure_mcts(BomberTrainingAgent* agent,
                                          int simulations, int rollout_depth) {
    return agent ? mcts_agent_configure(&agent->agent, simulations, rollout_depth) : 0;
}

int bomber_training_baseline_action(int agent_type,
                                    const BomberTrainingEnv* training_env,
                                    int perspective, uint64_t seed) {
    if (!training_env || perspective < 0 || perspective >= 2 ||
        agent_type < AGENT_RANDOM || agent_type > AGENT_EVASIVE ||
        agent_type == AGENT_EXTERNAL) return -1;
    Agent agent;
    agent_init(&agent, (AgentType)agent_type);
    agent_reset(&agent, seed);
    Observation observation;
    DebugSnapshot snapshot;
    env_observe(&training_env->env, perspective, &observation);
    env_get_debug_snapshot(&training_env->env, &snapshot);
    return (int)agent_act(&agent, &observation, &snapshot);
}
