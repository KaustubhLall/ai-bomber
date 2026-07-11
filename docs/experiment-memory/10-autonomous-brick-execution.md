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

## Read this first — executive summary

**Fixed:** the actual experiment-integrity bugs KL-101 was written to fix, all
verified with real regression tests (35/35 native CTest, 22/22 dependency-free) —
checkpoints now carry a semantic manifest so reward/mechanics values can't silently
default instead of inheriting from the checkpoint; the LR schedule horizon no longer
silently re-derives on `--iterations` extension; a real "fake initial promotion" bug
is fixed and regression-guarded; two OS-level locks now make the exact GPU-contention
crash from earlier this session structurally impossible; an explicit `--fork-from`
mechanism with full provenance replaces ad-hoc checkpoint copying.

**Found (the actual research result):** ran the full matched-control causal
comparison KL-108 exists for. **Lever 1 (reward reshaping, `arena-crush-win-value`
0.3→0.1) failed** — a checkpoint trained with the lever and a matched checkpoint
trained without it are statistically indistinguishable on real combat skill (both
land a bomb-kill in exactly 3.1% of games against held-out MCTS-256; a
sudden-death-off control shows both collapse to 87-97% draws with the crush mechanic
removed). The apparent score improvement crush01 showed earlier was a crush-mechanic
artifact, not new combat skill — exactly the failure mode this whole harness exists
to catch, and it caught it on the *new* lever this time, not just the old v5 claim.
**New root-cause evidence:** first real use of the KL-107 neural-trace capture shows
the network's own raw policy (before any search) already agrees with the final
chosen action 74-93% of the time — passivity is coming from the trained policy
itself, not from search overriding an aggressive one. This points at self-play data
diversity / curriculum (KL-105) as a more promising lever than further reward or
timing tweaks.

**Running:** lever 2 (`sudden-death-start` 120→160), launched with explicitly
calibrated low confidence given the finding above — it doesn't touch the policy
training signal either, so it's plausibly going to fail the same way. Running it
anyway since it's cheap, next in the predeclared order, and a wrong prediction
logged honestly is better than a skipped step. Check `tasklist` /
`results/alphazero-native-superhuman-lever2-sdstart160/metrics.jsonl` for where it
landed.

**Recommended next, if lever 2 also fails (or if you'd rather skip straight there):**
lever 3 — add a *weak* MCTS or a frozen-self checkpoint to the self-play league
(currently heuristic-only) — needs a small new CLI flag (league opponent type),
deliberately not implemented tonight to avoid rushing new C++ under time pressure
late in the session. Or skip the remaining reserve levers and go straight to KL-105's
curriculum/self-play-diversity work, which the trace evidence now points at directly.

**Not done:** Bricks 5 (duel-v2 fidelity/ABI bump) and 6 (throughput) not started.
Brick 3 (traces) has its capture mechanism verified but no viewer UI. Brick 7 (docs)
is ~60% done. Full detail below, in chronological order.

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

Commit `d87dcf4`. Two mechanisms:

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

## Brick 7 (KL-104): substantial progress during the control03 wait

Continued using GPU-idle windows productively rather than sitting idle or piling
more unverified C++ on top of the already-blocked Brick 3 work:

- Champion launchers (`champion-eval.ps1`, `champion-replay.ps1`) relabeled as
  retracted — loud runtime warning + header comments + README table markers.
  Deliberately reworded rather than renamed (the user's own phrasing allowed
  either): renaming would break any existing desktop shortcuts pointing at these
  exact filenames, which can't be discovered or fixed from inside the repo. This is
  exactly the kind of "hard to reverse, affects something outside the local
  environment" action worth avoiding when an equally-compliant safer option exists.
- Obsidian project note (`Project Management/Projects/AI Bomber Search &
  Self-Play.md`) updated with all five standing rules the user asked for: one-trainer
  rule, semantic-fork provenance, LR-horizon rule, claim-evidence hierarchy (five
  ranked items, each tied to a specific thing that was actually exploited or nearly
  was, not abstract principles), fresh-holdout rule. Written as a durable "why this
  exists" preamble + checklist, not just a rule dump, per this vault's own writing
  conventions (General Preferences.md: "principle -> rule -> one-line rationale").
  Not committed via git - this file lives outside the ai-bomber repo entirely, in the
  Obsidian vault that syncs via Obsidian Git on its own, not something this session
  has a git remote for.
- KL-104 updated in Linear with the real state: 3 of 7 acceptance items done this
  session, 1 discovered already-done (KL-81/KL-109 reconciliation predates tonight),
  2 genuinely not started ("distinguish current vs archived truth in fidelity/
  campaign docs" and "repository/Linear/Obsidian links agree" - both broader,
  open-ended verification passes rather than a single bounded fix, deliberately not
  rushed).

Control03 still healthy at iteration 104/130 throughout this entire stretch (checked
without disrupting it - read-only tasklist/metrics.jsonl checks only). Settling back
into a lower-activity monitoring posture now rather than continuing to manufacture
more parallel work threads - the Monitor (task b3n749li0) will notify at the next
5-iteration milestone or on completion.

## Brick 2: the real eval matrix — results as they land

control03 finished training on its own (the trainer's loop naturally exited at
`iteration == config.iterations == 130`, exit code 0, watchdog saw a clean exit and
did not restart it — no force-stop needed at all, the cleanest possible outcome).
Rebuilt Release with the GPU finally free, full CTest 35/35, manually verified
`--trace-output` end-to-end against the real control03-130 checkpoint (464 correctly
structured rows across 2 real games, exit code 0) - confirming the earlier Debug-mode
segfault really was unrelated to this code, as diagnosed. Committed (`6a3be36`).

**Important correction caught before running the matrix:** crush01's checkpoints
(including `iteration_000130.pt`) were saved by the pre-KL-101 binary — it was
stopped before that fix even existed — so they have no semantic manifest and hit the
same fail-closed check the control03 bootstrap did. Extended
`v6-league-gate-eval.ps1` with `-LegacyArenaCrushWinValue` (forces
`--legacy-accept-unverified-semantics` + an explicit `--arena-crush-win-value`, not
left to a guess) rather than discovering this the hard way mid-matrix.

### Config 1/4 — crush01-130, SD-on, N=32 (64 games), correct 0.1 semantics

vs MCTS-256: **31W-17D-16L, score=0.6**, WAIT=64.1%. Win-cause: bomb-kill=2 (6.5% of
wins), arena-crush=29 (93.5%). Loss-cause: self-kill=2, arena-crush=14 (87.5% of
losses - MCTS is winning the same way). Draws: mutual-death=17.

Compare to the KL-97 iteration-100 baseline (24W-6D-34L, score=0.4, bomb-kill=2/64=
3.1%, WAIT=75.0%): score up (0.4→0.6), WAIT down a little (75.0%→64.1%) - but
**bomb-kill is unchanged in absolute terms: 2/64 = 3.1% both times.** The apparent
improvement is losses converting to draws and crush-wins, not new kills.

**All three predeclared intervention thresholds triggered:** bomb-kill (3.1%) well
below the ~10% floor; WAIT (64.1%) above the ~60% ceiling; arena-crush still causes
the large majority of decisive outcomes on both sides.

### Config 2/4 — crush01-130, SD-off (the discriminating control), N=32

vs MCTS-256: **1W-62D-1L, score=0.5, 96.9% timeout draws.** Without the crush
mechanic to convert stalemates into decisions, crush01-130 essentially cannot force
*or avoid* a decisive outcome against a held-out strong opponent at all. The two
decisive games were both bomb-kills (the only cause type possible with no crush), but
there were only two of them out of 64.

**This is the same near-all-draws pattern the original investigation found at
iteration 50** (SD-off-vs-MCTS: 91.7% draws then). The gap has not meaningfully
closed between iteration 50 and iteration 130 on this specific diagnostic - if
anything it looks slightly wider (96.9% vs 91.7%), though a two-point comparison
across different checkpoints/times isn't strong evidence of a trend by itself.

**Reading config 1+2 together:** the SD-on score improvement (0.4→0.6) is now
understood to be almost entirely a crush-mechanic artifact, not increased combat
competence - exactly the failure mode this whole harness was rebuilt to catch. Lever
1 (reward reshaping alone) has **not** produced defensible combat skill, 130
iterations in.

Configs 3/4 (control03, the matched no-lever comparison) in progress - the honest
conclusion isn't final until that side of the comparison is in, since without it
there's no way to know whether crush01's numbers are worse than, the same as, or
(unlikely given the above, but not yet ruled out) better than what 28 iterations of
completely unmodified training would have produced anyway.

### Config 3/4 — control03-130, SD-on, N=32 (64 games), matched control (0.3, unmodified)

vs MCTS-256: **45W-7D-12L, score=0.8**, WAIT=65.7%. Win-cause: bomb-kill=2 (4.4% of
wins), arena-crush=43 (95.6%). Loss-cause: self-kill=5, arena-crush=7. Draws:
mutual-death=7.

**Striking comparison to config 1 (crush01-130, same eval protocol, same seeds):**
score 0.8 vs 0.6 - control03 (no lever) scores *higher*. But **bomb-kill is
identical: 2/64 = 3.1% for both.** WAIT is within noise of each other (65.7% vs
64.1%). Control03 simply produced more crush-wins (43 vs 29) and fewer losses (12 vs
16) - not more real kills.

**Reading so far:** the reward lever (crush01) has not improved real combat skill
relative to doing nothing (control03) - both are equally unable to force a bomb-kill
against MCTS, at the same 3.1% rate. If anything, by the SD-on score alone the
*unmodified* continuation looks better, though that's plausibly just how it happens
to navigate the crush mechanic rather than a meaningful difference, since the actual
combat-skill number (bomb-kill%) that matters is tied. Config 4 (control03 SD-off)
will show whether control03's underlying combat skill differs from crush01's now
that the crush confound is removed - expecting a similarly dismal near-all-draws
result given the identical bomb-kill rates, but not assuming it before the number is
in hand.

### Config 4/4 — control03-130, SD-off, N=32 (64 games)

vs MCTS-256: **3W-56D-5L, score=0.5**, WAIT=58.2%, 87.5% timeout draws. Wins: 3
bomb-kills (100%). Losses: 4 self-kill, 1 bomb-kill (the one game where MCTS itself
landed a real kill against control03).

### Paired comparison (tools/analyze_paired_gate_eval.py, identical seeds both sides)

SD-on: net bomb-win delta = **0** (2 vs 2 - and not even the same seeds). SD-off: net
delta = **-2** (crush01 minus control03) - the *unmodified* control landed 2 more
paired bomb-wins. Neither comparison favors the lever.

### Verdict: lever 1 FAILED, all three predeclared thresholds triggered for BOTH configs

bomb-kill 3.1%/3.1% (floor ~10%), WAIT 64.1%/65.7% (ceiling ~60%), crush-win share
93.5%/95.6% of wins. crush01's SD-on score improvement over the iter-100 baseline
(0.4→0.6) is a crush-mechanic artifact - losses converting to crush-wins and draws,
not new kills - confirmed by SD-off collapsing to 96.9% draws, matching the
near-total-draw pattern found in the original investigation at iteration 50.
**Retained as an honest negative result per KL-100's standard, not reframed.**

## Root-cause diagnostic: first real use of KL-107 tracing

Ran `tools/analyze_neural_trace.py` against the SD-on trace files captured during
configs 1 and 3 (8156 and 8079 rows respectively - full per-step traces, not
samples). Across ~13 games inspected per config: **the final chosen action agrees
with the raw network policy's own argmax 70-95% of the time (mostly 80-90%),
consistently in both crush01 and control03.**

This is the load-bearing new finding of the night. If MCTS search were the primary
source of passivity - an aggressive raw policy getting "corrected" toward WAIT by the
search's own value backups - chosen action would frequently *diverge* from the raw
policy's own preference. It mostly doesn't, in either config. **The raw policy itself
is already passive before search touches it, regardless of which reward lever was
applied.** Neither lever 1 (reward reshaping) nor the not-yet-tried lever 2
(sudden-death timing) touches the policy's own training signal or the self-play data
distribution that shaped it - both operate on the environment/reward side. This
directly informs Brick 4/KL-105's eventual "same-state diagnosis" (raw policy vs
PUCT vs robust-search vs held-out-opponent-action) - this session's read already
points at policy/data, not search, being the primary lever to pull there.

Entropy (0.77-1.18 out of a max ~1.79 for 6 actions) and value trajectories (many
games start confident ±0.5-0.9, decay toward 0/negative as the game runs long and a
draw becomes likely) both look like a value head that's tracking the game
sensibly, not a broken/collapsed one - reinforcing that the issue is specifically
about what the POLICY has learned to prefer doing, not a value-estimation failure.

**Caveat, stated plainly:** this is a qualitative read of ~13 games' trace summaries
per config, not a rigorous statistical test - a real, useful diagnostic signal, not
proof. Worth a more systematic pass (all 64 games, not a manual sample) if/when this
becomes load-bearing for a bigger decision.

## Decision: lever 2 launched with calibrated (lowered) expectations

`--sudden-death-start` 120→160, forked from **control03's** iteration-130 checkpoint
(the stronger unmodified baseline, not crush01 - one variable at a time, no reason to
carry lever 1's reward change into lever 2). `tools/launch/lever2-sdstart160-
bootstrap.ps1` / `-resume.ps1` created, following the exact same fork+watchdog-resume
pattern as control03. Bootstrap launched.

**Explicitly logging low confidence, not hiding it:** given the policy-not-search
finding above, lever 2 is plausibly going to fail for the same underlying reason
lever 1 did, since it doesn't touch the policy's training signal either. Running it
anyway because it's next in the predeclared order, costs nothing new to verify
honestly (zero new C++ code, same fork/watchdog infrastructure already built and
tested), and a wrong prediction here is exactly the kind of thing worth being wrong
about on the record rather than skipping based on an untested hunch. If it also
fails, the better-reasoned next candidate is lever 3 (opponent diversity in the
league - MCTS or frozen-self, not just heuristic) implemented with a *weak* MCTS
config specifically to avoid tanking training throughput, or escalating straight to
KL-105's curriculum work, which already depends on the KL-107 trace tooling this
finding came from.

## Session time budget note

Current wall-clock is ~09:45 AM PDT; the "your 10 hours starts now" autonomy grant
landed roughly 7.5-8 hours before this point (estimated from Linear issue
timestamps around the KL-101-109 creation cluster, ~01:15-02:00 AM PDT - no exact
marker exists in this conversation, so treat this as an estimate, not a precise
figure). This is why lever 2 was launched rather than immediately implementing lever
3 (which needs new C++ code - a league opponent-type CLI flag - that deserves
un-rushed implementation, not something to write under time pressure late in an
autonomous window). Shifting to final consolidation now: verifying everything is
committed, and making sure this file gives a complete, ordered picture for morning
review rather than starting further new work threads.
