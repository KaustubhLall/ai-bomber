#ifndef BOMBER_TRAINING_ENCODING_H
#define BOMBER_TRAINING_ENCODING_H

#include "env/env.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOMBER_TRAINING_CHANNELS 17
#define BOMBER_TRAINING_VIEW_SIZE 11
#define BOMBER_TRAINING_ACTIONS 6
#define BOMBER_TRAINING_OBSERVATION_SIZE \
    (BOMBER_TRAINING_CHANNELS * BOMBER_TRAINING_VIEW_SIZE * BOMBER_TRAINING_VIEW_SIZE)

/* Shared encoder used by both the stable C ABI and the native LibTorch trainer.
   Keeping this in C guarantees that every training frontend sees identical
   features and action masks. */
int bomber_training_encode_env(const BomberEnv* env, int perspective,
                               float* output, int output_count);
int bomber_training_safe_actions_env(const BomberEnv* env, int perspective,
                                     int* output_mask, int output_count);

#ifdef __cplusplus
}
#endif

#endif
