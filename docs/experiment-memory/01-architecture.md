# 01 — Architecture

_Last updated: 2026-07-08 (session start). Grounded in a direct read of the sources._

## Layered picture

```
        deterministic ground truth (C17)
  ┌───────────────────────────────────────────────────────────┐
  │ src/env/*        BomberEnv: state, rules, bombs, blasts,    │
  │                  danger, powerups, reward, observation      │
  │ src/core/*       rng, config, replay, metrics, ring buffer  │
  │ src/agents/*     random, scripted, heuristic, greedy,       │
  │                  evasive, alpha-beta, MCTS (search_agent)   │
  │ src/sim/*        runner, benchmark, evaluator               │
  └───────────────────────────────────────────────────────────┘
                              │  (C ABI)
        shared training bridge + encoder (C)
  ┌───────────────────────────────────────────────────────────┐
  │ src/training/training_api.{h,c}   stable C ABI (v6)         │
  │ src/training/encoding.{h,c}       17ch × 11×11 encoder      │
  └───────────────────────────────────────────────────────────┘
            │ ctypes                         │ direct C++ link
   ┌────────────────────┐        ┌──────────────────────────────────┐
   │ NumPy reference     │        │ Native C++/LibTorch trainer       │
   │ tools/train_alphazero.py     │ src/training/native/*.cpp         │
   │ tools/alphazero/*.py│        │ (bomber_alphazero_native.exe)     │
   └────────────────────┘        └──────────────────────────────────┘
```

The encoder lives **next to the game** (in C), not inside a framework frontend, so both
trainers receive byte-identical features and masks. This is the parity guarantee.

## Shared C training bridge — `src/training/training_api.{h,c}`

Stable C ABI, `BOMBER_TRAINING_ABI_VERSION = 6`. Opaque handles `BomberTrainingEnv`,
`BomberTrainingAgent`. Key entry points:

- `create(width,height,max_steps,crate_density,seed)` → battle-config env, reset.
- `clone(env)` → `env_copy` into a fresh handle, `opponent=NULL` (isolated, no shared
  policy state — required for search fidelity).
- `encode(env,perspective,out,n)` → fills the 17×11×11 float tensor (see below).
- `legal_actions` / `safe_actions(env,perspective,mask,n)` → per-action 0/1 masks.
  Safe = `valid_actions[a] && safe_actions[a]` from the observation.
- `step_joint(env,a0,a1)` → applies simultaneous 2-player actions; returns 1 terminal /
  0 continue / -1 invalid.
- `outcome(env,perspective)` → **+1 win / -1 loss / 0 draw-or-timeout-or-nonterminal**
  (timeout at `step >= max_steps` is a draw here).
- `state_hash(env)` → `env_state_hash`, stable across clones (used for dedup + seeding).
- `tactical_value(env,perspective)` → `evaluator_score_state` on a copy, with a
  max_steps bump so a timed-out position still yields a positional score (draw tiebreak).
- `agent_create` / `baseline_action(type,env,perspective,seed)` → persistent or
  one-shot native baseline policies. Type ids: random=0, scripted=1, heuristic=2,
  greedy=3, alpha-beta=6, MCTS=7, evasive=8 (`AGENT_EXTERNAL` rejected).

## Observation encoding — `src/training/encoding.c`

Centered **17-channel 11×11** tensor (`LOCAL_OBS_HALF` = 5), perspective = acting agent
at the center. On the 13×11 arena this covers nearly the whole board while keeping local
bomb/danger/escape/trap motifs translation-consistent. Channels:

| Ch | Meaning |
| --- | --- |
| 0–5 | Tile one-hot over 6 tile categories (`local_tiles`, value 0..5). |
| 6 | Bomb present (`local_bombs`). |
| 7 | Bomb timer normalized by `config.bomb_timer`. |
| 8 | Current blast at that world cell (`danger.current_blast`, world-coord lookup). |
| 9 | Local danger, `1/(danger+1)` (0 if none). |
| 10 | Self marker at center (only if self alive). |
| 11 | Opponent at its relative offset (only if alive and within the 11×11 window). |
| 12 / 13 | Self bomb ammo /4, self blast range /8. |
| 14 / 15 | Opponent ammo /4, opponent range /8. |
| 16 | Game progress = `step / max_steps` (scalar broadcast to the plane). |

## Model — `src/training/native/model.cpp`

Residual policy/value tower (default 128 channels, 10 blocks, **3,250,839 params**):

- **Stem**: conv3×3 (17→C, no bias) → BatchNorm → ReLU.
- **N residual blocks**: conv3×3→BN→ReLU→conv3×3→BN, add skip, ReLU.
- **Policy head**: conv1×1 (C→32) → BN → ReLU → flatten → Linear(32·11·11 → 6 logits).
  (Logits, not probabilities; softmax applied at use sites.)
- **Value head**: conv1×1 (C→8) → BN → ReLU → flatten → Linear(968→256) → ReLU →
  Linear(256→1) → **tanh** → squeeze. Output in (−1, 1).

Model width/depth are CLI params (`--channels`, `--blocks`), so this shape is a
reproducible starting point, not a permanent claim.

## Native trainer — `src/training/native/trainer.cpp` (the core, ~1264 lines)

### Search: `BatchedMcts` (decoupled zero-sum PUCT, simultaneous moves)

- A `Node` holds a **cloned env** and flat `kJointActions = 36` arrays (`priors`,
  `visits`, `value_sum`, `children`) over the 6×6 joint action space.
- **Leaf batching**: each simulation gathers one unexpanded leaf from *every* active
  root, encodes **both seats** for each leaf, and runs a single batched forward
  (`jobs·2` rows). GPU utilization comes from the number of concurrent roots.
- Independent root traversals are parallelized with **OpenMP** when built with it. This
  is safe: each `root_index` touches only its own tree; the RNG is used only in the
  sequential `add_root_noise` / `sample_joint_action`; leaves are compacted in
  `root_index` order, so leaf batching is deterministic regardless of thread schedule.
- **`select_joint`**: marginalizes visits/values/priors over the opponent axis for each
  seat, then picks each seat's best PUCT action independently and combines into a joint
  index. Player 0 uses `Q = value_sum/visits`; player 1 uses `Q = −value_sum/visits`
  (zero-sum). PUCT bonus `c_puct · prior · sqrt(ΣN+1)/(1+N)`.
- **`expand_and_backup`**: priors = product of per-seat safe-masked softmax policies
  (falls back to legal, then uniform); a fixed-opponent-seat constraint replaces that
  seat's policy with a deterministic baseline action (used in evaluation to model the
  real opponent). Leaf value = `0.5·(v[seat0] − v[seat1])` (player-0 perspective),
  optionally blended early in training with a tanh'd tactical heuristic
  (`bootstrap_value_weight` decaying over `bootstrap_value_iterations`). This blend
  affects **search backups only**, not stored value targets.
- Terminal leaves back up `outcome(env, 0)` (player-0 perspective) each visit; they are
  never sent to the network.
- **Root Dirichlet noise** (`dirichlet_alpha`, `dirichlet_fraction`) applied to root
  joint priors, at the root only, for self-play (`root_noise=true`); off in eval.

### Self-play: `collect_self_play`

`self_play_games` concurrent games. Each step: batched PUCT with root noise → per-seat
policy target = **marginalized root joint visits**; encode both seats as samples; pick
the played joint action by `sample_joint_action` (temperature `config.temperature` for
the first `temperature_steps` plies, then argmax). On terminal, value target =
`terminal_training_value(env, 0)` for even (seat-0) samples and its negation for odd
(seat-1) samples. `terminal_training_value` = terminal ±1, else `tanh(Δtactical/250)`
(bounded, symmetric draw shaping).

### Teacher curriculum: `collect_teacher`

For the first `teacher_iterations` iterations, records `teacher_games` expert games:
expert = **`AGENT_HEURISTIC`** (alternating seat) vs `AGENT_RANDOM`. Only the expert's
one-hot action is kept as the policy target; the whole trajectory's value = the expert's
terminal value. Supplies complete competent trajectories before pure self-play dominates.
(NOTE: the NumPy reference uses an **MCTS** teacher by default — see parity notes below.)

### Optimization: `optimize`

`train_steps` minibatches of `batch_size` sampled uniformly from replay. Loss =
policy cross-entropy `−Σ target·log_softmax(logits)` + value `mse_loss`. Grad-norm
clipped to 5.0, **AdamW** (`weight_decay`), **cosine LR** from `learning_rate` down to
`min_learning_rate` over `iterations·train_steps` total updates. Forward runs in FP32
here (BF16 is only for the larger inference workload) → exact resume without scaler state.
Entropy is reported for monitoring. The phase is **atomic per iteration** — once
optimization starts it finishes (no intra-iteration cursor), so resume never re-consumes
self-play samples.

### Evaluation: `evaluate_baseline`

Plays `games·2` matches — **both seat orientations** — vs a baseline `AgentType`.
Learner move = argmax of marginalized root visits; opponent move = the real persistent
agent. During the learner's PUCT the opponent seat is **modeled** by the same baseline
(`fixed_opponent_seat`), except vs MCTS where the model is disabled (`modeled_seat=-1`,
i.e. self-play-like search) to avoid recursively invoking MCTS at every leaf — the real
MCTS agent still plays the root game. Score = `(wins + 0.5·draws)/played`. Seeds =
`evaluation_seed_base + seed_index`.

### Promotion gate (in `run`)

Every `evaluation_interval` iterations: eval vs random and heuristic. Promote (`best.pt`)
iff `random.score ≥ 0.90` **and** `heuristic.score > best_score`. Every
`mcts_evaluation_interval` iterations: an additional MCTS diagnostic (not a gate).

### Checkpoint / recovery

- `save_checkpoint(dest)` writes a LibTorch archive to `dest.tmp` then
  `atomic_replace` (Windows `MoveFileExW` with `REPLACE_EXISTING|WRITE_THROUGH`; POSIX
  `rename`). Contents: model params+BN buffers, AdamW state, replay
  (states fp16 / policies / values / ring `next`), meta (format, iteration,
  global_updates, replay next), best_score, **std::mt19937_64 RNG state**, and a
  **config signature**.
- `load_checkpoint` **rejects a mismatched config signature before training**
  (`format|w|h|max_steps|crate_density|channels|blocks|replay_capacity`) and restores
  all of the above. `--iterations` is a **total** target, not additive.
- `latest.pt` every iteration; immutable `iteration_XXXXXX.pt` every `snapshot_interval`
  (copied from latest); `best.pt` on promotion; `emergency.pt` + `latest.pt` on Ctrl+C
  (SIGINT sets an atomic stop flag; the trainer stops at a state boundary).
- `--fresh` deletes `*.pt`, `metrics.jsonl`, `*.tmp` in the run dir before starting.
- `config.json` is (re)written atomically at startup with the current config + git sha +
  torch version + param count.

### Metrics: `append_metrics`

Append-only `metrics.jsonl`, one flushed JSON object per completed iteration:
iteration, global_updates, replay_size, new_samples, elapsed_seconds, self_play W/D/L +
mean_steps, optimization {loss, policy_loss, value_loss, entropy, learning_rate},
optional random/heuristic/mcts {W,D,L,score,mean_steps}, promoted, best_score. **The
checkpoint is the recovery authority; metrics are the audit trail.**

### Entry point — `src/training/native/main.cpp`

Loads `torch_cuda.dll` explicitly (MSVC can drop the CUDA import lib), requires CUDA,
enables TF32, dispatches `benchmark` / `train` / `evaluate`. CUDA is mandatory for the
native trainer.

## NumPy reference vs native — parity notes

Same C sim + encoder, but the pipelines differ in more than scale:

- **Teacher**: reference default `--teacher-type mcts`; native hardcodes heuristic.
- **Adversary curriculum**: reference has one (`--adversary-*`, seed pool, win-repeats);
  native has none.
- **Optimizer/LR**: reference Adam with multiplicative decay (`0.995`); native AdamW with
  cosine decay to `min_learning_rate`.
- **Promotion**: reference uses `--promotion-margin` head-to-head vs incumbent; native
  promotes on the heuristic-score gate with a random floor.
- **Scale defaults**: reference small (16 self-play, hidden 96, buffer 20k); native large
  (128 self-play, 128ch/10-block, replay 200k, BF16 CUDA).

So "the two cannot silently learn from different games" holds for the **encoder/rules**,
but the **training curricula are not identical** — keep that in mind when comparing.
