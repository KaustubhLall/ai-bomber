#include "training/encoding.h"

#include <string.h>

static int cell_index(int channel, int x, int y) {
    return (channel * BOMBER_TRAINING_VIEW_SIZE + y) * BOMBER_TRAINING_VIEW_SIZE + x;
}

int bomber_training_encode_env(const BomberEnv* env, int perspective,
                               float* output, int output_count) {
    if (!env || !output || perspective < 0 || perspective >= 2) return 0;
    const BomberState* state = &env->state;
    if (output_count < BOMBER_TRAINING_OBSERVATION_SIZE || state->agent_count != 2) return 0;
    memset(output, 0, BOMBER_TRAINING_OBSERVATION_SIZE * sizeof(*output));

    Observation observation;
    env_observe(env, perspective, &observation);
    int opponent = perspective == 0 ? 1 : 0;
    const BomberAgentState* self = &state->agents[perspective];
    const BomberAgentState* other = &state->agents[opponent];
    float self_ammo = (float)self->bomb_ammo / 4.0f;
    float self_range = (float)self->blast_range / 8.0f;
    float other_ammo = (float)other->bomb_ammo / 4.0f;
    float other_range = (float)other->blast_range / 8.0f;
    float progress = (float)state->step /
                     (float)(env->config.max_steps > 0 ? env->config.max_steps : 1);
    for (int y = 0; y < BOMBER_TRAINING_VIEW_SIZE; y++) {
        for (int x = 0; x < BOMBER_TRAINING_VIEW_SIZE; x++) {
            int tile = observation.local_tiles[y][x];
            if (tile >= 0 && tile <= 5) output[cell_index(tile, x, y)] = 1.0f;
            output[cell_index(6, x, y)] = observation.local_bombs[y][x] ? 1.0f : 0.0f;
            int bomb_timer = observation.local_bomb_timers[y][x];
            output[cell_index(7, x, y)] = bomb_timer < 0 ? 0.0f :
                (float)bomb_timer / (float)(env->config.bomb_timer > 0 ? env->config.bomb_timer : 1);
            int wx = self->x + x - LOCAL_OBS_HALF;
            int wy = self->y + y - LOCAL_OBS_HALF;
            if (wx >= 0 && wy >= 0 && wx < state->width && wy < state->height)
                output[cell_index(8, x, y)] = env->danger.current_blast[wy][wx] ? 1.0f : 0.0f;
            int danger = observation.local_danger[y][x];
            output[cell_index(9, x, y)] = danger < 0 ? 0.0f : 1.0f / (float)(danger + 1);
            output[cell_index(12, x, y)] = self_ammo;
            output[cell_index(13, x, y)] = self_range;
            output[cell_index(14, x, y)] = other_ammo;
            output[cell_index(15, x, y)] = other_range;
            output[cell_index(16, x, y)] = progress;
        }
    }
    if (self->alive) output[cell_index(10, LOCAL_OBS_HALF, LOCAL_OBS_HALF)] = 1.0f;
    int opponent_x = other->x - self->x + LOCAL_OBS_HALF;
    int opponent_y = other->y - self->y + LOCAL_OBS_HALF;
    if (other->alive && opponent_x >= 0 && opponent_y >= 0 &&
        opponent_x < BOMBER_TRAINING_VIEW_SIZE && opponent_y < BOMBER_TRAINING_VIEW_SIZE)
        output[cell_index(11, opponent_x, opponent_y)] = 1.0f;
    return BOMBER_TRAINING_OBSERVATION_SIZE;
}

int bomber_training_safe_actions_env(const BomberEnv* env, int perspective,
                                     int* output_mask, int output_count) {
    if (!env || !output_mask || output_count < BOMBER_TRAINING_ACTIONS ||
        perspective < 0 || perspective >= 2) return 0;
    Observation observation;
    env_observe(env, perspective, &observation);
    int count = 0;
    for (int action = 0; action < ACTION_COUNT; action++) {
        output_mask[action] = observation.valid_actions[action] && observation.safe_actions[action];
        count += output_mask[action] ? 1 : 0;
    }
    return count;
}
