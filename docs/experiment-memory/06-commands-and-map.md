# 06 — Commands & Source Map

_Last updated: 2026-07-08._

## Source tree (the parts that matter for the experiment)

```
src/
  core/       rng, config, math_util, ring_buffer, replay, metrics, playback_clock, match_history
  env/        bomber_env, bomber_rules, bomber_bombs, bomber_blast, bomber_danger,
              bomber_observation, bomber_reward, bomber_map, bomber_state, types
  agents/     agent, random, scripted, heuristic_bomber, greedy_crate, evasive, search_agent (alpha-beta + MCTS)
  sim/        runner, benchmark, evaluator (evaluator_score_state = the tactical leaf/tiebreak value)
  training/   training_api.{h,c}  ← stable C ABI v6 (create/clone/encode/masks/step_joint/outcome/hash/tactical/baseline)
              encoding.{h,c}       ← 17ch × 11×11 centered encoder (shared ground truth)
    native/   model.{h,cpp}   ← residual policy/value tower
              trainer.{h,cpp} ← ReplayBuffer, BatchedMcts (PUCT), self-play, teacher, optimize, evaluate, checkpoint  ← THE CORE
              main.cpp        ← CLI entry (train/evaluate/benchmark), CUDA bootstrap
  cli/        main_headless.c, main_benchmark.c
  viz/        raylib visualizer (renderer, dashboard, charts, ui_controls, viz_session, layout, theme)
tools/
  train_alphazero.py          ← NumPy reference trainer CLI (ctypes → C sim)
  evaluate_alphazero.py       ← standalone evaluator for reference checkpoints
  run_native_alphazero.ps1    ← launcher: puts torch DLL dir on PATH, then benchmark/train/evaluate
  alphazero/                  ← env(ctypes), model, mcts, trainer, checkpoint, progress
tests/       CTest suite incl. test_alphazero.py, test_training_api.c, test_search_api.c, ...
docs/        design + protocol docs (see below)
docs/experiment-memory/  ← THIS FOLDER
results/     run dirs + tournament JSON (alphazero-native-grokking-v1 = the live run)
```

## Key docs (pre-existing)

`ALPHAZERO_TRAINING.md` (NumPy reference), `NATIVE_ALPHAZERO.md` (native build/train/
resume/evaluate), `ALPHAZERO_RESULTS.md` (verified iter-45 result + MCTS boundary),
`SELF_PLAY_GROKKING_PLAYBOOK.md` (portability + claim gates — read before any strength
claim), `EVALUATION_AUDIT.md` (adversarial eval standard), `EXPERIMENTS.md`,
`MCTS_SELF_PLAY_PLAN.md`, `FUTURE_MODELS.md`, `ARCHITECTURE.md`, `ENV_API.md`,
`OBSERVATION.md`, `REWARD.md`, `DANGER_MAP.md`, `VISUALIZER.md`.

## Build

```powershell
# Native GPU trainer (opt-in; needs torch wheel in .venv-gpu or AI_BOMBER_TORCH_ROOT)
cmake -S . -B build-native-gpu -G "Visual Studio 18 2026" -A x64 `
  -DAI_BOMBER_BUILD_VIZ=OFF -DAI_BOMBER_BUILD_TESTS=ON -DAI_BOMBER_BUILD_NATIVE_ALPHAZERO=ON
cmake --build build-native-gpu --config Release -j 12
ctest --test-dir build-native-gpu -C Release --output-on-failure

# NumPy-reference bridge DLL only
cmake -S . -B build-codex-vs -DAI_BOMBER_BUILD_VIZ=OFF
cmake --build build-codex-vs --config Release --target bomber_training
```

## Train / resume / evaluate (native)

```powershell
# Resume the live run to a larger target (same command, no --fresh; --iterations is TOTAL)
.\tools\run_native_alphazero.ps1 train --run-dir results\alphazero-native-grokking-v1 --iterations 200

# Final untouched holdout AFTER the run finishes (distinct seed base from 900001!)
.\tools\run_native_alphazero.ps1 evaluate --run-dir results\alphazero-native-grokking-v1 `
  --eval-seed-base 1500001 --eval-games 64 --eval-simulations 96 --eval-mcts --mcts-eval-games 8
```

## Gotchas / things future-me should not relearn the hard way

- **Do not pass `--fresh` to resume** — it deletes checkpoints + metrics in the run dir.
- **Config signature** only pins `format|w|h|max_steps|crate_density|channels|blocks|
  replay_capacity`. Everything else (iterations, eval cadence, teacher, LR) can change on
  resume — which is exactly why this run's eval cadence changes at iteration 10.
- **`metrics.jsonl` mixes runs** — it is append-only across resumes, so early rows can
  come from a differently-configured earlier run.
- **Self-play W/L is symmetric by construction** — never cite it as strength. Use the
  random/heuristic/MCTS evals only, both seats, W-D-L reported separately.
- **CUDA is mandatory** for `bomber_alphazero_native`; launch via
  `tools/run_native_alphazero.ps1` so `torch_cuda.dll` and the wheel's DLL dir are found.
- **The native trainer's teacher is the heuristic agent** (hardcoded), unlike the NumPy
  reference whose default teacher is MCTS. Curricula differ; encoder/rules do not.
