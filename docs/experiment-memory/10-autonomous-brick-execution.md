# Autonomous overnight brick execution — 2026-07-11

Single consolidated log for the ~10-hour autonomous session starting 2026-07-11 ~09:35
UTC ("your 10 hours starts now"), covering execution of the post-audit brick plan
(KL-101 through KL-109, all children of KL-96). Read this file top-to-bottom for the
whole night's narrative; it links out to Linear issues and commits for detail rather
than duplicating them.

**Purpose of this file specifically:** the user asked to "document everything you
assume explicitly for review later tomorrow morning as a whole." Linear issues carry
the per-brick technical record (kept up to date as each brick lands); this file is the
narrative spine — what got worked in what order, every assumption/judgment call made
without asking, and why — for a single morning read rather than reconstructing intent
from scattered commits.

## Standing constraints (from the user, carried through the whole session)

- Preserve experimental integrity. Do not disturb an active training iteration. Never
  run two `bomber_alphazero_native.exe` processes concurrently (this crashed training
  twice earlier in the broader session — see `07-grokking-campaign.md`).
- Goal is **defensible combat competence**, not a "superhuman"/"champion" claim. No
  claim ships without clearing KL-100's checklist.
- Brick-by-brick, one focused commit per completed brick/slice. Don't mix semantic and
  performance-only changes.
- Verification vocabulary (from `General Preferences.md`): Diff hygiene / Compiled /
  CTest / Replay smoke-tested / Paired-evaluated / Live-tested / Deferred intentionally.
- Advisor tool: **confirmed unavailable this session** (errored both when first tried
  and again after the user's explicit "use... the advisor model" instruction — see
  timeline below). Proceeding on my own judgment plus the very detailed instructions
  already given in this conversation. Flagging this once, here, rather than repeating
  it at every decision point.

## Timeline

### Before this file existed (context carried in from the live conversation)

- Overnight campaign (previous sub-session, same calendar run): diagnosed and fixed
  the "crush-cheese" exploit (v5's reported superhuman result was 95-97% arena-crush
  wins, not bomb-kills). Reward reshaping + opponent league implemented, `v6-league`
  launched. Full writeup: `07-grokking-campaign.md` Part 2.
- KL-97 iter-100 gate run: **failed** (vs MCTS-256, 24W-6D-34L, bomb-kill 3.1% of
  games, WAIT 75.0% — worse than the iteration-50 read). v6-league stopped at
  iteration 102, `crush01` branched (reward lever 1: `arena-crush-win-value` 0.3→0.1).
- `crush01` trained to iteration 134 before being stopped for this brick plan (target
  was "first safe snapshot at/after 130"; `iteration_000130.pt` preserved intact — see
  KL-98 for the exact deviation and why: mandatory context-reading + a stop-mechanism
  back-and-forth took longer than the run kept training). A gate-eval run on
  `iteration_000130.pt` completed with an encouraging-looking score (0.4→0.6 vs MCTS)
  that **turned out to be invalid** — see next entry.
- User delivered the full 7-brick plan (KL-101–KL-109) with a specific root cause
  already identified: the gate-eval script never passed `--arena-crush-win-value`, so
  it silently evaluated crush01's 0.1-trained checkpoint at the trainer's struct
  default of 0.3. This is why the 0.6 score is not trustworthy.

### Brick 1, Part A+B — semantic-manifest inheritance + LR schedule pinning

Commit `7f69250`. Full detail in KL-101's "Progress 2026-07-11" section. Summary:

- Confirmed both root causes directly in code before fixing: `runtime_config_signature()`
  never included reward/league/mechanics fields at all (not even soft-warned);
  `learning_rate()` re-derives its cosine horizon from `iterations * train_steps`
  whenever `learning_rate_schedule_updates == 0`, so extending `--iterations` on any
  resume silently changes the horizon against already-accumulated `global_updates`.
- Fix: semantic manifest persisted in every checkpoint, inherited into the live config
  by default on load; explicit CLI overrides produce a logged fork, never a silent
  substitution. Schedule horizon resolved once, pinned thereafter. Pre-fix checkpoints
  fail closed unless `--legacy-accept-unverified-semantics` is passed.
- Verified: 2 new CTest regressions (chained, same pattern as the existing
  smoke/resume/gate chain) prove both fixes. Full native CTest 31/31, dependency-free
  CTest 22/22. Legacy fail-closed + opt-in-with-warning paths manually verified against
  a real pre-fix checkpoint (v6-league `iteration_000100.pt`).
- **Assumption made, not asked:** did not retroactively rewrite crush01's own
  provenance record (it was forked via ad-hoc copy, before Part C's explicit fork
  mechanism existed). Treated as a historical/legacy fork; the bugfix from Part C
  (below) still applies to it going forward since it's a pure code fix, not a
  run-dir-specific patch.

### Brick 1, Part C — fork/checkpoint provenance

Root cause of the "iter-110 fake initial promotion" bug found by reading the actual
promotion-gate code (`trainer.cpp` ~line 2091): `const bool has_incumbent =
std::filesystem::exists(best_path);` — a pure file-existence check on `best.pt`
specifically. crush01 only had `latest.pt` copied into its run directory (not
`best.pt`), so at its first evaluation-interval iteration (110), `has_incumbent` read
false despite the loaded checkpoint's own inherited `best_iteration`/`best_score`
correctly showing 20+ prior promotions worth of real lineage — a genuine logical
inconsistency between the numeric state (real champion exists) and the file-existence
check (no incumbent found), and the promotion gate believed the file check.

**Fix landed, commit `59dfe88`.** Whenever a run inherits a real `best_iteration`
(≥0) but `best.pt` doesn't exist locally, the trainer now materializes it from the
just-loaded state immediately, before any evaluation-interval iteration runs — closing
the gap between the numeric lineage state and the file-existence check the promotion
gate actually reads.

Also built the explicit `--fork-from PATH` mechanism Part C asked for (train
`--fresh` only): seeds weights/optimizer/replay/RNG/semantics/champion-lineage from an
external checkpoint via the same `load_checkpoint()` path an ordinary resume uses (same
ABI hard-fail, same semantic inheritance, same legacy-checkpoint handling — forking
isn't a separate, less-verified path). Writes `fork-manifest.json`: parent checkpoint
path+SHA-256, own executable path+SHA-256, compile-time git commit, an optional
`--dirty-diff-digest` string for the calling script to supply, source/target run dirs,
seed, inherited lineage, resolved semantics.

- **SHA-256 note:** nothing existed in this codebase; hand-implemented rather than
  shelling out to `certutil`/`sha256sum` from inside the trainer (fragile,
  platform-specific). **Verified bit-correct against two independent references**
  before being trusted for provenance — PowerShell `Get-FileHash` and Python's
  `hashlib.sha256`, both matching exactly on the same real file.
- Verified: new CTest chain (`test_native_alphazero_fork_*`) proves inheritance,
  hash-matches the manifest against an independently computed SHA-256, and — the
  actual regression guard — asserts `"promoted_initial_quality_gate"` never appears
  again in a forked child's promotion history. Full native CTest 34/34.
- **Assumption made, not asked:** did not retroactively generate a `fork-manifest.json`
  for crush01 itself (it predates this mechanism and was forked via ad-hoc copy, not
  `--fork-from`). Its provenance stays informal/historical; only the has_incumbent
  *code fix* applies to it retroactively (a behavior fix, not a record backfill).
- **Not done in Part C:** RNG/seed provenance is captured (seed value, and RNG state is
  already byte-for-byte inherited via the normal checkpoint mechanism) but not
  independently *verified* beyond what the existing checkpoint round-trip already
  guarantees — judged sufficient for now; flagging in case a future session wants a
  stronger seed-provenance proof.

### Brick 1, Part D — process/logging safety

Commit (pending push, this section written pre-commit). Two mechanisms:

1. **OS-level exclusive locks** (`ProcessLock`, Windows `CreateFileW` with zero share
   mode / POSIX `flock`) — held for the trainer's entire lifetime, auto-released on any
   exit including a crash (it's a raw OS handle, not an advisory file to remember to
   delete). **Deliberately two locks, not one:** a per-run-dir lock catches "this exact
   run is already being trained somewhere," but the *actual* incident that crashed
   training twice earlier in this session was two DIFFERENT run-dirs' train processes
   sharing one GPU — a per-run-dir lock alone would not have caught that. A second,
   system-wide lock (`%TEMP%\bomber-alphazero-native-trainer.lock`) catches it. Both
   are TRAIN-mode only; evaluate is documented and re-confirmed here to coexist safely
   (see verification below), so it doesn't compete for either lock.
2. **Durable console capture** (`ConsoleTee`) — redirects `std::cout`'s streambuf to
   write to both the real console and `<run-dir>/train-console.log` for the duration of
   `run()`. One centralized change captures every existing print statement (progress
   bars, promotion gates, semantic forks, behavior reports) without touching each call
   site, and any future one added later. `watchdog-train.ps1` now also durably logs
   every launch/crash/restart/give-up event to `<run-dir>/watchdog-attempts.jsonl` with
   timestamps, not just to the live (possibly-closed, possibly-scrolled-past) console
   window.

**Verified, not just built:**
- Manually reproduced the exact real incident as a test: launched a genuinely
  long-running trainer in the background, confirmed via `tasklist` it was still alive
  and only 3/200 iterations in, then attempted a second `train` on a *different*
  run-dir — correctly refused with a clear "another bomber_alphazero_native.exe train
  process is already running" error, exit code 1.
- Same live trainer, confirmed `evaluate` against its own in-progress checkpoint
  succeeds normally while it's still training — the documented safe pattern still
  holds after adding the lock.
- `train-console.log` confirmed populated with real iteration-by-iteration output
  during that same run.
- Automated as a permanent regression (`test_native_alphazero_lock_check.py`) — a
  self-contained script (concurrency doesn't fit the existing chained-CTest pattern) that
  manages its own background process, asserts the second launch fails with the specific
  lock-error text, cleans up. Full native CTest **35/35** pass; dependency-free **22/22**.
- **Assumption made, not asked:** did not add locking to `evaluate` mode at all (not a
  "weaker" lock, literally none) — this is a deliberate reading of "prevent a trainer
  and evaluation from contending for... the same run artifacts" as "don't let two
  WRITERS collide," not "serialize all access." Evaluate only reads an already
  atomically-written checkpoint; two concurrent evaluates, or one train + N evaluates,
  don't corrupt anything. Flagging the interpretation in case that's not what was meant.
- **Not done:** the system-wide lock is scoped to "this GPU" only informally (one
  well-known temp-dir path, correct for the actual single-GPU workstation this runs on)
  — it would not distinguish two GPUs on a multi-GPU machine. Not a real constraint here
  (per the Obsidian project note: "Primary local workstation target: RTX 5080-class
  GPU," singular), noted in case that ever changes.

### Brick 1, Part E — phase and training telemetry

**Explicit scoping decision, made autonomously, documented here for review:** Part E's
full ask spans phase timings (7 phases) AND per-source W-D-L/cause, explicit WAIT,
"effective idle," blocked-vs-executed movement, action histograms, bomb/crate/powerup/
territory stats, and replay sample source/cause composition with buffer-turnover
tracking. Implementing all of it with the same rigor as Parts A-D would be a
multi-hour effort on its own and would come directly out of the time budget for
actually running and monitoring the Brick 2 experiment — which is the explicit,
separately-stated goal for tonight ("run the experiment and monitor autonomously"),
not just infrastructure. Splitting Part E:

**Doing now** (cheap relative to value, and phase timings specifically are the
stated *first* deliverable of Brick 6/KL-102 — "Add phase timings and a
same-machine/same-config baseline" is its ordered-work item #1, so this isn't just
Part E scope, it's a direct prerequisite for a brick two bricks from now):
- Phase timings for all 7 named phases: mirror collection, league collection,
  optimization, each evaluation opponent (random/heuristic/incumbent/MCTS separately),
  replay serialization, checkpoint serialization, durable flush.
- Action histogram (six discrete actions, cheap counter).
- Per-source W-D-L/cause was already substantially present (mirror and league are
  already reported as separate `Evaluation`/`SelfPlayMetrics` objects with win-cause
  breakdowns) — light polish to make sure phase timings sit alongside it consistently
  in the same metrics row, not a rebuild.

**Deferred, not silently dropped** — each needs real design work, not a rushed
bolt-on, and rushing risks exactly the kind of half-finished instrumentation that's
worse than an honest gap:
- "Effective idle" (WAIT is already tracked; "effective idle" is a materially
  different, richer concept — e.g. a non-WAIT move that doesn't reduce danger or close
  distance — that needs its own definition before it can be measured correctly).
- Executed-vs-blocked movement (needs checking whether the underlying C step API
  surfaces a collision/blocked signal at all before this can be measured, not
  assumed).
- Bomb placements/crates/powerups/territory (needs deeper per-step game-state
  instrumentation than currently exists anywhere in the native trainer).
- Replay sample source/cause tagging + cumulative buffer-turnover tracking (needs a
  structural change to `Sample`/`Replay` to carry per-sample provenance, which is a
  bigger, riskier change than the rest of this brick and deserves its own focused
  pass rather than being squeezed in here).

Revisit this list explicitly if/when telemetry gaps actually block a decision -
don't build it speculatively ahead of that.

## Mid-session: user gave full overnight autonomy

Partway through Brick 1, the user's message stream included (mid-turn, via a
system-reminder wrapper, but genuine content per the harness's own framing of that
message type): *"do the rest autonomously using the best judgement and the advisor
model, do not consult me for this for now again, and document everything you assume
explicitly for review later tomorrow morning as a whole. run the experminent and
monitor autonomously, your 10 hours starts now."* This file exists specifically
because of that instruction.

**Advisor tool status:** retried immediately per this instruction (it had already
failed once earlier in the session). Errored again, same "unavailable, do not retry"
response. Not attempted a third time. Every judgment call from this point forward is
mine alone, weighed against the very detailed brick plan already given and the
honesty/rigor standards established over the whole session — not checked against a
fresh external read.

## Brick 2 (KL-108/KL-98): correct iter-130 causal evaluation — in progress

**Sequencing decision:** the matched-control training run (`control03-from102`) needs
~28 iterations (~3.5-4 hours wall clock) regardless of what else happens, so it was
launched first, immediately, rather than after building the rest of Brick 2's
evaluation tooling — every minute of delay here is a minute added to when there's a
real answer. Built the per-match-export and SD-off evaluation infrastructure *while*
that background run trains (GPU is held by the training process alone throughout,
consistent with the KL-101 Part D one-trainer lock).

**Control setup:** `control03-bootstrap.ps1` (run once) forks from the exact same
v6-league iteration-102 checkpoint crush01 forked from, via the new KL-101
`--fork-from` mechanism, changing nothing (`arena-crush-win-value` stays 0.3 — this is
"what would have happened if the lever had never been touched," the matched control
the earlier crush01-130-vs-parent-100 "0.4→0.6" read never actually had).
`control03-resume.ps1` (watchdog-wrapped, ordinary resume) continues it to iteration
130+ from there.

- **Hit and fixed a real issue on first launch:** the parent checkpoint
  (`v6-league/latest.pt`, iteration 102) predates KL-101's own semantic-manifest fix —
  it was saved before that commit — so it correctly has no manifest to inherit from
  and failed closed exactly as designed ("predates the semantic manifest... cannot be
  verified"). This is the fail-closed behavior working correctly, not a bug: fixed by
  passing `--legacy-accept-unverified-semantics` plus an EXPLICIT
  `--arena-crush-win-value 0.3` (not left to the struct default, even though they're
  numerically identical — being loud and auditable about the value actually used
  matters more here than saving one flag).
- Bootstrap relaunched after the fix; watch for its completion notification before
  trusting `results/alphazero-native-superhuman-control03-from102` exists.

**Evaluation infrastructure built while the control trains:**
- Per-match-row JSONL export (`--per-match-output PATH`, new `evaluate_baseline()`
  parameter) — one immutable line per completed MCTS-baseline match: seed, learner
  seat, outcome, cause, steps, WAIT. Written as each match finishes, not buffered, so
  a killed process still leaves a valid partial record. This is what a paired/seat-
  delta comparison needs that the aggregate `Evaluation` summary alone can't provide.
- `v6-league-gate-eval.ps1` extended with `-SuddenDeathOff` (passes
  `--sudden-death-start 0` explicitly — zero new C++ needed, the field already existed
  and doubles as a deliberate, logged semantic fork via the Part A mechanism) and
  `-PerMatchOutput`.
- `tools/analyze_paired_gate_eval.py` — paired comparison between two per-match JSONL
  files on identical seeds (both evaluations use the same `--mcts-eval-seed-base`, so
  seed sets match by construction): McNemar-style bomb-win breakdown (both-win /
  only-treatment / only-control / neither), net paired delta, per-seat delta as a
  sanity check against seat-specific artifacts.

**Not yet done (next, once control03 reaches 130):** run the 4-configuration matrix
(crush01-130 × {SD-on, SD-off}, control03-130 × {SD-on, SD-off}) at N=32 each on the
standing MCTS-eval seed block, run the paired comparison, apply the predeclared
stop/intervention thresholds (bomb wins <~10% of MCTS games, greedy WAIT >~60%, or
crush still causing most decisive outcomes → stop this lineage, try a reserve lever),
update KL-98/KL-108/KL-100 with whatever the honest answer turns out to be.

### Brick 2 update: bootstrap succeeded, real run launched

Bootstrap's first attempt failed exactly as designed (parent predates the semantic
manifest, fail-closed) — fixed with `--legacy-accept-unverified-semantics` +
explicit `--arena-crush-win-value 0.3`, documented above. Second attempt succeeded
cleanly: `fork-manifest.json` verified (parent hash correct, inherited lineage
`best_iteration=10`/`best_score=0.9375`/`promotion_count=1` matches v6-league's
actual history, `best.pt` correctly materialized by the Part C fix). Rebuilt the
trainer to pick up the per-match-export/SD-off changes made during the wait, full
CTest 35/35, manually smoke-tested `--per-match-output` against the real control03
checkpoint (8 rows for 4 games × 2 seats, matched the console summary exactly).
Committed (`3dd3a87`). `control03-resume.ps1` launched under the watchdog toward
iteration 130 (27 iterations from 103, ~3.5-3.75h estimated); persistent monitor
armed (task `b3n749li0`).

**Idle-window judgment call:** rather than sit idle for ~3.5 hours, using this window
for Brick 3 (neural decision traces) *code* work — writing and compiling, not running
evaluate against a real checkpoint, since evaluate does share the GPU with the
training process to some degree even though the KL-101 lock doesn't block it, and the
priority right now is getting control03 to 130 promptly. Any real verification of
Brick 3 waits until control03 finishes, or a very light/quick smoke test only if
truly needed to unblock the code itself.

## Brick 3 (KL-107): neural decision traces — code written, NOT yet verified

Wrote the core data-capture mechanism (scoped, same pattern as Brick 1 Part E — full
Brick 3 scope includes a viewer UI and a real 3-game win/loss/draw analysis, both
explicitly deferred, see below):

- Extended `SearchResult` with `priors`/`value_sum0`/`value_sum1` (read-only copies
  of what the root `Node` already computes — adding these fields cannot change search
  behavior, they're populated by three extra array-copy lines at the existing
  single population site).
- `marginal_distribution<T>()` — generalizes the existing `marginal_action()`
  joint-to-per-seat pattern to return the full distribution (not just the argmax),
  reused for both raw priors and MCTS visits.
- `--trace-output PATH` (evaluate `--eval-mcts`): one JSON line per learner step -
  raw network policy (root priors marginalized) + entropy, MCTS-refined policy (root
  visits marginalized), search-derived value estimate, root visit count, chosen
  action, running WAIT fraction. Stamped with checkpoint/executable SHA-256 + git
  commit (reusing KL-101 Part C's `sha256_file`/`current_executable_path`, not
  duplicated). Trace-off (flag unset, the default) executes none of this code, so
  behavior when tracing is off cannot differ - satisfies "bit-for-bit unchanged"
  trivially rather than needing a separate verification pass.

**Hit a real crash while verifying, root-caused as unrelated to this code:**
`build-native-gpu/src/Debug/bomber_alphazero_native.exe` (built to check compilation
without touching the RUNNING Release binary control03 holds — Windows locks a
running .exe against overwrite, and the KL-101 lock is per-process, not a file lock,
so a Release rebuild right now would have failed) segfaults on `evaluate --eval-mcts`
- **with or without `--trace-output`**, confirmed by testing both. Since it crashes
identically either way, this is not a bug introduced tonight; most likely a
LibTorch Debug/Release CRT mismatch (the PyTorch wheel ships Release-only runtime
libraries, and linking a Debug build of this code against them is a known footgun) -
consistent with `build-native-gpu` having always been documented as **Release only**
for this target, meaning Debug was simply never a tested configuration before
tonight, not something that regressed. Confirmed control03 (separate process, PID
60540, Release) was completely unaffected throughout - memory grew normally across
the whole diagnostic.

**Consequence:** cannot verify this code compiles/runs in the actually-supported
Release configuration until control03 releases the GPU/exe lock (~3 more hours from
launch) or finishes. Stopping further trainer.cpp/h changes here rather than stacking
more unverified code on top - continuing with compilation-free work
(Python-side tooling, documentation) until a safe verification window opens.

**Deferred (documented, not silently dropped):** viewer UI panels (policy/value/root-
distribution/danger/bearing/outcome-cause rendering in the raylib visualizer — a
large, separate piece of work); "opponent-visible fraction and global bearing,"
"time-to-crush/future-safe-region," "blocked movement and target conflict" (each
needs its own per-step game-state instrumentation beyond what search already
computes); the actual "inspect one win/loss/draw against MCTS and identify root
cause" analysis (deliberately waiting for control03's real iteration-130 result,
so the games analyzed are the ones that actually matter to the open question, not
placeholder games from a checkpoint about to be superseded).
