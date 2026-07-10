# 00 — Experiment Overview

_Last updated: 2026-07-08 03:2x (session start)._

## What this project is

**AI Bomber** is a deterministic Bomberman-style simulator written in C17, with a
headless benchmark harness, a raylib visualizer, and a set of AI agents that share one
observation/action interface. The research thread ("AI Bomber Search & Self-Play",
Linear project in team **Kaus**/`KL`) grows it from rule-based baselines up through
classic search (alpha-beta, MCTS) to **AlphaZero-style self-play** and a PPO baseline,
with an evaluation harness designed to prevent fake progress.

## The AlphaZero experiment

Two trainers share **one authoritative C simulator + encoder** (so they cannot silently
learn from different games):

1. **NumPy reference** (`tools/train_alphazero.py`, `tools/alphazero/*.py`) —
   dependency-light, framework-free. Drives the real C sim via ctypes. Good for the
   recovery/parity story; not GPU-optimized. Has a teacher curriculum (default
   **MCTS** teacher) and an **adversary** curriculum the native path lacks.
2. **Native C++/LibTorch** (`src/training/native/*.cpp`) — the high-throughput
   successor. The whole hot path is native: C `BomberEnv` → C++ concurrent decoupled
   zero-sum PUCT → batched LibTorch **BF16 CUDA** inference → C++ replay → **FP32**
   optimization → atomic checkpoint. Python is optional offline analysis only.
   Uses a **heuristic** teacher (hardcoded), no adversary phase.

The **overnight run being monitored is the native trainer** (KL-94). See
[03-running-experiment.md](03-running-experiment.md).

## The goal (Linear KL-94 / "AI-BOMBER-16")

Scale the completed full-simulator AlphaZero prototype (KL-93) into a fully native,
high-throughput system and run a **long-horizon generalization experiment** that could
discover durable Bomberman strategy. Key intents:

- Remove the Python control-plane bottleneck; keep the deterministic C env authoritative.
- Residual policy/value tower, BF16 inference / FP32 optimization, large concurrent
  self-play, long iteration targets, benchmark model shapes on the local RTX 5080.
- **Unattended recoverability**: atomic latest checkpoint, immutable snapshots, best,
  emergency Ctrl+C checkpoint; auto-resume; config-signature rejection.
- **Test generalization, not just fit**: separate self-play / selection / holdout seeds;
  promote only on random+heuristic gates; treat "grokking" as a *hypothesis to test*,
  not a promised outcome.

## Bottom line (honest status)

- **Prior verified result (NumPy run `alphazero-conv-main`, iter 45):** 31-0-1 vs random
  (96.9%), 9-19-4 vs native heuristic (57.8%, >2:1 W/L), 0-6-2 vs native MCTS (37.5%).
  Native MCTS is the retained upper boundary — the learned agent usually survives it but
  did not win the final 8-game block. See `docs/ALPHAZERO_RESULTS.md`.
- **Current native run (COMPLETED 2026-07-08 ~05:33):** finished all 100/100 iterations
  cleanly (no interrupt, no errors; auto-resumed once mid-run). 100% vs random throughout;
  vs heuristic it plateaued at ~0.64–0.66 with **zero losses** (clean best 0.65625 at
  iteration 70 = `best.pt`); vs MCTS mostly draws with an occasional win (1-7-0 at iter
  100). **No grokking jump** — ordinary learning that plateaued, which (per the playbook)
  honestly establishes the next bottleneck rather than a strength breakthrough.
- **Self-play W/L is ~balanced by construction** (symmetric two-seat self-play):
  wins ≈ losses with many draws. Do **not** read self-play W/L as strength; strength is
  the baseline evals (random/heuristic/MCTS) only.
- **Code audit (2026-07-08):** no result-invalidating defect; core PUCT/replay/optimizer/
  checkpoint/encoder are correct and the published numbers are arithmetically honest. Four
  medium eval/promotion issues remain — the one that matters here is that the bootstrap
  heuristic **leaks into the in-training evaluation search** (iter<30), so this run's
  early scores and its iter-20 `best.pt` are heuristic-aided; the clean iter≥30 scores are
  ~0.53–0.61. See [04-code-audit.md](04-code-audit.md).

## Claim discipline (from the project's own playbook)

Strength claims require: 90%+ vs random on both seats, improvement over the incumbent on
a fixed selection block, no material heuristic regression, a periodic stronger-search
diagnostic, and **one untouched, role-balanced final holdout after selection freezes**.
Always report W-D-L separately (a 50% score of all draws ≠ balanced wins/losses).
The native run's in-training eval uses selection seed base **900001**; a *final holdout*
must use a distinct, pre-registered seed range (e.g. 1500001) after the run finishes.
