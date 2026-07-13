#include "training/training_api.h"

#include <assert.h>
#include <stdlib.h>

int main(void) {
    assert(bomber_training_abi_version() == BOMBER_TRAINING_ABI_VERSION);
    assert(bomber_training_channels() == BOMBER_TRAINING_CHANNELS);
    assert(bomber_training_actions() == BOMBER_TRAINING_ACTIONS);

    BomberTrainingEnv* env = bomber_training_create(13, 11, 40, 50, 1337);
    assert(env != NULL);
    assert(bomber_training_width(env) == 13);
    assert(bomber_training_height(env) == 11);
    int size = bomber_training_observation_size(env);
    assert(size == BOMBER_TRAINING_VIEW_SIZE * BOMBER_TRAINING_VIEW_SIZE *
                   BOMBER_TRAINING_CHANNELS);
    float* encoded = (float*)calloc((size_t)size, sizeof(float));
    assert(encoded != NULL);
    assert(bomber_training_encode(env, 0, encoded, size) == size);
    int legal[BOMBER_TRAINING_ACTIONS];
    assert(bomber_training_legal_actions(env, 0, legal, BOMBER_TRAINING_ACTIONS) > 0);
    assert(legal[5] == 1); /* wait is always legal for a living agent */
    int safe[BOMBER_TRAINING_ACTIONS];
    assert(bomber_training_safe_actions(env, 0, safe, BOMBER_TRAINING_ACTIONS) > 0);
    for (int action = 0; action < BOMBER_TRAINING_ACTIONS; action++)
        assert(!safe[action] || legal[action]);

    uint64_t initial_hash = bomber_training_state_hash(env);
    BomberTrainingEnv* clone = bomber_training_clone(env);
    assert(clone != NULL);
    assert(bomber_training_state_hash(clone) == initial_hash);
    assert(bomber_training_step_joint(clone, 5, 5) == 0);
    assert(bomber_training_state_hash(clone) != initial_hash);
    assert(bomber_training_state_hash(env) == initial_hash); /* clone isolation */
    bomber_training_reset(clone, 1337);
    assert(bomber_training_state_hash(clone) == initial_hash);
    assert(bomber_training_tactical_value(env, 0) > -10000.0f);

    BomberTrainingAgent* heuristic = bomber_training_agent_create(2, 42);
    BomberTrainingAgent* mcts = bomber_training_agent_create(7, 42);
    assert(heuristic != NULL && mcts != NULL);
    assert(!bomber_training_agent_configure_mcts(heuristic, 8, 4));
    assert(bomber_training_agent_configure_mcts(mcts, 8, 4));
    assert(!bomber_training_agent_configure_mcts(mcts, 0, 4));
    int heuristic_action = bomber_training_agent_action(heuristic, env, 1);
    int mcts_action = bomber_training_agent_action(mcts, env, 1);
    assert(heuristic_action >= 0 && heuristic_action < BOMBER_TRAINING_ACTIONS);
    assert(mcts_action >= 0 && mcts_action < BOMBER_TRAINING_ACTIONS);
    int modeled_action = bomber_training_baseline_action(2, env, 1, 42);
    assert(modeled_action >= 0 && modeled_action < BOMBER_TRAINING_ACTIONS);
    bomber_training_agent_destroy(heuristic);
    bomber_training_agent_destroy(mcts);

    free(encoded);
    bomber_training_destroy(clone);
    bomber_training_destroy(env);
    return 0;
}
