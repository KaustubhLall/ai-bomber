# 09 — Viz / Replay / Arena work plan & status

_2026-07-09. Tracks the sim + visualization batch (reward/architecture analysis, MCTS
self-play replays, flame-correct polished playback, arena/bracket). Status kept current._

## Reward & architecture analysis (design questions)
- ✅ **Draw = −0.9 tradeoff + configurable option** → `docs/REWARD_AND_MODEL_DESIGN.md`.
  Verdict: valid as *general-sum reward shaping* (not the true zero-sum payoff); it's an
  aggression dial (`attack iff win:loss odds > (1+d)/(1−d)`). Already configurable
  (`--timeout-draw-value`, `--mutual-death-value`, validated to [−1,0]). Keep default −0.5;
  −0.9 risks throwing drawable games vs strong opponents + collapses value-head resolution
  (draw≈loss). ✅ **`--draw-value X` implemented** (native trainer): sets both per-seat draw
  values, validated to [−1,0], and the terminal-value tactical shaping now scales down as the
  draw value hardens toward −1 (`shape_scale = clamp((1+d)/0.5, 0, 1)`) so `−0.9` stays ≈ −0.9
  — while exactly preserving the trained `−0.5` regime (scale = 1 for all d ≥ −0.5). Default
  unchanged. Verified: `evaluate ... --draw-value -0.8` accepted and applied.
- ✅ **ResNet vs Transformer** → same doc. Corrected: current model is a **conv ResNet**, not
  an RNN. Recommendation: keep the ResNet (already whole-board receptive field; inference
  speed = search strength; ViT risks fewer sims/sec). Reach for attention only for larger
  boards / history / diagnosed long-range failures; prefer a conv+attention **hybrid** behind
  a `--model-type` flag if experimenting. Obs/replay pipeline unchanged either way.

## Sim / replay
- ✅ **Replay format v4** (`src/core/replay.c`): the per-frame `BomberState` now includes
  `flame_ttl`/`flame_owner` and sudden-death walls, so v4 frames render flames correctly.
  Bumped `REPLAY_VERSION 3→4`; the loader cleanly rejects old v3 files (size/version change).
  *Old replays are not re-run — regenerate with the current binary.*
- ✅ **MCTS self-play replay generation** (`src/cli/main_headless.c`): new flags
  `--sudden-death-start N --shrink-interval N --flame-duration N` let a headless game match the
  training dynamics. Example (verified — produces a decisive game with flame):
  `bomber_headless --mode battle --agent mcts --enemy mcts --sudden-death-start 120
  --episodes 1 --seed 7 --replay mcts_selfplay.bin`
- ✅ **Flame-render investigation**: old viz showed no flame for two reasons — (a) old replays
  predate the flame struct (format-incompatible, now version-gated), and (b) the renderer
  never read `flame_ttl`. Both fixed. Documented here.
- ✅ **Replay round-trip test** (`tests/test_replay_flame.c`, ctest `test_replay_flame`): drives
  a controlled battle so a real bomb detonation lays persistent flame and the sudden-death arena
  walls an interior tile, then asserts both the flame layer (`flame_ttl`/`flame_owner`, byte-for-
  byte) and the new solid walls survive `replay_save`/`replay_load` and re-simulate
  deterministically (`replay_validate`). Guards against a serializer that silently drops the v4
  flame state. Passes; full suite 22/22 green.

## Visualization
- ✅ **Renderer polish** (`src/viz/renderer.c`): persistent flame overlay (fades by ttl),
  blast-range preview per active bomb, dead-agent ✕ markers, mini-arena flame, flame legend.
  Verified via headless `--smoke-screenshot` (see screenshots in scratchpad / repo root).
- ✅ **Replay viewer** (`src/viz/main_viz.c`): `bomber_viz --replay <v4-file>` loads into the
  History view; `draw_history_view` builds the snapshot from the full frame state and renders
  through `renderer_draw_arena`, so flame/bombs/powerups/identities show. Frame stepping
  (Left/Right, PageUp/PageDown), play/pause (Space) work. Added `--smoke-frame N` and an
  auto "most-action frame" picker for headless verification screenshots.
- ✅ **Live arena / bracket mode (scripted agents)**: the Compare view renders a live multigrid
  of independent matchups, `M` launches a live recorded matchup, and the headless
  **round-robin tournament** engine (`--tournament a,b,c`) plays role-balanced games among the
  scripted agents (random/scripted/heuristic/greedy/evasive/alpha-beta/mcts), prints a W-D-L
  matrix + score-ranked standings, and saves one replay per matchup — the batch/bracket engine
  that feeds the viewer.
- ✅ **Model-vs-model (neural checkpoints)** — the honest architecture given the torch split:
  the neural net lives in the C++/LibTorch trainer (the C viz/headless binaries do **not** link
  torch), so checkpoint games are played by the binary that owns the net and surfaced to the
  **same** polished viewer as v4 replays. Implemented in `bomber_alphazero_native evaluate`:
  - `--replay-out FILE` records one representative game of the loaded checkpoint (vs heuristic,
    or vs native MCTS with `--eval-mcts`) as a v4 `.bin`.
  - `--replay-incumbent CKPT.pt` (with `--replay-out`) records **checkpoint-vs-checkpoint**.
  Recording taps match 0 of the existing batched eval loop (`replay_record_env` per step →
  `replay_save`), so it reuses the tested search/step path and needs no new game loop. The
  replays carry the training dynamics (native eval defaults `sudden_death_start=120`,
  `flame_duration=2`), so flames + the closing arena render. Verified end-to-end with
  `best.pt` (iter 70): watched **best vs heuristic** and **best vs iteration_000010** in
  `bomber_viz --replay`.
- ⬜ **Fully-unified in-process arena (scripted + neural in ONE standings table)**: would need a
  torch-linked implementation of the C `Agent` act hook (the `AGENT_EXTERNAL` slot already
  exists as the injection point) or a batch-level bridge where the C tournament shells the
  neural matchups to the native binary and folds the JSON results into standings. Not needed for
  the current goal — the two-binary split (C round-robin for scripted, native `evaluate` +
  `evaluate_native_ladder.py` for neural W-D-L vs the ladder) already covers MCTS-vs-heuristic
  and checkpoint-vs-checkpoint; documented here as the remaining convenience.

## How to watch a game

### MCTS / scripted self-play (C headless, no torch)
```
# 1) generate (matches training dynamics: sudden death + persistent flame)
bomber_headless --mode battle --agent mcts --enemy mcts --sudden-death-start 120 \
                --episodes 1 --seed 7 --replay mcts_selfplay.bin
# 2) watch (polished, flame-correct)
bomber_viz --replay mcts_selfplay.bin        # Space play/pause, Left/Right step
```

### A trained neural checkpoint (native trainer owns the net → exports a v4 replay)
```
# checkpoint vs heuristic (add --eval-mcts to face native MCTS instead)
bomber_alphazero_native evaluate --run-dir results/<run> --checkpoint best.pt \
    --channels 128 --blocks 10 --eval-games 1 --eval-simulations 48 \
    --replay-out best_vs_heuristic.bin
# checkpoint vs checkpoint (model-vs-model)
bomber_alphazero_native evaluate --run-dir results/<run> --checkpoint best.pt \
    --channels 128 --blocks 10 --eval-games 1 --eval-simulations 48 \
    --replay-out best_vs_iter10.bin --replay-incumbent results/<run>/iteration_000010.pt
# then, same polished viewer:
bomber_viz --replay best_vs_iter10.bin
```
(Run the native binary via `tools/run_native_alphazero.ps1` or with the LibTorch `lib/` on PATH.
`--channels/--blocks` must match the checkpoint. The recorded game is match 0 at
`--eval-seed-base`; re-run with a different seed base for a different game.)

## Build notes
- Viz + headless + all ctests build in `build/` (VS generator, `AI_BOMBER_BUILD_VIZ=ON`, raylib
  cached) — independent of the native trainer. Full suite: **22/22 green** (adds
  `test_replay_flame`).
- Native trainer rebuilt in `build-native-gpu/` (Release, LibTorch/CUDA) with the `--draw-value`
  alias + shaping scale and the checkpoint replay-export flags (`--replay-out`,
  `--replay-incumbent`). No self-play run was active, so the rebuild was safe. `--model-type`
  hybrid remains unimplemented (see `docs/REWARD_AND_MODEL_DESIGN.md`; only worth it for larger
  boards / history / diagnosed long-range failures).
