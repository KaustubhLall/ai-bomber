# Future Model Integration

## Overview

The simulator is designed to support future ML/AI experiments without requiring any ML framework for the core environment. All algorithms interact through the same observation/action/reward API.

## C ABI for Python integration

The opaque `bomber_training` shared library exposes the current training ABI
without requiring Python to mirror internal C structs:

```python
import ctypes

lib = ctypes.CDLL("./build/src/libbomber_training.so")
# bomber_training_create/clone/encode/legal_actions/step_joint/outcome
```

Build the bridge directly:
```bash
cmake --build build --target bomber_training
```

## Planned experiment types

### 1. Behavior cloning
- Run heuristic agent, record (observation, action) pairs.
- Train a neural network to imitate the heuristic policy.
- Deploy as an `ExternalPolicyAgent`.

### 2. DQN (Deep Q-Network)
- Use flat observation as state input.
- Action space: 6 discrete actions.
- Reward: `StepResult.reward`.
- Deploy trained model via `ExternalPolicyAgent`.

### 3. PPO (Proximal Policy Optimization)
- Same observation/action/reward interface.
- Self-play mode: run multiple agents in battle mode.
- Use `MODE_BATTLE` for competitive training.

### 4. Genetic algorithms over neural policies
- Encode small neural networks as weight vectors.
- Evaluate fitness via `evaluator_run()` over N episodes.
- No gradient computation needed; pure rollouts.

### 5. Evolution strategies
- Population-based optimization over policy parameters.
- Use `runner_run()` for parallel evaluation.
- Deterministic seeds ensure reproducible fitness.

### 6. NEAT (Neuroevolution of Augmenting Topologies)
- Optional future experiment.
- Observation vector is fixed-size, suitable for NEAT inputs.
- Action output: 6 discrete actions via argmax.

### 7. MCTS (Monte Carlo Tree Search)
- Use `env_step()` for rollouts.
- Clone state via `BomberState` copy (POD struct).
- Deterministic simulation ensures consistent rollouts.

### 8. Self-play PPO
- `MODE_BATTLE` with 2+ agents.
- Agents share or compete with the same policy.
- Observation includes enemy positions.

### 9. Imitation learning from replay logs
- Load replays via `replay_load()`.
- Extract (observation, action) pairs from recorded frames.
- Train offline policy.

### 10. Hybrid safety-shielded learned policy
- Use danger map `action_safe[]` to filter unsafe actions.
- Learned policy proposes actions; safety shield overrides unsafe ones.
- Prevents suicidal behavior during exploration.

## ExternalPolicyAgent

The `ExternalPolicyAgent` (currently a placeholder) is the integration point for trained models:

```c
// Future implementation:
typedef struct {
    float weights[NUM_WEIGHTS];
    // Or: function pointer to Python callback
    // Or: socket/file-based action server
} ExternalPolicyAgent;

Action external_agent_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug) {
    float flat[OBS_FLAT_SIZE];
    int size;
    obs_to_flat(obs, flat, &size);
    // Forward pass through model
    // Return argmax or sampled action
}
```

## Key design choices for ML compatibility

1. **Fixed-size observation**: No variable-length inputs; neural networks get consistent shapes.
2. **Deterministic**: Reproducible training runs; same seed = same experience.
3. **Fast headless**: Hundreds of thousands of steps/sec for efficient data collection.
4. **No rendering dependency**: Training runs without raylib; viz is optional.
5. **Reward breakdown**: Per-component tracking helps with reward shaping analysis.
