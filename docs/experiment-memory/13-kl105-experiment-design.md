# KL-105 experiment design: tactical gates + first bounded intervention (capped cause-balanced replay)

**Status: DESIGN — criteria below are DRAFT until frozen at the launch gate (checkpoint 4).**
Written by the planner (Fable, orchestrator pattern) as Phase 2a of the post-audit execution
plan (`docs/experiment-memory/12-post-audit-execution.md`). The advisor tool has been
unavailable across two sessions; the user resolved the plan's checkpoint-3 hard stop by
invoking the orchestrator pattern with Fable as the session/planner model — planner review
here *is* the design review, and launch-gate sign-off (checkpoint 4) is the planner's
documented review per the user's standing decision ("advisor sign-off suffices" for bounded
training launches).

## 1. Hypotheses (from KL-107's diagnostic, restated for this experiment)

- **H1 (tested here): data/curriculum.** The policy head's raw argmax is WAIT on 63-65% of
  steps because replay essentially never shows aggression being reinforced: bomb-kill-decisive
  games are ~3% of MCTS-eval games, and while league-vs-heuristic collection games produce more
  bomb kills, their winning-side samples are a small, unweighted sliver of a 200k buffer.
  Up-weighting exactly those samples changes *what the head sees reinforced* without touching
  reward magnitudes, environment timing, league composition, or search.
- **H2 (not tested here): value/objective structure.** One Phase-1c data point (raw value head
  lagging search's danger detection) keeps this live; untouched by this experiment.
- **H3 (not tested here): opponent-model mismatch.** `opponent_modeled_as` = "self" on 100% of
  traced rows; real and total, causal weight unknown; untouched by this experiment.

A flat result here is a first-class outcome: it kills the cheapest lever and directs the next
design pass at league diversification (KL-105 ordered-scope item 3's other half) or H2/H3.

## 2. Deterministic tactical gates (Phase 2b — built BEFORE the intervention, used as its probe)

### Purpose
Pre-registered, deterministic, seconds-cheap behavioral probes that (a) baseline the current
checkpoints' tactical competence, (b) serve as the pre/post probe for the intervention arms,
and (c) become the standing entry bar before any future long run (the last audit's directive).

### Mechanism
New `gates` subcommand in `bomber_alphazero_native` (src/training/native/trainer.cpp):
- Loads a checkpoint exactly like `evaluate` (manifest semantics inheritance;
  `--legacy-accept-unverified-semantics` supported; acquires the system ProcessLock, no
  run-dir lock — read-only).
- Runs each scenario twice: **search mode** (BatchedMcts, noise-off, greedy, default 96 sims —
  the deployed decision procedure; this is the mode pass/fail is scored on) and **raw mode**
  (unmasked policy-head argmax each step, no search — diagnostic only, recorded but never the
  pass/fail).
- Also runnable with a scripted agent instead of the network (`--gates-agent
  heuristic|mcts|...`) to prove each gate is achievable — a gate no strong scripted agent can
  pass is a broken gate, not a hard gate.
- Writes a KL-101-style write-once JSON evidence file (`--output`): provenance header
  (checkpoint/executable SHA-256, git commit, argv, resolved semantics) + per-scenario
  `{name, mode, passed, steps_used, terminal_cause, notes}`.

### Scenario construction
`BomberEnv` is an open struct (src/env/env.h:18-33): `env_init(&env, &base)` +
`env_reset(&env, fixed_seed)` with `crate_density=0`, then overwrite `env.state` fields
directly (tiles / agents / bombs / flame_ttl / step). `DangerMap` has an explicit
`danger_compute(dm, state)` (src/env/bomber_danger.h:17) — call it after construction; the
executor must verify whether `env_step_joint` recomputes danger internally each step (expected)
and add a construction-validity assertion (e.g. `env_observe` on the constructed position
matches the intended layout). Determinism: fixed seed, greedy selection, no dirichlet —
identical runs must produce identical JSON (asserted in CI by running twice and diffing).

Opponent modes per scenario: `NONE` (opponent fed ACTION_WAIT every step), `CONSTANT(action)`
(fed a fixed action every step), `AGENT(type)` (real `agent_act` via
env_observe/env_get_debug_snapshot, as the eval loop does at trainer.cpp:2287-2293).

### The six scenarios (executor drafts exact layouts as ASCII diagrams in code comments;
planner reviews the diagrams and predicates at evaluation)

| # | name | setup sketch | opponent | pass (search mode, within K steps) |
|---|------|--------------|----------|-------------------------------------|
| 1 | bomb-and-escape | learner beside a 3-crate cluster, open floor behind | NONE, far corner | ≥1 crate destroyed AND learner alive; K=20 |
| 2 | corridor-clear | learner inside a dead-end corridor sealed by one crate | NONE, far corner | learner alive outside the corridor region; K=30 |
| 3 | trap | opponent in a dead-end, learner at the mouth, bomb_ammo≥1 | NONE (static victim) | opponent dead with `death_owner == learner_seat`; K=20 |
| 4 | chase | opponent in open quadrant, learner across the board | AGENT(evasive) | learner within Manhattan distance ≤2 of opponent at any step; K=30 |
| 5 | flame-timing | opponent-owned bomb (timer=2) in the only corridor forward | NONE | learner alive AND past the blast tile after flame expires; K=15 |
| 6 | stall-break | learner and opponent facing across a single chokepoint tile (the mutual-rejection pattern from the streak finding) | CONSTANT(toward chokepoint) | learner reaches the far-side target tile OR destroys a crate opening an alternate path; fail if combined-idle every step; K=25 |

### Gate tests (CI, tiny fixture checkpoint — same pattern as the trace-raw fixture chain in tests/CMakeLists.txt:208-230)
- Runs `gates` twice on the fixture checkpoint (small sims, e.g. 8) → identical JSON both times
  (determinism), well-formed provenance header, all 6 scenarios present × both modes.
- Achievability: runs `--gates-agent heuristic` and asserts the heuristic passes the scenario
  subset the executor empirically finds it reliably passes (pinned explicitly in the test with
  a comment naming which and why — at minimum scenarios 1 and 2 are expected; if the heuristic
  can't pass a scenario, the scenario spec gets flagged to the planner, not silently weakened).
- Fixture pass/fail values for the *network* are NOT asserted (a tiny random-ish fixture net
  may fail everything; that's fine and expected).

## 3. First bounded intervention: capped cause-balanced replay sampling (Phase 3)

### Treatment definition (ONE variable)
Tag every replay sample at collection with the finished game's outcome cause and whether the
sample's seat was the bomb-kill winning side; then bias training batches toward bomb-kill
winning-side samples, with hard caps. Generation (league mix, reward values, schedule, search)
untouched. Control arm: identical binary and flags with the cap set to 0 (tagging still
happens — tags are inert bookkeeping; only the *sampler bias* is the treatment).

### Sample tagging
`struct Sample` (trainer.cpp:302) gains two `uint8_t` fields:
- `outcome_cause`: {0=unknown/legacy, 1=bomb, 2=selfkill, 3=arena_crush, 4=mutual_death,
  5=timeout_draw} — the game-level cause, classified with the *same death_owner logic already
  used* at the two finalization sites (mirror: trainer.cpp:1788-1822, per-sample seat is
  `sample_index % 2`; league: trainer.cpp:1929-1958, whole trajectory is the learner seat).
- `bomb_win_side`: 1 iff the game was bomb-decisive AND this sample's seat is the winner seat.
Mirror games: winner-seat samples of a bomb game get `bomb_win_side=1`; loser-seat samples get
the cause tag but `bomb_win_side=0`. League games: `bomb_win_side=1` iff the learner won by
bomb (`game_outcome > 0 && died_owner == learner_seat`).

### Persistence
`ReplayBuffer::tensors()/load()` (trainer.cpp:356-395) gain a fourth tensor (uint8, shape
[count, 2]) saved under a new archive key. Loading a checkpoint whose archive lacks the key
(every pre-change checkpoint, incl. the arms' base) defaults both fields to 0 — legacy samples
land in the unweighted pool and age out naturally with replay turnover (~10 iterations per
full turnover at current new-samples rates; by mid-arm most of the buffer is tagged). Executor
must confirm the archive read is key-based (extra keys ignorable by old binaries) and add a
round-trip test: save with tags → load → tags identical; load a pre-tag checkpoint → all zeros,
no error.

### Sampler
`ReplayBuffer::batch()` (trainer.cpp:331-354), when `cap > 0`:
- One linear pass builds the pool-A index list (`bomb_win_side == 1`). (200k uint8 scan per
  batch ≈ sub-millisecond; optimization phase is 7.6s/iteration total, so this is noise —
  measured, not assumed, in the executor's verification.)
- Batch composition: `f = min(cap, boost_max * n_A / N)` with `boost_max = 16` (compile-time
  constant), `cap` from the CLI flag. `round(f * batch)` samples drawn uniformly WITH
  replacement from pool A; the remainder drawn uniformly from the whole buffer (not "pool B" —
  keeps the remainder distribution simple and means cap=small still mostly resembles uniform).
- Why both caps: `cap` bounds the batch share (default 0.25); `boost_max` bounds the effective
  oversampling factor so a tiny pool A can't be repeated pathologically — max expected
  repeats per pool-A sample per batch = `f*batch/n_A ≤ boost_max*batch/N ≈ 16*1024/200000 ≈
  0.08`, i.e. bounded regardless of pool size.
- Unit test with synthetic pools: f formula honored at boundary cases (n_A=0 → pure uniform;
  huge n_A → f=cap; tiny n_A → f=boost_max*n_A/N), determinism given a seeded rng.

### Semantics / provenance (KL-101 discipline — this is where past sessions got burned)
- New CLI flag `--replay-cause-balance-cap FLOAT` (default 0 = off). It is a **semantic field**:
  added to the semantic manifest (persist + inherit-on-load + explicit-override = logged
  semantic fork), to `runtime_config_signature()` (trainer.cpp:1022 region / :1097 region), to
  the semantic-fork detection diff, to `--help`, and to the strict CLI validation list
  (trainer.cpp:1266-1283).
- Metrics observability: per-iteration `replay_cause_pools` (counts per cause + pool-A size)
  and `realized_pool_a_batch_fraction` (mean over the iteration's batches) added to
  metrics.jsonl (additive schema change).

### Arms (both forked via `--fork-from`, per KL-101 provenance)
- **Base**: `control03-from102/iteration_000130.pt` (SHA-256
  `ab5771d34f3be18e0911b1da51690fa62dc75f5ec6d0a06dba082cdc069cb288`). Chosen over crush01-130
  because it is manifest-verified (a legacy checkpoint hard-fails TRAIN forks without an
  explicit horizon whose provenance for crush01 is genuinely murky — its lineage contains the
  re-derived-horizon bug). Its floor-LR history is shared context for both arms, not a
  confound.
- **Schedule**: both arms fork with explicit `--lr-schedule-updates 33280` (= 260 iterations ×
  128 train_steps). At the fork point (global_updates = 16640 = exactly half the horizon) the
  cosine gives LR ≈ 1.05e-4 annealing to ≈ 6e-5 over the arm — deliberately mirroring the LR
  range crush01 actually trained under (1.005e-4 → 6.19e-5 across iters 103-130). This is an
  explicit, logged semantic fork (the sanctioned override path), identical in both arms.
  Rationale: leaving both arms at the inherited 1e-5 floor risks a mute, uninformative flat
  result because *nothing* can move at floor LR — that would test the lever at a learning rate
  chosen by an old bug, not a fair test.
- **Length**: 24 iterations each (130 → 154). Measured throughput (control03 metrics.jsonl,
  last 10 iters): mean 481s/iter incl. eval spikes → ~3.3h/arm, ~6.5-7h sequential.
- **Identical in both arms**: base checkpoint, binary (hash recorded), `--seed`,
  `--lr-schedule-updates 33280`, `--iterations 154`, league fraction, reward values, eval
  cadence, everything except `--replay-cause-balance-cap` (0.25 treatment / 0 control).
- **Post-launch verification (KL-100 item 7, before any interpretation)**: resolved
  `learning_rate` at matched iterations from both arms' metrics.jsonl, manifest diff showing
  exactly one differing semantic field, same executable SHA-256 in both run dirs.

### Evaluation (paired, both arms at iteration 154)
1. Tactical gates (search mode), vs. each arm AND vs. the base checkpoint's pre-launch
   baseline.
2. Wait/idle diagnostic: `evaluate --eval-mcts --trace-output` on seed block 1300001, 16 games
   (32 matches), full `analyze_wait_diagnostic.py` table (raw-argmax-WAIT, chosen-WAIT,
   combined idle, streaks).
3. SD-on vs MCTS-256, N=32 matches/arm, win-cause classified, both seats.
4. SD-off control, same N — the collapse check.
Heuristic/random numbers recorded as training-health only. Diagnostic block 1300001
throughout; burned holdout blocks untouched; no "superhuman"-adjacent language anywhere.

### DRAFT success/failure criteria (frozen verbatim or amended ONLY at the launch gate, then immutable)
Baselines: bomb-kill ≈ 1-2/64 games SD-on vs MCTS-256; chosen-WAIT 61.9% (control03-130);
combined idle 62.9%; SD-off ≈ 87.5% draws; gates baseline TBD pre-launch.
- **SUCCESS (H1 supported)** — all of:
  (a) treatment bomb-kill ≥ 8% of games (≥6/64) AND ≥ +4 games over control at 154;
  (b) treatment chosen-WAIT ≤ control − 8pp on the 1300001 diagnostic;
  (c) SD-off timeout-draw rate not more than +5pp vs control (no survival-collapse regression);
  (d) both-seat breakdown does not reverse the direction of (a) or (b).
- **DIRECTIONAL** — (a) or (b) met but not both, no (c) violation → one follow-up decision
  (extend arms OR adjust cap) allowed, once, with the same controls; otherwise treat as flat.
- **FLAT/NEGATIVE** — anything else → lever killed, retained as the negative result, next
  design pass goes to league diversification or H2/H3. No re-runs to "give it another chance."
Gate pass-rate movement is reported descriptively, not part of the success definition (six
scenarios is too few to gate on statistically).

## 4. Execution structure (orchestrator)

Executor units are **strictly serialized** (all touch trainer.cpp; and ctest/GPU work cannot
overlap the single-GPU process lock):
- **Unit A (Sonnet)**: gates subcommand + scenarios + CI tests + docs. No GPU beyond ctest.
- Planner review → commit → **gates baseline run** (planner-driven, GPU: control03-130,
  crush01-130 [legacy flags], lever2-160; minutes each).
- **Unit B (Sonnet)**: Sample tags + persistence + sampler + semantics/manifest + metrics +
  tests. No GPU beyond ctest.
- Planner review → commit → **Unit C (Sonnet)**: two arm launch scripts (fork + explicit
  schedule + cap flag), post-arm paired-eval script, README/docs. Tiny.
- Planner launch-gate review (freeze criteria, verify exact commands, record hashes) →
  sequential arm launches (planner-driven, background, watchdog) → paired eval → planner
  evaluator pass vs. frozen criteria → Linear/docs closeout.

### Standing constraints binding every unit
One native GPU process ever (ctest included — never run ctest while a training/eval process
is up). Commit before reconfigure+build before generating evidence (the Phase 1b lesson —
`git status --untracked-files=all` counts untracked files as dirty). Brick-per-commit.
No claim above evidence; KL-100 item 7 verbatim for the arm comparison. Evidence under
`docs/experiment-memory/evidence/`, raw traces local-only with hashes.

## 5. Risks
- **Pool A too small to matter** (mirror games rarely bomb-kill; league-vs-heuristic supplies
  most kills). Mitigation: metrics expose pool sizes per iteration from iteration 1; if pool A
  stays <0.2% of buffer through iteration ~140, that's visible early and reportable honestly
  rather than discovered post-hoc.
- **Oversampling artifacts** (policy overfits the up-weighted sliver): bounded by boost_max;
  entropy + policy_loss in metrics give an early tell; SD-off criterion (c) catches the
  survival regression this could cause.
- **Serialization compatibility**: new tag tensor must not break old-checkpoint loads (test
  covers) or silently drop tags on save/load round-trip (test covers).
- **Gates too easy/too hard**: achievability check vs heuristic catches "impossible"; the
  planner review of ASCII layouts catches "trivial."
- **Schedule override second-guessed later**: it is a deliberate semantic fork, identical in
  both arms, logged by the manifest machinery, and justified above — recorded here so the
  reasoning survives.
