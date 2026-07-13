# Full-simulator AlphaZero training

## What this is

This document describes the dependency-light NumPy reference implementation.
For the fully native, batched CUDA trainer used for long runs, see
[Native C++/LibTorch AlphaZero](NATIVE_ALPHAZERO.md). Both paths share the same
C simulator and encoder.

`tools/train_alphazero.py` is an AlphaZero-style self-play trainer connected to
the authoritative C Bomberman simulator. Unlike the earlier `alphazero-lite`
reference in `tools/learning.py`, its games use real maps, bombs, danger,
powerups, simultaneous actions, and terminal rules.

The policy/value input is a centered 17-channel 11x11 tensor. On the standard
13x11 arena it covers nearly the whole board while making local bomb, danger,
escape, and trapping patterns translation-consistent for the compact network.

The trainer searches all 36 simultaneous two-player joint actions with
decoupled, zero-sum PUCT: player zero maximizes its marginal value while player
one maximizes the negated value. Root visits are marginalized into the project's
six-action policy target for each player. A shared policy/value network learns
from both seat perspectives.

This reference implementation is intentionally dependency-light: NumPy supplies a shared
3x3 convolutional encoder, compact policy/value heads, and Adam. This makes the
whole recovery path runnable without a large framework install. It is not yet a
GPU-optimized batched inference engine; the native trainer is.

## Build the simulator bridge

```powershell
cmake -S . -B build-codex-vs -DAI_BOMBER_BUILD_VIZ=OFF
cmake --build build-codex-vs --config Release --target bomber_training
```

If the trainer cannot discover the resulting library, pass it explicitly:

```powershell
python tools/train_alphazero.py --library build-codex-vs/src/Release/bomber_training.dll
```

## Start a training run

```powershell
python tools/train_alphazero.py `
  --library build-codex-vs/src/Release/bomber_training.dll `
  --run-dir results/alphazero-main `
  --iterations 100 --self-play-games 16 --simulations 64
```

The terminal displays progress, elapsed time, throughput, and ETA for self-play,
optimization, and evaluation. `metrics.jsonl` receives one fsynced record per
completed iteration.

## Resume and recover

Run the same command again. If `latest.npz` exists, resume is automatic.
`--iterations` is the desired total, not an additional count:

```powershell
# Continue a 100-iteration run through iteration 250.
python tools/train_alphazero.py --run-dir results/alphazero-main --iterations 250
```

Each checkpoint includes network weights, Adam and learning-rate scheduler state, iteration/global counters,
NumPy RNG state, replay samples and order, configuration, evaluation state, and
best score. Writes use a temporary file plus atomic replacement. Immutable
`iteration_XXXXXX.npz` snapshots are kept at `--snapshot-interval`; `best.npz`
tracks the promoted model. Ctrl+C requests a safe stop and writes
`emergency.npz`.

Use `--fresh` only when intentionally discarding the checkpoints and metrics in
that run directory.

## Quick verification

```powershell
python tools/train_alphazero.py `
  --library build-codex-vs/src/Release/bomber_training.dll `
  --run-dir results/alphazero-smoke --fresh `
  --iterations 1 --self-play-games 1 --simulations 2 `
  --train-steps 1 --batch-size 4 --hidden-size 16 `
  --width 7 --height 7 --max-steps 8 --evaluation-games 1
```

Then rerun with `--iterations 2` and without `--fresh`; the log must say it
resumed at iteration 1, and `metrics.jsonl` must contain iterations 1 and 2.

## Evaluation and claim boundary

Evaluation uses fixed seeds and both seat orientations. It reports wins, losses,
draws, and mean episode length against random, the native heuristic, the
previous best checkpoint, and periodically the native MCTS agent.
The first evaluated checkpoint seeds the league. Later checkpoints are promoted
only when their two-seat head-to-head score against the incumbent exceeds
`0.5 + --promotion-margin`, preserves at least a 90% random score, and does not
materially regress on the native heuristic score.

A short smoke run validates mechanics and recovery only. It does not demonstrate
strategy improvement. Strength claims require a long run plus untouched holdout
evaluation under the protocol in `docs/EXPERIMENTS.md`.

The default early curriculum also records a small number of native-MCTS teacher
games against a random opponent, alternating the expert's seat, for the first 20
iterations. Only expert decisions are retained. This supplies complete winning
trajectories before pure self-play dominates, without teaching the random side's
mistakes. Set `--teacher-games 0` for a strict from-scratch ablation; teacher
samples and throughput are reported separately.

See [the verified checkpoint report](ALPHAZERO_RESULTS.md) for role-balanced
holdout results and the honest native-MCTS boundary.
