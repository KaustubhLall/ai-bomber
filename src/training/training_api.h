#ifndef BOMBER_TRAINING_API_H
#define BOMBER_TRAINING_API_H

#include <stdint.h>
#include "training/encoding.h"

#if defined(_WIN32) && defined(bomber_training_EXPORTS)
#define BOMBER_TRAINING_API __declspec(dllexport)
#elif defined(_WIN32)
#define BOMBER_TRAINING_API __declspec(dllimport)
#else
#define BOMBER_TRAINING_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BOMBER_TRAINING_ABI_VERSION 6
typedef struct BomberTrainingEnv BomberTrainingEnv;
typedef struct BomberTrainingAgent BomberTrainingAgent;

BOMBER_TRAINING_API int bomber_training_abi_version(void);
BOMBER_TRAINING_API int bomber_training_channels(void);
BOMBER_TRAINING_API int bomber_training_actions(void);

BOMBER_TRAINING_API BomberTrainingEnv* bomber_training_create(
    int width, int height, int max_steps, int crate_density, uint64_t seed);
BOMBER_TRAINING_API BomberTrainingEnv* bomber_training_clone(
    const BomberTrainingEnv* source);
BOMBER_TRAINING_API void bomber_training_destroy(BomberTrainingEnv* training_env);
BOMBER_TRAINING_API void bomber_training_reset(BomberTrainingEnv* training_env,
                                                uint64_t seed);

BOMBER_TRAINING_API int bomber_training_width(const BomberTrainingEnv* training_env);
BOMBER_TRAINING_API int bomber_training_height(const BomberTrainingEnv* training_env);
BOMBER_TRAINING_API int bomber_training_step_count(const BomberTrainingEnv* training_env);
BOMBER_TRAINING_API int bomber_training_observation_size(
    const BomberTrainingEnv* training_env);
BOMBER_TRAINING_API int bomber_training_encode(const BomberTrainingEnv* training_env,
                                               int perspective,
                                               float* output,
                                               int output_count);
BOMBER_TRAINING_API int bomber_training_legal_actions(
    const BomberTrainingEnv* training_env, int perspective, int* output_mask,
    int output_count);
BOMBER_TRAINING_API int bomber_training_safe_actions(
    const BomberTrainingEnv* training_env, int perspective, int* output_mask,
    int output_count);

/* Returns 1 when the match is terminal, 0 while play can continue, and -1 on
   invalid input. Joint actions use the public six-action ordering. */
BOMBER_TRAINING_API int bomber_training_step_joint(BomberTrainingEnv* training_env,
                                                   int player_zero_action,
                                                   int player_one_action);
/* Terminal value from perspective: +1 win, -1 loss, 0 draw/timeout/nonterminal. */
BOMBER_TRAINING_API int bomber_training_outcome(
    const BomberTrainingEnv* training_env, int perspective);
BOMBER_TRAINING_API uint64_t bomber_training_state_hash(
    const BomberTrainingEnv* training_env);
BOMBER_TRAINING_API float bomber_training_tactical_value(
    const BomberTrainingEnv* training_env, int perspective);

/* Persistent native baseline policy. Type values use AgentType from agent.h:
   random=0, scripted=1, heuristic=2, greedy=3, alpha-beta=6, MCTS=7, evasive=8. */
BOMBER_TRAINING_API BomberTrainingAgent* bomber_training_agent_create(
    int agent_type, uint64_t seed);
BOMBER_TRAINING_API void bomber_training_agent_destroy(BomberTrainingAgent* agent);
BOMBER_TRAINING_API int bomber_training_agent_action(BomberTrainingAgent* agent,
                                                     const BomberTrainingEnv* training_env,
                                                     int perspective);
BOMBER_TRAINING_API int bomber_training_agent_configure_mcts(
    BomberTrainingAgent* agent, int simulations, int rollout_depth);
BOMBER_TRAINING_API int bomber_training_baseline_action(
    int agent_type, const BomberTrainingEnv* training_env, int perspective,
    uint64_t seed);

#ifdef __cplusplus
}
#endif

#endif
