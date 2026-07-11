# Native C++/LibTorch AlphaZero

## Purpose

`bomber_alphazero_native` is the high-throughput successor to the NumPy
reference trainer. The complete hot path is native:

```text
C BomberEnv -> C++ concurrent PUCT -> batched LibTorch CUDA inference
            -> C++ replay -> LibTorch optimization -> atomic checkpoint
```

Python is not involved in self-play, search, inference, optimization, replay,
checkpointing, or recovery. It remains useful for optional offline plots and
analysis. The authoritative simulator and observation encoder remain C so the
native and reference trainers cannot silently learn from different games.

The selected default model is a 128-channel, 10-block residual policy/value
tower with 3,250,839 parameters. On the local RTX 5080, BF16 inference measured
about 64,000 positions/second at batch 512. Model width and depth are CLI
parameters, so this is a reproducible starting point rather than a permanent
architecture claim.

## Install the local LibTorch runtime

The C++ headers and import libraries are taken directly from the official
PyTorch wheel. A separate LibTorch archive or full CUDA SDK is not required.

```powershell
python -m venv .venv-gpu
.\.venv-gpu\Scripts\python.exe -m pip install --upgrade pip
.\.venv-gpu\Scripts\python.exe -m pip install torch --index-url https://download.pytorch.org/whl/cu130
```

Verify that CUDA is visible before building:

```powershell
.\.venv-gpu\Scripts\python.exe -c "import torch; print(torch.__version__, torch.cuda.is_available(), torch.cuda.get_device_name(0))"
```

## Build

```powershell
cmake -S . -B build-native-gpu -G "Visual Studio 18 2026" -A x64 `
  -DAI_BOMBER_BUILD_VIZ=OFF `
  -DAI_BOMBER_BUILD_TESTS=ON `
  -DAI_BOMBER_BUILD_NATIVE_ALPHAZERO=ON
cmake --build build-native-gpu --config Release -j 12
ctest --test-dir build-native-gpu -C Release --output-on-failure
```

Set `AI_BOMBER_TORCH_ROOT` if the torch package is not in `.venv-gpu`:

```powershell
cmake -S . -B build-native-gpu `
  -DAI_BOMBER_BUILD_NATIVE_ALPHAZERO=ON `
  -DAI_BOMBER_TORCH_ROOT=C:\path\to\torch
```

The target is opt-in. Normal simulator builds do not require PyTorch or CUDA.

## Benchmark model shapes

The launcher places the wheel's runtime DLL directory on `PATH`:

```powershell
.\tools\run_native_alphazero.ps1 benchmark `
  --channels 128 --blocks 10 --batch-size 512 --warmup 10 --repeats 50
```

Benchmark the exact GPU while other normal desktop workloads are present. A
larger network that halves throughput is not automatically better; compare
validation progress per wall-clock hour, not parameters alone.

## End-to-end smoke

```powershell
.\tools\run_native_alphazero.ps1 train `
  --run-dir results\alphazero-native-smoke --fresh `
  --iterations 1 --games 4 --simulations 4 `
  --train-steps 2 --batch-size 8 --replay-capacity 512 `
  --channels 32 --blocks 2 `
  --teacher-games 2 --teacher-iterations 1 `
  --eval-interval 1 --eval-games 2 --eval-simulations 4 `
  --snapshot-interval 1 --max-steps 20
```

This exercises the real C simulator, joint-action PUCT, BF16 CUDA inference,
FP32 backward, replay sampling, both-seat evaluation, atomic checkpointing, and
best/snapshot promotion. It is a mechanics test, not evidence of strategy.

Promotion evaluation is always clean network search: the early tactical-value
bootstrap is enabled only for self-play. A first champion must pass random and
heuristic floors. Later challengers must also avoid material heuristic
regression and defeat `best.pt` over both seats with a one-sided Wilson lower
bound above `0.5 + promotion_margin`. Metrics record W-D-L, the lower bound,
gate reason, champion iteration, seed blocks, and gate configuration.

## Completed local long run

```powershell
.\tools\run_native_alphazero.ps1 train `
  --run-dir results\alphazero-native-grokking-v1 --fresh `
  --iterations 100 `
  --games 128 --simulations 64 `
  --channels 128 --blocks 10 `
  --train-steps 128 --batch-size 512 `
  --replay-capacity 200000 `
  --teacher-games 32 --teacher-iterations 20 `
  --eval-interval 10 --eval-games 16 --eval-simulations 64 `
  --mcts-eval-interval 20 --mcts-eval-games 4 `
  --snapshot-interval 10 --seed 1
```

Self-play games are advanced concurrently. Each PUCT simulation gathers one
leaf from every active root, evaluates both seat perspectives in one GPU batch,
and then performs zero-sum backup. Independent tree traversals are parallelized
with OpenMP when available. BF16 is used for the dominant inference workload;
optimization stays FP32 for stable, exact recovery without gradient-scaler
state.

## Resume, stop, and recover

Resume by running the same command without `--fresh`. `--iterations` is the
total target, not an additional count:

```powershell
.\tools\run_native_alphazero.ps1 train `
  --run-dir results\alphazero-native-grokking-v1 --iterations 200
```

For custom model/map/replay settings, repeat the shape-defining options on
resume. A mismatched signature is rejected before training.

Checkpoint contents:

- network parameters and BatchNorm running state;
- AdamW optimizer moments and parameter groups;
- replay states, policy targets, values, capacity position, and order;
- completed iteration and global optimizer-update counters;
- complete C++ RNG engine state;
- best selection score and configuration signature.

`latest.pt` is written to `latest.pt.tmp` and atomically replaced only after the
archive closes. `iteration_XXXXXX.pt` is an immutable periodic snapshot and
`best.pt` tracks the current promoted checkpoint. Ctrl+C sets a stop flag; the
trainer finishes the current safe operation and writes both `emergency.pt` and
`latest.pt`. A power loss during replacement leaves the previous `latest.pt`
valid; an orphan `.tmp` file is never loaded.

To test recovery, interrupt a non-smoke run once, verify that
`emergency.pt` loads, then resume without `--fresh`. The startup line must report
the saved iteration and replay count.

When extending the total iteration target, do not silently restart the old
cosine schedule. Use `--lr-schedule-start-update` and
`--lr-schedule-updates` to declare an intentional continuation. Every startup
appends its iteration, update counter, checkpoint, and runtime hyperparameters
to `config-history.jsonl`; checkpoints also carry their runtime config.

## Progress and metrics

The terminal shows separate progress bars and ETA for teacher data, self-play,
optimization, random evaluation, heuristic evaluation, and total iterations.
Use `--no-progress` only for log collectors that do not handle carriage returns.

`metrics.jsonl` is append-only with one valid JSON object per completed
iteration. It records new/replay sample counts, optimizer updates, policy/value
loss, entropy, learning rate, elapsed time, W-D-L/score/mean length for each
evaluation opponent, Wilson lower confidence bounds, and promotion state. The checkpoint is the recovery
authority; metrics are the audit trail.

Schema-2 evaluation objects also serialize W-D-L separately for learner seat 0
and seat 1. Aggregate role balance is therefore auditable rather than inferred
only from the evaluator loop.

For an atomic JSON summary of a live or stopped campaign:

```powershell
python tools\analyze_native_run.py results\alphazero-native-superhuman-v2 `
  --target-iteration 500 --window 20
```

The summary reports rolling iteration time/ETA, loss and entropy slopes,
self-play decisiveness/game-length slopes, champion state, and all completed
evaluation gates.

Calibrate native baselines against each other rather than assuming search
budget is a total strength ordering:

```powershell
python tools\evaluate_native_ladder.py `
  --library build-native-gpu-next\src\Release\bomber_training.dll `
  --agent-a heuristic --agent-b mcts `
  --agent-b-mcts-simulations 512 --agent-b-mcts-depth 16 `
  --seed-base 1320001 --seeds 8 --output results\ladder.json
```

The bridge exposes validated MCTS simulation/depth configuration, and the
ladder output includes both seat orientations, W-D-L, score, lower bound, and
mean game length.

## Evaluation

```powershell
.\tools\run_native_alphazero.ps1 evaluate `
  --run-dir results\alphazero-native-grokking-v1 `
  --checkpoint best.pt `
  --eval-seed-base 1500001 --eval-games 64 --eval-simulations 96 `
  --eval-mcts --mcts-eval-games 8 `
  --output results\alphazero-native-grokking-v1\holdout-best.json
```

Training uses disjoint defaults: `900001` for frequent random/heuristic
validation, `1100001` for challenger-versus-incumbent promotion, and `1300001`
for sparse native-MCTS diagnostics. Overlapping configured blocks are rejected.
The example deliberately
uses `1500001` as a separate final holdout; choose and record that range before
training. Evaluation uses fixed seeds and both seat orientations. Search models the
baseline opponent during PUCT and then plays the real persistent native agent.
The native MCTS opponent is configurable with
`--baseline-mcts-simulations` and `--baseline-mcts-depth`; the strengthened
default is 512 simulations at depth 16. Specify 96/depth 12 to reproduce the v1
holdout exactly.
Keep final holdout seed ranges outside training and checkpoint selection. See
[the reusable protocol](SELF_PLAY_GROKKING_PLAYBOOK.md) before making strength
or grokking claims. The completed run's full validation trajectory, checkpoint
hash, holdout, comparison with the reference trainer, and negative grokking
conclusion are recorded in [AlphaZero full-simulator results](ALPHAZERO_RESULTS.md).

The continuing statistically gated campaign and its predeclared strength
criterion are specified in [Superhuman AlphaZero ladder](SUPERHUMAN_ALPHAZERO.md).

**RETRACTED 2026-07-11:** this section previously claimed the v5 frozen
iteration-210 checkpoint cleared that ladder as an agent-ladder superhuman
result. An overnight audit found 95-97% of its wins (including the reported
A3/B3 holdout wins) were arena-crush deaths, not bomb-kills — the W-D-L/Wilson
gate above has no win-cause classification, so it could not see the
difference. Full diagnosis, root cause, and the new evidence bar any future
claim must clear: [Superhuman AlphaZero ladder](SUPERHUMAN_ALPHAZERO.md)
(retraction banner) and Linear KL-100 (the honest-claim ledger). Do not cite
the v5/iteration-210 result as evidence of combat skill; treat it only as a
historical record of how a statistically careful-looking protocol still
missed a structural exploit.
