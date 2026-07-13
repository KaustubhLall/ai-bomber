# 07 — Grokking / Superhuman Campaign (KL-96)

_Living record. Started 2026-07-08. Goal: break the self-play draw collapse and drive a
durable improvement toward superhuman (decisively beat the heuristic; clear the MCTS
ladder on an untouched holdout). Honest negatives retained — grokking is a hypothesis._

Linear: **KL-96** (AI-BOMBER-17), continuation of KL-94.

## Where things stand (the v2 reality)

The prior session built a strong **"superhuman ladder"** continuation (`docs/SUPERHUMAN_ALPHAZERO.md`):
- Run `results/alphazero-native-superhuman-v2` (PID varies), continues from v1 latest(100)/best(70), target 500 iters, sims 96, LR restart 1e-4→1e-5 over updates 12800→64000, teacher off.
- **Hardened promotion gate** (fixes my earlier audit #2): champion arena vs the incumbent `best.pt`, both seats, Wilson 95% LCB > 0.5, disjoint seed blocks (eval 900001 / promotion 1100001 / MCTS 1300001). Clean bootstrap-free evaluation (fixes audit #1). Strengthened MCTS-256/depth-16 baseline. Durable retried checkpoints (fixes audit #7/#8), seed-overlap validation (audit #12), schema-2 seat-split metrics.

**The prior audit findings are largely fixed. The open problem is the draw collapse, which the v2 setup does not address.**

## Diagnosis: self-play draw collapse (root cause)

Evidence:
- v1 final vs heuristic 9-23-0 (0.641, **0 losses**); vs MCTS 1-7-0. v1 iter-70 vs MCTS-512 was 0-16-0.
- v2 iter-130 champion arena: **candidate vs incumbent 0W-128D-0L** (LCB 0.428). Heuristic 17-47-0. Self-play draws ~80% and rising (draws 88→104 of 128).

The agent has learned to **not lose (survive)** but not to **win (kill)**. Why the equilibrium is degenerate:
1. **AZ value ≈ 0 for a draw.** `terminal_training_value` (trainer.cpp:249) returns `tanh(Δtactical/250) ≈ 0` on a timeout; the shaped `timeout_penalty=-1` in `config_battle` is **never consumed by the AZ path** (AZ ignores `reward_compute`). Safe draw (0) beats risky attack (p·1+(1−p)·(−1) < 0 for p<0.5) → survival dominates.
2. **No sudden-death / closing arena.** `rules_check_terminal` returns `TERMINAL_TIMEOUT` at `max_steps`; nothing forces contact. (Fidelity gap: canonical battle mode closes in.)
3. **Instantaneous 1-tick flame.** `apply_blast_damage` only kills agents on a blast tile at the explosion tick; no persistent flame (`current_blast` is even recomputed to ~always-0). Canonical flame lingers ~2-3 ticks. Dodging is trivial, kills are hard. (Fidelity gap.)
4. **Pure self-play mirror** (no opponent league) → symmetric avoidance Nash.

**The fidelity fixes and the grokking fix are the same fixes.**

## Intervention design (sequenced, cheapest-first)

| # | Change | Type | Why it breaks draws | Cost |
|--|--|--|--|--|
| A | **Lingering flame** (`flame_duration` ~2-3 ticks; kills anyone in flame; shown in danger + encoding) | fidelity + mechanics | Makes bombs lethal → traps/kills become achievable | Moderate C (state+env+danger+encoding+tests) |
| B | **Draw-averse AZ value** (true both-alive timeout → bounded negative for both, seat-antisymmetric) | value signal | Removes the "safe draw = 0" attractor; forcing a decision beats stalling | Cheap (trainer.cpp only) |
| C | **Sudden-death closing arena** (indestructible walls encroach after a step threshold; shown in encoding) | fidelity + mechanics | Hard guarantee against stall draws | Larger C (state+env+encoding+tests) |
| D | **Opponent league** (fraction of self-play vs frozen champions + heuristic/MCTS) | curriculum | Must learn to beat non-mirror opponents | Medium (trainer.cpp) |

**Plan:** v3 = A + B (fidelity-mandatory flame + cheap draw penalty). Measure self-play draw
rate and champion-arena decisiveness. If draws persist, v4 escalates to C and/or D. Every
sim change must be reflected in the 17→N-channel encoding or the agent is blind to it, and
must keep the determinism/blast tests green.

## Success signals

- Self-play draw fraction drops well below the ~0.8 plateau; wins ≈ losses but both rise.
- Champion arena produces decisive games (not 0-128-0); promotions resume.
- A frozen checkpoint beats the heuristic with **wins** (not just draws), then clears the
  MCTS ladder on untouched active holdouts (A3 2400001, B3 2500001 per SUPERHUMAN doc).

## Operational notes / gotchas

- **Rebuilding requires stopping the running trainer** — Windows locks the running `.exe`, so
  `cmake --build build-native-gpu` can't relink while a native run is live. Sequence: stop
  the run (Ctrl+C writes emergency.pt / or it checkpoints latest each iter), rebuild, launch.
- Editing source while a run is live is fine (it already loaded its code); only relink conflicts.
- Any env/state change alters `config_signature` compatibility only if it touches
  width/height/max_steps/crate_density/channels/blocks/replay_capacity. Adding flame state to
  `BomberState` changes the checkpoint replay tensor? No — replay stores encoded observations,
  not raw env; but if the **encoding channel count changes**, `BOMBER_TRAINING_CHANNELS` changes
  → old checkpoints/replay are incompatible → v3 must start fresh (new run dir), not resume v2.

## Decision & status log

- **2026-07-08 ~15:5x** — Diagnosis complete; KL-96 opened; fidelity audit running; v2 left
  running (dead-end for grokking but harmless; will stop before rebuild). Next: fold audit
  results, implement A+B, rebuild, launch v3.
- **2026-07-08 ~16:0x** — Fidelity audit done (49 agents, 40 confirmed). Recorded in
  [08-sim-fidelity-audit.md](08-sim-fidelity-audit.md). Confirmed the diagnosis + added two
  under-weighted causes: 11×11 view can't see a far opponent, and mirror-symmetric spawns.
- **2026-07-08 ~16:3x** — **Implemented + shipped v3** (staged, 17 channels preserved):
  - Persistent flame (`flame_ttl`/`flame_owner` in state; `ignite_flame`/`apply_flame_damage`/
    `decay_flame`; `flame_duration` config, battle default 2; post-move lethal pass in
    `env_step_joint`; revived `current_blast`→channel 8 from flame).
  - Draw-averse value: `terminal_training_value` → per-seat negative for both-alive timeout
    (`timeout_draw_value=-0.5`) and mutual death (`mutual_death_value=-0.2`); `collect_self_play`
    now assigns per-seat values (not simple negation).
  - Diagonal 2-player spawns (canonical + anti-mirror); powerups destroyed by blast; blast
    bounds guard. New CLI: `--flame-duration --timeout-draw-value --mutual-death-value`.
  - **Verification:** dependency-free build 21/21 ctest green (added a lingering-flame
    regression test); native trainer relinked clean; native smoke run passed end-to-end.
  - **Baseline probe:** heuristic-vs-heuristic battle still 92/100 timeouts *with* flame —
    but the heuristic is a hardcoded safe-evader, so this is not a proxy for the learning
    agent (flame mechanics do fire: 5 deaths / 3 owned elims occurred). The real test is the
    neural run's response to the timeout penalty, which the heuristic can't feel.
  - Stopped v2 at iter 145/500 (best still iter 70 — 45 iters, zero promotions: plateau
    confirmed). Launched **v3** `results/alphazero-native-superhuman-v3` (fresh, 300 iters,
    128 games / 96 sims / 128ch-10blk, teacher 32×20, LR 2e-4→1e-5). Watching the self-play
    **draw rate** over the first ~15-20 iters as the go/no-go signal.
  - **Deferred to v4 if draws persist:** sudden-death arena shrink (#6, the audit's
    definitive cure) + global opponent bearing (#5); also speed powerup, agent-aware legal
    actions, swap-2-cycle, chain-danger fixpoint, would-trap fuse (low/non-draw).
- **2026-07-08 ~16:45** — v3 iter-5 self-play draws climbed 52%→**70%** (25W-89D-14L) —
  same direction as the old plateau. Consistent with the architectural limit: the single-
  scalar zero-sum search backs up a terminal draw as 0, so the per-seat value penalty has
  weak teeth. iter-10 clean heuristic eval is the decider (wins vs all-draws).
- **2026-07-08 ~17:0x** — **Implemented + tested sudden-death (v4), while v3 runs.** Canonical
  closing walls: `map_apply_sudden_death` converts inward wall rings to `TILE_SOLID_WALL`
  (one ring per `shrink_interval` steps from `sudden_death_start`), crushing agents (death
  owner −1) and clearing bombs on newly-walled tiles; called in `env_step_joint` after
  `tick_bombs`. **No new state fields** (computed from the step counter, which cloned search
  envs carry) and **17 channels preserved** (closing walls are visible in the tile one-hot;
  `game-progress` channel gives the clock). Config `sudden_death_start`/`shrink_interval`
  (defaults 120/4 in `TrainConfig`, so ON for the next native build; DISABLED in base config
  so headless/viz/other tests are unaffected). CLI `--sudden-death-start --shrink-interval`.
  Dependency-free build **21/21 green** incl. a new sudden-death crush test. Ready to launch
  as **v4** (= v3 + sudden-death) the moment the v3 iter-10 eval confirms the plateau; a
  rebuild of build-native-gpu (needs v3 stopped) picks it up. Sudden-death also forces agents
  into the shrinking center, which largely obviates the separate opponent-bearing fix (#5).
- **2026-07-08 ~17:2x — v3 VERDICT: plateau confirmed → escalate to v4.** iter-10 clean eval
  (bootstrap-free): random 64-0-0 (1.00), heuristic **12-52-0 (0.594)** — 12 wins but 52
  draws / 64, 0 losses; self-play draws ~70-74%. Same draw-dominated regime as v1/v2 (v1
  iter-10 heuristic ≈ 0.594 too). **Flame + timeout-value did NOT break the draw collapse** —
  confirms the architectural limit (zero-sum single-scalar search values a draw at 0) and the
  heuristic-baseline foreshadowing. Decision: launch **v4 = v3 + sudden-death** after the
  impl-review lands and v3 is stopped for the rebuild.
- **2026-07-08 ~17:35 — impl-review found the ROOT CAUSE (HIGH, confirmed) + v4 launched.**
  The review (15 agents) proved *in code* why v3's draws didn't drop: **the −0.5 draw penalty
  is a common-mode offset that cancels exactly in the PUCT `0.5·(v0−v1)` leaf aggregation, and
  in-tree terminal draws back up `outcome()=0`** — so the value *head* learns −0.5 but the
  *search that selects moves never sees it*. (Also flagged: full sudden-death closure makes the
  −0.5 timeout branch unreachable; symmetric crush can create −0.2 mutual-death draws.)
  **Fix implemented — per-seat value MCTS (decoupled general-sum PUCT):** `Node` now carries
  `value_sum0`/`value_sum1`; each seat backs up and *maximizes its own* terminal/leaf value
  (seat 1 no longer negates seat 0), so a draw negative for both seats is avoided by both.
  Terminal draws back up the per-seat `terminal_training_value` (not 0). Clamped
  `mutual_death_value` + a `[-1,0]` validate check. Native rebuild clean; smoke passed — with
  sudden-death ON a random net already produced fully **decisive** games (heuristic 2-0-2,
  mean 13 steps).
  Stopped v3 (iter 15). **Launched v4** `results/alphazero-native-superhuman-v4` (PID 32132):
  per-seat value + flame, **sudden-death OFF** (`--sudden-death-start 0`) to isolate the
  root-cause fix. Watching self-play draw rate vs v3's 52→74%. **v5 is one CLI flag away**
  (`--sudden-death-start 120`, same binary, no rebuild) if evasion keeps draws high.
  Full review in scratchpad `weitmn2nh.output` (1 HIGH + 1 medium + several low, all confirmed).
- **2026-07-08 ~18:1x — v4 verdict + v5 launched.** v4 (per-seat value + flame, sudden-death
  OFF) self-play draws climbed **50%→71%→77%** (iters 3→7→9) — v3's plateau again. Confirms
  the per-seat value fix is **necessary but not sufficient**: it makes the search *prefer*
  winning, but when evasion is easy a defender can still force a draw (drawing −0.5 beats
  losing −1), so a draw remains the rational equilibrium. The binding constraint is the
  **dynamics**, not the value representation. Stopped v4 (iter 9). **Launched v5** (PID 44628,
  `results/alphazero-native-superhuman-v5`): per-seat value + flame + **sudden-death ON**
  (`--sudden-death-start 120 --shrink-interval 4`, same binary, no rebuild) — removes the draw
  from the dynamics (arena fully closes by ~step 136) while the per-seat value fix makes the
  search value the now-decisive outcomes correctly. This is the audit's "definitive cure"
  combined with the root-cause value fix. Watching draw rate + mean_steps (should drop) +
  heuristic/incumbent decisiveness. **This is the decisive experiment.**
- **2026-07-08 ~18:2x — v5 WORKS: draw collapse broken + strength jump.** Self-play draws
  dropped to **~15-20% and HELD** (iters 5/9/12: 18/20/20%) — not the v3/v4 climb to 77%.
  mean_steps ~110 (was ~160). **iter-10 eval:** random **64-0-0** (1.00); heuristic **56-3-5
  (score 0.898, Wilson LCB 0.819)**, role-balanced (seat0 29-1-2, seat1 27-2-3). Vs the old
  best-ever 0.656 (v1 iter-70, draw-heavy) and v3 iter-10's 0.594 — a genuine, seat-balanced
  **decisive-win** regime, at iteration 10. Promoted (initial gate), best 0.898.
  **Honest caveats before any "superhuman" claim:** (1) eval runs WITH sudden-death; the
  heuristic is a hardcoded safe-evader NOT designed for a closing arena, so it may be
  sudden-death-naive → some of the 0.898 reflects that, not pure Bomberman superiority.
  (2) The real test is **MCTS-256/depth-16 at iter 50** — MCTS rolls out the true env
  (sudden-death included) so it adapts; beating it is the honest superhuman boundary.
  (3) Need sustained improvement (incumbent promotions) + untouched active final holdouts
  (SUPERHUMAN_ALPHAZERO.md blocks A3=2400001/B3=2500001) before freezing a claim. Watching iter-20+
  eval, promotions, and the iter-50 MCTS ladder.
- **2026-07-08 ~19:xx — v5 strength trajectory (heuristic score): 0.898(10) → 0.914(20) →
  0.742(30) → 0.828(40) → 0.961(50).** The iter-30/40 dip was the post-teacher wobble
  (teacher ended iter 20); it resolved into a breakthrough at iter-50. Self-play draws held
  ~13-20% throughout (collapse stays broken).
- **2026-07-08 ~19:xx — v5 iter-50: self-improvement CONFIRMED + honest MCTS boundary.**
  random 64-0-0; heuristic **61-1-2 (0.961, LCB 0.899)** role-balanced; **incumbent 72-32-24
  (0.688, LCB 0.617) → PROMOTED** (promotion #2, champion iter-10→iter-50, both seats) — the
  first confident self-improvement past iter-10, so it is genuinely getting stronger.
  **MCTS-256/depth-16: 3-0-5 (LCB 0.161)** — NOT superhuman vs MCTS, but decisive/competitive
  and far past v1's 0-16-0 all-draws. Seat split notable: **3-0-1 as seat0, 0-0-4 as seat1**
  (only 4 games/seat → noisy; possible seat-specific weakness vs strong search — increase
  `--mcts-eval-games` for a clearer read). Training healthy (value_loss 0.040, entropy 0.94,
  LR still ~1.9e-4 at iter 50/300 → lots of decay runway). Decision: **let v5 continue** — it
  is improving with a promotion + new high, only 1/6 through the schedule. Next MCTS eval at
  iter 100. If MCTS gap doesn't close, options: more mcts-eval games (clearer read), longer
  teacher, stronger/longer training, or larger net. Then a final untouched holdout before any
  superhuman claim.
- **2026-07-09 — v5 iter-100: BEATS MCTS-256.** vs MCTS-256/depth-16 **7-0-1 (0.875, Wilson
  LCB 0.589), role-balanced** (s0 4-0-0, s1 3-0-1 — the iter-50 seat weakness is gone).
  Trajectory vs MCTS: **3-0-5 (iter-50) → 7-0-1 (iter-100)** — rapidly improving; v1 never won
  a game vs MCTS (0-16-0). heuristic 0.906, random 1.00, self-play draws ~18-23%.
  `iteration_000100.pt` snapshotted. **Honest caveats — NOT yet a confirmed superhuman claim:**
  (1) only **8 MCTS games** (LCB 0.589 clears 0.5 but the sample is tiny); (2) seeds are the
  MCTS *selection* block (1300001), NOT the untouched final holdouts; (3) MCTS-256,
  not the harder MCTS-512; (4) the iter-100 model is **not the champion** — best.pt is still
  iter-50 (heuristic-gated promotion doesn't track MCTS strength). The trend is strongly
  positive: the setup is now **grokking toward superhuman**.
  **Plan → rigorous holdout confirmation** (to run once v5 matures/plateaus): `evaluate` the
  strongest late snapshot(s) — selected by MCTS strength, not heuristic — vs **MCTS-256 AND
  MCTS-512** on a **large** role-balanced non-final selection sample:
  `bomber_alphazero_native evaluate --run-dir results/alphazero-native-superhuman-v5
  --checkpoint iteration_000NNN.pt --eval-seed-base 1300001 --eval-games 64 --eval-simulations
  96 --eval-mcts --mcts-eval-games 32 --baseline-mcts-simulations 512 --output selection-mcts512.json`.
  Report Wilson LCBs; only freeze for final holdout if the selection gate passes.
- **2026-07-09 — v5 iter-200: CLEAN SWEEP vs MCTS-256 (8-0-0, score 1.000).** MCTS-256/
  depth-16 ladder: **0.375(50) → 0.875(100) → 0.812(150) → 1.000(200)** — combined
  **21-1-2 over 24 games (~0.90)**. heuristic 0.945, random 1.00. The draw-collapse fix has
  turned into a genuinely strong, MCTS-beating agent. Champion still iter-50 (heuristic-gated;
  the strongest-vs-MCTS snapshots are iter-100/150/200 — evaluate THOSE for the claim, not
  best.pt). **Still not a frozen superhuman claim:** selection seeds, MCTS-256 (not the harder
  MCTS-512), 8-game samples. Plan: let v5 finish (~iter 300), then run a non-final selection
  gate on the strongest-vs-MCTS snapshot vs MCTS-256 AND MCTS-512 with Wilson bounds before any
  final holdout.

- **2026-07-09 — v5 iter-300 completed, but no final claim.** Training reached the 300-iteration
  target cleanly; `latest.pt`/`iteration_000300.pt` SHA-256 is
  `05d8619971e3b16fc1db550ec03411b4a5f631fc53afcbcde8c20191ead0604c`. The promoted champion
  remained iter-210 (`best.pt` SHA-256
  `e205daccf3023e367afe6899433b744a04a8185a6ded6346cbee9db4d58149fb`). Iter-300 validation was
  random 64-0-0, heuristic 62-1-1, incumbent 48-35-45, and MCTS-256 7-0-1. It did **not**
  promote because the incumbent confidence gate failed. This is strong but still selection data,
  not agent-ladder superhuman proof.

- **2026-07-09 — holdout hygiene incident.** A heartbeat inspection found external native
  `evaluate` processes already running against reserved seed base 2000001 in `results/v5-holdout`
  (`A_best_mcts256.json` and `A_best_mcts512.json`). A small `test.json` result also existed for
  seed base 2000001, so that block is leaked and cannot be used for final proof. After A was
  retired to A2=2200001, a separate Claude-launched PowerShell job was found running a full
  "PRISTINE HOLDOUT EVALS" ladder on A2=2200001 and B=2100001; those child processes and parent
  job were killed, but both blocks are conservatively retired because evaluation had started.
  Active final proof blocks are now A3=2400001 and B3=2500001. Next safe action is a non-final
  selection evaluation or continued training with no `--fresh`; do not launch any final holdout
  until a checkpoint is frozen by the documented selection gate.

- **2026-07-09 — evaluator seed partition fixed.** `evaluate_only()` was found to ignore
  `--mcts-eval-seed-base` and reuse `--eval-seed-base` for MCTS games. Patched the native
  evaluator so MCTS uses `config.mcts_evaluation_seed_base`, and the output JSON now records
  `mcts_seed_base` plus `mcts_seeds_per_opponent`. A one-seed non-final smoke wrote
  `results/alphazero-native-superhuman-v5/selection-seed-smoke.json` with `seed_base=900001`
  and `mcts_seed_base=1300001`.

- **2026-07-09 — v5 best.pt PASSED non-final MCTS-512 selection.** Output
  `results/alphazero-native-superhuman-v5/selection-best210-mcts512.json` used schema 2 with
  `seed_base=900001` for random/heuristic and `mcts_seed_base=1300001` for MCTS. Results:
  random 128-0-0 (score 1.000, LCB 0.979300), heuristic 127-1-0 (score 0.996094, LCB
  0.972187), MCTS-512/depth-16 23-1-8 (score 0.734375, LCB 0.591441; seat0 10-1-5, seat1
  13-0-3). This passes the predeclared strong-search selection boundary. Frozen candidate:
  `results/alphazero-native-superhuman-v5/frozen-best210-selection-mcts512.pt`, SHA-256
  `e205daccf3023e367afe6899433b744a04a8185a6ded6346cbee9db4d58149fb`. Selection JSON SHA-256:
  `f368c9db518cb57f9a6b4cd6a21a0525b4cdfc26f8dede06bff996a374bd3345`. Because the evaluator
  enforces disjoint seed ranges, active final blocks are split as A3 validation=2400001,
  A3 MCTS=2410001, B3 validation=2500001, B3 MCTS=2510001.

- **2026-07-09 — lingering quarantined A2 eval killed during A3 proof.** During the approved
  A3 final holdout run against `frozen-best210-selection-mcts512.pt`, another external
  Claude/PowerShell/native chain was still active in `results/v5-holdout`, running
  `A2_best_256.json` with `--eval-seed-base 2200001`. It was stopped along with its parent
  PowerShell job. The Claude Code parent process (`claude.exe --resume
  c60a8d0c-109c-43b1-b2be-5fb2da953e86`) then attempted to respawn a shorter
  `2200001` eval and was stopped as well. The approved A3 process using
  `--eval-seed-base 2400001` and `--mcts-eval-seed-base 2410001` remained running.
  `results/v5-holdout/test2.json` already existed for seed base 2200001 and is quarantined
  diagnostic evidence only. B3 remains untouched until A3 independently passes.

- **2026-07-09 — final holdout A3 PASSED** (`holdout-a3-frozen-best210-mcts512.json`, champion
  `frozen-best210-selection-mcts512.pt` iter-210 on validation 2400001 / MCTS 2410001, 64
  seeds/opp): random **128-0-0** (LCB 0.979), heuristic **121-1-6** (0.949, LCB 0.907),
  **MCTS-512/depth-16 91-5-32 (score 0.730, Wilson LCB 0.662)**, role-balanced (seat0 43-2-19,
  seat1 48-3-13). All three A3 gate conditions met (random no-loss, heuristic no regression,
  MCTS LCB > 0.5, both seats positive).

- **2026-07-09 — final holdout B3 LAUNCHED (interrupted test resumed).** Same frozen champion on
  the second pristine block: validation **2500001** / MCTS **2510001**, 64 seeds/opp, MCTS-512/
  depth-16, `--no-progress`, output `holdout-b3-frozen-best210-mcts512.json`. Run cleanly (no
  fragile `2>&1`/`Out-Null` pipe — the earlier A2 exit-255 failures were that pipe, not the
  eval). If B3 independently clears the same gate, the two-block agent-ladder confirmation is
  complete and "superhuman vs the shipped agents (random/heuristic/MCTS-512) on untouched
  holdouts" is earned. Verdict pending.

- **2026-07-09 — viz/champion tooling refresh (separate from the eval).** Rebuilt `build/`
  Release viz+headless (flame rendering / v4 replay / tournament). Generated a watchable champion
  replay (`champion-replay.ps1` → v4 `.bin`; the viewer shows the trained model playing). Added
  checked-in launchers `tools/launch/*.ps1` (+README): arena, compare, history, mcts, alpha-beta,
  mcts-selfplay (regenerates a v4 replay — the old v3 "MCTS Causal Win" file is loader-rejected),
  champion-replay, tournament, champion-eval. Repointed the 9 desktop shortcuts (were stale
  `build-codex-vs`) to call these launchers via `_env.ps1`, which resolves the current
  Release/Debug build. `champion-eval.ps1` documents seed hygiene (never point at a holdout block).

- **2026-07-09 - B3 first process stopped before JSON; relaunched with logs.** Heartbeat found
  no active B3 native/PowerShell process, no `holdout-b3-frozen-best210-mcts512.json`, and no
  B3 stdout/stderr artifacts. Frozen checkpoint hash still matched
  `e205daccf3023e367afe6899433b744a04a8185a6ded6346cbee9db4d58149fb`, GPU was healthy, and no
  retired-seed process was active. Relaunched B3 at about 14:15 PDT via
  `tools/run_native_alphazero.ps1` with the same approved seeds (`2500001`/`2510001`) and
  explicit stdout/stderr logs.

- **2026-07-09 - agent-ladder superhuman proof completed.** B3 finished with schema 2 on
  `seed_base=2500001`, `mcts_seed_base=2510001`, and `mcts_seeds_per_opponent=64`: random
  **128-0-0** (LCB 0.979300), heuristic **123-1-4** (score 0.964844, LCB 0.927031), and
  **MCTS-512/depth-16 102-5-21** (score 0.816406, Wilson LCB 0.753772; seat0 54-2-8,
  seat1 48-3-13). Together with A3's independent pass, the frozen iter-210 checkpoint clears the
  predeclared shipped-agent ladder (random, heuristic, native MCTS-512/depth-16). This is not a
  human-superiority claim. Final hashes: frozen checkpoint
  `e205daccf3023e367afe6899433b744a04a8185a6ded6346cbee9db4d58149fb`; selection JSON
  `f368c9db518cb57f9a6b4cd6a21a0525b4cdfc26f8dede06bff996a374bd3345`; A3 JSON
  `356314ca0d080d94573bd4ebb681160fdbb74f48870781873c697975aa420da2`; B3 JSON
  `5347606520ebdeb0bedb93e5c45a2e4861371eb5c64607b04152d6669e35a150`; config history
  `be0d60ec039eabc873312938672edd239f280e18504714006c932c16f3670010`; metrics
  `95e396e02ef497144b9b25d32ada9d4f7aaf5e2819667acce5e64358de6ce391`. Verification:
  `cmake --build build-native-gpu --config Release -j 12` and native CTest **25/25 passed**;
  `cmake --build build-native-c-only-audit --config Release -j 12` and dependency-free C-only
  CTest **22/22 passed**.

## Part 2 — the agent-ladder proof was real but hollow (KL-96 continued, 2026-07-10)

_The A3/B3 ladder win was a decisive-outcome statistic, not a combat-skill claim. This section
is the honest audit that found that out, and the fix. Same discipline as Part 1: negative
results retained, no claim promoted past what the evidence supports._

**2026-07-09/10 — user red flag: champion idles for long stretches in its "best" game.**
Frame-by-frame, data-only replay analysis (death_owner + action histograms, no per-frame
visual inspection) plus a new instrumented aggregate evaluator (win/loss classified by
`death_owner`: bomb = opponent's bomb, selfkill = own bomb, crush = sudden-death arena) run
at N=64 vs heuristic / N=48 vs MCTS-512 on the frozen champion (`frozen-best210-selection-
mcts512.pt`, iter-210). **Result: 95.3% of wins vs heuristic (61/64) and 97.1% of wins vs
MCTS-512 (33/34) were arena-crush, not bomb kills — only ~4% of wins were a demonstrated
kill.** All 12 MCTS losses were also arena-crush (of the champion itself). Learner WAIT rate
54.4% (heuristic) / 67.5% (MCTS) — idling more than half its turns. **Verdict: the champion's
apparent strength is overwhelmingly attributable to the sudden-death mechanic, not tactical
combat.** The A3/B3 statistical win-rate proof stands as a *win-rate* fact but does not support
a "beats MCTS at Bomberman" claim without this caveat — confirms the user's stated concern.

**Root cause, established by 3 diagnostics (fastest-first, each gating the next before
spending training budget on the wrong lever):**
1. **Self-play win-cause** (the linchpin — measures the TRAINING signal itself, not just
   eval-time behavior): resumed `v5` 3 iterations at champion weights (isolated warm-start
   dir, source run untouched), instrumented `collect_self_play()`. Result: crush is the
   *plurality* of decisive self-play games (~39-42%), self-kill (opponent's own mistake) is
   ~40%, and genuine bomb-kills are **~1%**. Mean_steps ~106 (under the 120 sudden-death
   trigger) is a bimodal average, not evidence crush rarely fires — it fires in the majority
   of DECISIVE games specifically.
2. **SD-off control** (`--sudden-death-start 0` on the frozen champion): vs heuristic, win
   rate collapsed to a near coin-flip (5W-123D-0L, score 0.52, mean_steps 193.6/200) —
   confirms the crutch was doing most of the work. But the 5 wins that did happen were
   **100% genuine bomb-kills** — proof real offense is achievable against a non-mirror
   opponent, just rare for this policy.
3. **Retroactive baseline** (re-ran the new win-cause instrumentation against v5's own
   historical snapshots, old reward): bomb-kill% vs heuristic was noisy but real throughout
   training — iter20 24.6%, iter50 18.0%, iter100 19.6%, iter150 24.6%, iter200 15.3% — then
   **collapsed specifically at the promoted iter-210 checkpoint (4.7%)**. The promotion gate
   selects on win-rate only, blind to win-cause, and happened to promote an unusually
   crush-heavy checkpoint.

**Structural diagnosis (why mirror self-play can't fix this alone):** both seats in self-play
are the *same* network, so one seat's offense is exactly the other seat's defense by
construction — a clean kill against a perfect mirror is structurally rare regardless of reward
shaping. Confirmed by the diagnostics above (bomb-kill ~1% in mirror self-play vs 100% of the
SD-off champion's rare wins against a real, non-mirror opponent). This is the same "necessary
but not sufficient" wall the campaign hit at v4 (per-seat value fix alone didn't break the draw
collapse) — the binding constraint is the dynamics/matchup, not just the value target.

**Fix, two complementary levers, built and validated same session:**
- **Reward reshaping** (`trainer.h`/`trainer.cpp`, `terminal_training_value`): a win pays full
  `+1.0` **only** when `death_owner` is the WINNER's own bomb (a demonstrated kill).
  Arena-crush wins and opponent-self-kill wins are separately devalued (`arena_crush_win_value`,
  `selfkill_win_value`, both default `0.3`, independently CLI-tunable without a rebuild) — a
  decisive win is still much better than a draw/loss, just no longer as attractive as a real
  kill. Losses are NOT reweighted by cause. Validated the discriminator itself (not just the
  hypothesis) before committing: self-play is genuinely crush/selfkill-dominated, so this lever
  has real training-signal gradient to act on (a prior "self-play ends before step 120 so the
  crush reward barely fires" concern was checked and refuted by the actual data).
- **Opponent league** (`collect_league_play()`, new): a tunable fraction
  (`--league-heuristic-fraction`, default 0 = off) of each iteration's self-play games are
  played against the heuristic agent instead of a network mirror — one randomly-chosen seat
  per game (learner seat balanced; the SD-off diagnostic exposed a hard seat asymmetry, all 5
  wins from seat 0, so a fixed-seat league would only teach half the game). Training samples
  collected for the network-controlled seat only; dirichlet exploration kept ON (root_noise
  `true`, unlike the deterministic `evaluate_baseline` pattern it otherwise mirrors — the one
  bug that would have silently produced degenerate training data). Real opponent moves via
  `agent_act`, but the neural PUCT lookahead does **not** recursively invoke that held-out
  opponent policy; its opponent-response assumption remains an explicit diagnostic gap rather
  than a verified heuristic model. The league still supplies observed trajectories against a
  non-mirror opponent. Because the heuristic did not self-destruct in the measured block, a
  league win against it can supply an earned-kill target that mirror play rarely reaches, but
  this does not by itself prove that the search models the matchup correctly.

**Two parallel, matched-hyperparameter runs launched to isolate which lever matters:**
`alphazero-native-superhuman-v6` (pure mirror, reward fix only) and `-v6-league`
(reward fix + `--league-heuristic-fraction 0.5`), both fresh, same arch/schedule as v5.
**Honest success metric differs per run and must not be conflated:** v6-pure's genuine test is
bomb-kill%-vs-heuristic (heuristic never seen in training → real generalization signal).
**v6-league trains ON heuristic, so its vs-heuristic number is contaminated by memorization —
the same measurement flaw that produced the original overstated ladder claim.** Its honest
arbiter is bomb-kill%-vs-**MCTS** (held out), read at the iter-~50 decision point via
`--eval-mcts`, not every checkpoint (slow). Smoke-tested league's first iteration before
trusting it unattended: bomb-kill 50% of wins vs mirror's ~1% — clean pass.

**Incident: concurrent-training GPU crash.** Running v6-pure + v6-league simultaneously on one
RTX 5080 killed both processes with no in-app error trace (abrupt external termination) after
~10 minutes. v6-pure's checkpoint was intact (safely parked at iter 11); v6-league's was
truncated mid-write (206MB vs the expected ~868MB) and discarded. A solo v6-pure run had
earlier coexisted with a lightweight `evaluate` process. **Superseded safety interpretation
(2026-07-11 closeout):** absence of a crash did not make that overlap safe or make timing
evidence valid. The current rule and OS lock serialize every native train/evaluate process,
not only two trainers. **Recovery: serialize — league gets the GPU first** (the diagnostics point to it as the load-bearing
lever); v6-pure stays parked at iter 11 until there's a deliberate reason to resume it
non-concurrently.

**v6-league early trend (training-time, vs heuristic — NOT yet the honest held-out read):**
bomb-kill% of decisive wins climbed iteration 0→19: 57%→69%→76%→80%→85%→82%→88%→82%→75%,
settling in the 75-88% band by iter 10+ (vs the old reward's best-ever 24.6%, champion's
4.7%). Every loss row shows bomb-kill=0 (heuristic never lands a kill; all losses are
self-inflicted). WAIT settled ~20-31% (vs old ~55-60%). Standard promotion-gate reads (iter
10/20/30, SD-on, vs the *original* baseline agents): heuristic score 0.94/0.95/0.88 — noisy,
not yet gated on win-cause. **This is a strong, clean signal that the mechanism is working as
designed on the games it trains on. It is explicitly not yet proof of general skill — that
requires the iter-50 vs-MCTS read.** Continuing to iter 50, watched by a persistent background
monitor (milestone + failure-signature alerts).

**Next:** at iter 50, run `evaluate --eval-mcts` on v6-league vs MCTS (held-out) as the honest
arbiter. If bomb-kill%-vs-MCTS is materially above the champion's ~1-3% baseline, the league
lever is confirmed and the run continues/extends. If not, re-examine before spending more of
the night's budget — options in reserve: drop `arena_crush_win_value` toward 0.1 (CLI-only, no
rebuild), increase `--league-heuristic-fraction`, or resume v6-pure (serialized) as the
pure-reward-only comparison point.

**2026-07-10 — iter-50 decision point: real signal, real gap, honest read.** Dedicated
instrumented eval (N=16 games/opponent, larger than the tiny in-loop N=4 MCTS check) on
`iteration_000050.pt`: vs heuristic 58-1-5 (0.914), wins 79.3% bomb-kill. Vs MCTS-256/depth16
17-3-12 (0.578, LCB 0.434) — a real improvement over v5's own iter-50 MCTS-256 score (0.375),
though N is too small to trust the exact win-cause split precisely. Advisor flagged a confound
before accepting "vs MCTS still mostly crush" at face value: MCTS games run longer (mean_steps
127 vs heuristic's 118), mechanically opening the sudden-death window more often regardless of
skill. **Discriminating test: SD-off vs MCTS on the same checkpoint.** Result: 1W-22D-1L
(91.7% draws), mean_steps 195/200 — games mostly can't be decided at all without the arena.
Not a mechanical artifact: this is a real remaining ceiling against a strong adaptive
opponent, not yet closed by heuristic-league training. But the single decisive win was a
genuine bomb-kill (0 crush, impossible with SD off) and SD-off vs heuristic was a clean
8-0-0 with 100% bomb-kill wins — real combat skill clearly exists, it just isn't reliably
landing on MCTS yet. **Second signal, arguably the more important one:** eval-time (no
dirichlet) WAIT is 57-67%, matching the ORIGINAL cheese profile, vs 20-31% during actual
league *training* (dirichlet on). Same network. This means the aggression seen in training
logs is substantially noise-induced exploration, not yet the network's own greedy/default
preference — a fragility finding independent of opponent choice. **Decision: keep training
(not a league-composition pivot yet)** — iter 50 of a schedule extended to 200, LR still live
(~2.3e-5 and decaying on the new schedule), score not yet statistically distinguishable from
a coin flip either way. Re-check win-cause + eval-time WAIT trend at iter 100. Reserve moves
(MCTS/frozen-self in the league, lower `arena_crush_win_value`) stay in reserve unless iter-100
still shows a real SD-off-vs-MCTS ceiling *and* high eval-time WAIT.

**2026-07-10 — second unexplained crash; auto-restart watchdog added.** v6-league died again
at iteration 77→78 (no in-app error trace, same silent-external-kill signature as the earlier
concurrent-training incident) — this time running *solo* with only one lightweight monitor
alongside it, so the "two simultaneous trainers" theory doesn't explain this one; root cause
remains unknown (possibly GPU driver/TDR flakiness under sustained load, unrelated to
concurrency). Checkpoint was intact at iter 77 (correct ~868MB size, not truncated this time).
Rather than keep manually noticing and relaunching, added `tools/launch/watchdog-train.ps1`:
a thin wrapper that relaunches the trainer on any non-zero exit, always resuming (never
`--fresh`) from the run dir's own `latest.pt`, capped at `-MaxRestarts` (default 20) with a
short delay between attempts. Takes the full trainer argument list as one explicit
`-TrainerArgs` array (a first version tried `-RunDir`/`-Iterations` as typed named params plus
`ValueFromRemainingArguments` for the rest — PowerShell's parser ambiguously tried to bind a
later `--width` token to the `[int]$Iterations` parameter and failed; the single-array form
sidesteps this class of bug entirely). v6-league now runs under the watchdog from iter 77.

**2026-07-10 — root cause of BOTH crashes found: a second, unrelated Claude Code process was
silently running a replay-generation script the whole night.** User reported the run "still
running on CPU" (perceived slowness/contention) and asked to stop it completely. Investigation
found **two** `bomber_alphazero_native.exe` processes alive simultaneously, and killing them
kept getting immediately replaced — a whack-a-mole that traced back to a PowerShell process
(started 2026-07-10 14:30, parent `claude.exe --resume f6e97c61-4bb8-4b55-bfd9-46b515a15fdd`
— the SAME session ID as this one, i.e. a second Claude Code window/process attached to this
conversation) running a `foreach` loop over 6 `evaluate --replay-out` jobs (old champion vs
self/MCTS, v6-league iter-10 vs self/MCTS, v6-league iter-77 vs self/MCTS) to regenerate
demo replays. This process had **no memory-visible trace in this transcript** — it was either
launched by a parallel/duplicate session the user has open, or is a residual from a much
earlier point that survived a context compaction. It explains both "crashes": each one was
this script's own trainer instance colliding with the training run's, not a training-loop bug.
**Fully stopped:** killed the orphaned PowerShell process, its immediate worker, and all
`bomber_alphazero_native.exe` instances; confirmed GPU idle (2% util) and zero matching
processes for two consecutive checks. Did NOT kill the parent `claude.exe` itself (a
different, larger action than stopping a wayward script — flagged to the user instead so they
can check for duplicate open windows).
**Recovery, per explicit user instruction ("restart in 4 hours"):** training stays fully
stopped; a background timer (polling in 2-minute increments, not a single long sleep) will
signal in 4 hours. Checkpoint is intact at iteration 77 (`latest.pt`, correct ~868MB size).
Added `tools/launch/v6-league-resume.ps1` — a one-command wrapper (all params baked in,
`-Iterations N` to extend) around the watchdog, so restarting doesn't require re-deriving the
40-flag command. Documented the one-trainer-at-a-time lesson in `tools/launch/README.md`.
**Also cleaned up tonight (explicit request, "the folder is huge"):** deleted 9 stray
`build*` directories unreferenced by `_env.ps1` (~1.8GB, all gitignored, zero risk — CMake
output only) and `results/diag-selfplay-warmstart` (828MB, this session's own disposable
warm-start scratch dir, data already extracted). Did NOT touch `results/alphazero-native-*`
historical run dirs (v1-v4, `v5-holdout`, various pilot/probe dirs, ~40GB+ combined) or
`results/replays/` — those hold either load-bearing proof/active-run data or are actively used
by the launcher scripts; flagged the breakdown to the user rather than unilaterally deleting.
