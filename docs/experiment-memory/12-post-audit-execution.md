# Post-audit execution: Phase 0 onward

Narrative log for the phased plan in `C:\Users\kaust\.claude\plans\can-you-audit-the-jolly-sutherland.md`
("Audit of post-d6eeb4e work + execution plan through the first KL-105 intervention"), executed
starting from HEAD `ab01458`. That plan's Part A is an independent audit (by a separate planning
session) of `11-kl107-v3-audit-review.md`; Part B is the phased implementation plan followed here.
Every brick below is its own commit; advisor checkpoints are called out explicitly, including when
the advisor tool was unavailable and the plan's documented-self-review fallback was used instead.

## Session start (advisor checkpoint 1)

Read the plan in full (the file directly, not the pasted copy, which had visible copy/paste
corruption in several places). Verified repo state matches the plan's assumptions: clean tree,
`HEAD=ab01458`, branch `polish/viz-and-policy-clarity`, 43 native tests present (`ctest -N`).

Advisor call attempted per checkpoint 1 ("session start... before the first edit") — **advisor
tool errored, unavailable**. Per the plan's own fallback clause, checkpoint 1 is one of the
checkpoints explicitly permitted to proceed on documented self-review when advisor is down
(only checkpoints 3/4/5 hard-stop). Self-review performed: spot-checked the plan's F1 finding
(dirty-tree `git_commit` stamp) directly against the three saved aggregate JSON files -
confirmed all three show `git_commit: d6eeb4ea30d3-dirty` with an identical `executable_sha256`
(`32394bed7c83...`) across all three, matching the plan's claim exactly. Nothing else in the
plan conflicted with what I already knew from having written the code it describes. Proceeded to
Phase 0a.

## Phase 0a: formalize the direct stats (F2)

Extended `tools/analyze_wait_diagnostic.py` with three new report sections, per the plan's spec:

1. **Raw policy head, directly measured** — `argmax(raw)==WAIT`, `chosen==WAIT`, `added`/
   `removed` (search+mask changing the outcome relative to the raw head's own top pick),
   `mean_raw_P(WAIT)`, `masked_rows` fraction, and `mask_kills_raw_top` (a precise boolean check
   — `safe_action_mask[argmax(raw)] == 0` — not an argmax-vs-argmax comparison, which would also
   pick up floating-point recompute-vs-search noise near ties; verified this distinction matters
   empirically, see below).
2. **Root Q: WAIT vs. best visited safe alternative** — restricted to "comparable" rows where
   WAIT and at least one other action were both actually visited (`mcts_policy[a] > 0` as the
   visited proxy, guarding against `search_root_q_values`'s 0.0 default for unvisited actions
   being misread as a real, competitive Q value).
3. An arithmetic self-check: `chosen==WAIT% == argmax(raw)==WAIT% - removed% + added%` on every
   row, printed as a warning if it ever fails (it didn't, on any of the three files).
4. Relabeled the Wilson bound (F3): `z=1.96` is a one-sided **97.5%** bound (equivalently the
   lower end of a two-sided 95% interval), not one-sided 95% as previously labeled. Kept `z=1.96`
   unchanged — the conservative direction was already correct, only the label was wrong, and this
   project has retracted a claim over a naming issue before (v5/iteration-210).

**Verification against the plan's independently-computed table A2:** re-ran the new tool against
the same three trace files this session already had on disk
(`results/kl107-wait-diagnostic-2026-07-11/{crush01-iter130,control03-iter130,lever2-iter160}
-trace.jsonl`, unchanged, no new evaluation run). Result:

```
checkpoint        argmax(raw)==WAIT  chosen==WAIT   added  removed  mean_raw_P(WAIT)  mask_kills_raw_top
crush01-iter130           63.0%          62.0%      4.2%    5.3%       0.367              0.0%
control03-iter130         63.4%          62.5%      4.0%    4.9%       0.369              0.0%
lever2-iter160            64.9%          64.2%      3.4%    4.1%       0.410              0.0%

checkpoint         comparable_rows  WAIT_loses   mean_gap
crush01-iter130            3477       58.0%     -0.0158
control03-iter130          3223       55.2%     -0.0220
lever2-iter160             4461       54.3%     -0.0100
```

This is an **exact match** on every figure in the plan's table A2, and the Q-gap figures fall
inside the plan's stated ranges (55-60% WAIT-loses, -0.010 to -0.022 mean gap) for all three
checkpoints. No arithmetic-check warnings fired on any file. This gives strong, independently-
reproduced confidence in both the new tool's correctness and the plan's own prior (separately
computed) analysis — two independent implementations landing on identical numbers is meaningfully
stronger evidence than either alone.

**What this changes about the claim:** the headline finding ("passivity is the raw policy head's
own preference") was previously supported by an *inference chain* (raw top survives masking
99.7% of the time → search agrees with the masked prior → chosen action reflects it). It is now
supported by a **direct measurement**: the raw policy head's own argmax IS WAIT on 63-65% of all
traced steps, essentially matching the final chosen-WAIT rate (62-64%) with only a small (~1
point) net anti-passive nudge from search+mask (removed exceeds added in all three checkpoints -
search's own value estimates mildly favor acting, per the Q-gap table, but visit-count allocation
at 96 simulations still lands on WAIT most of the time when the raw prior already favors it
heavily). The new wrinkle (Q(WAIT) loses to the best visited alternative on 54-58% of comparable
rows) is genuinely informative but per the plan's own hedge, should be treated as suggestive, not
conclusive - it's a real signal that the PRIOR, not search's own value judgment, is what's
dominating the final decision at this simulation budget, but the sample is a single diagnostic
pass, not a designed experiment.

**Linear updated:** KL-107 (direct-measurement table replaces/augments the inference-chain
framing, Q-gap observation added, Wilson-bound relabeled) and KL-105 (Diagnosis section updated
to reference the direct measurement). See both issues for the full text.

**Accept criteria met:** tool output reproduces table A2 exactly; Linear/docs updated; committed.

## Phase 0b: provenance hygiene (F1)

Confirmed F1 directly before fixing it: queried the three saved aggregate JSON files
(`results/kl107-wait-diagnostic-2026-07-11/*-agg.json`) and found all three stamped
`git_commit: d6eeb4ea30d3-dirty`, with an identical `executable_sha256` (`32394bed7c83...`)
across all three - confirming internal consistency (same binary for all three comparisons) even
though the human-readable commit stamp doesn't reflect the v3 source that actually ran.

**First rebuild attempt exposed the actual bug, not just its symptom.** Rebuilt at clean HEAD
(`2396258`, `git status` empty) via `cmake --build build-native-gpu --config Release` alone -
43/43 tests still passed, but a quick trace smoke-test against the cheap CI fixture still showed
`git_commit: d6eeb4ea30d3-dirty`, unchanged, even at a fully clean, fully-committed tree. This is
because `AI_BOMBER_GIT_SHA` is resolved by a `git rev-parse`/`git diff` check inside CMake's
*configure* step (`CMakeLists.txt` lines 4-16), and `cmake --build` alone does not re-run
configure unless `CMakeLists.txt` itself changed - it reuses whatever value the *last* configure
captured, however stale. Re-ran `cmake -S . -B build-native-gpu` explicitly (a real reconfigure),
rebuilt, and the stamp corrected itself to `2396258e40ee` (matching HEAD exactly, no `-dirty`
suffix) with a new binary hash `37b067c1ac74...` (different from the stale build's hash, since
the embedded SHA string is itself part of the binary's bytes).

**Practical rule going forward, now documented in `trainer.h` next to `AI_BOMBER_GIT_SHA`'s other
usage**: a `git_commit` stamp on any evidence file is only trustworthy if `cmake -S . -B
<builddir>` was explicitly re-run since the last commit, not just `cmake --build`.
`executable_sha256` is the only field immune to this - it hashes the actual bytes regardless of
what configure step produced them, which is why it was already treated as the strong anchor in
`11-kl107-v3-audit-review.md` and `KL-107`'s Linear text.

**Full verification at the corrected, clean-HEAD build:**
- `cmake --build build-native-gpu --config Release -j 12` — clean, zero errors.
- `ctest -C Release` in `build-native-gpu` — **43/43 passed**.
- `ctest -C Release` in `build` (dependency-free) — **22/22 passed**.
- `bomber_alphazero_native.exe` SHA-256 at commit `2396258`: `37b067c1ac7424adc1be66ce47703c3
  6106ffa667e08c3aa0f75e0718de9e877`. Recorded here and in the Phase 0c evidence manifest.

This does **not** change the validity of the earlier diagnostic findings (Phase 0a's table, the
systematic pass) - the *source* that produced them is exactly what's now committed (confirmed by
`11-kl107-v3-audit-review.md`'s own faithfulness cross-check and this session's Phase 0a
reproduction), only the evidence's self-description of which commit produced it was stale. Future
evidence generation (Phase 1's clean-binary re-run) will use this freshly-reconfigured binary.

**Accept criteria met:** clean-HEAD binary exists with a recorded, verified-accurate hash; the
configure-time staleness caveat is documented in `trainer.h`. The optional CMake custom-target
work (auto-refreshing the SHA at build time) was explicitly skipped as out of scope for this
phase's acceptance bar - noted as a possible future improvement, not implemented.

## Phase 0c: evidence archive (F5)

Created `docs/experiment-memory/evidence/kl107-wait-diagnostic-2026-07-11/` (committed) with:

- the three `*-agg.json` aggregate evaluation files (already write-once evidence per KL-101 -
  embeds exact invocation argv, checkpoint/executable SHA-256, timestamp, git commit stamp,
  resolved runtime semantics - copied verbatim, not regenerated);
- `SHA256SUMS`, listing the three committed aggs, the three (uncommitted, local-only) trace
  JSONLs, the three evaluated checkpoints, and both relevant binaries - the stale-stamped one
  that actually produced this evidence and the corrected clean-HEAD one from Phase 0b, both
  labeled clearly so neither is mistaken for the other;
- `README.md`, restating the Phase 0a stats table for reference without needing the local trace
  files, and explaining exactly why the raw traces themselves aren't committed (git-ignored
  `results/` convention, multi-MB files) and how to regenerate them (each agg's own
  `invocation_argv` field is the exact reproducing command).

Per the user's decision #3 (evidence preservation: commit agg JSONs + SHA-256 manifest; raw
traces stay local) - implemented exactly as specified, no raw trace JSONL committed.

**Accept criteria met:** committed; manifest lists every artifact with a hash, and every
committed artifact's producing argv is already embedded in the artifact itself (the agg JSON
files were KL-101 write-once evidence to begin with).

## Phase 0d: trace UX edges (F6/F7)

**F6.** Confirmed directly (`trainer.cpp` lines 3059-3063): `trace_output` is only ever passed
to the MCTS-branch `evaluate_baseline()` call, so `--trace-output` without `--eval-mcts`
previously produced no trace file at all - silent, not even a header-only one. Added a stderr
warning in `evaluate_only()`, right after the existing evidence-path checks: *"warning:
--trace-output was given without --eval-mcts; no rows will be written..."*. Verified against the
cheap CI fixture: running `evaluate --trace-output PATH` without `--eval-mcts` now prints the
warning and confirmed `Test-Path PATH` is `False` afterward (matches the documented behavior
exactly, now loud instead of silent).

**F7.** "Forced" (in `wait_forced` and this session's diagnostic tooling) means WAIT was the
position's only *safe* action per the tactical safety model (`safe_action_mask_for()`), not the
only *legal* action per plain game rules - an action can be legal but tactically unsafe (e.g.
walking into an active blast radius). Found two real instances of this conflation, both fixed:
`trainer.cpp` line ~2202 (a code comment said "only legal move," corrected to "only SAFE action"
with the legal-vs-safe distinction spelled out inline) and `11-kl107-v3-audit-review.md` line 287
("only legal/safe action" softened the distinction into a slash, corrected to state the
distinction explicitly). Checked the CLI `--help` text and the `trace_output` field's other doc
comments (`trainer.h`, `trainer.cpp` lines 2093/3350) for the same conflation - both already said
"safe-action mask" correctly, nothing further to fix there.

**Verification:** `cmake --build` + `ctest -C Release` in `build-native-gpu` after both changes
(the F6 code change and the F7 comment fix) — **43/43 passed** both times. The F6 warning was
manually exercised against the cheap fixture and confirmed to fire and to correctly predict the
no-trace-file outcome.

**Accept criteria met:** warning observable on a `--trace-output`-without-`--eval-mcts` run;
`ctest` still green.

---

**Phase 0 complete.** All four sub-items (0a direct stats, 0b provenance hygiene, 0c evidence
archive, 0d trace UX edges) done, each its own commit, narrative log updated per brick as this
file's own standing constraint requires. Next: Phase 1 (KL-107 to diagnostic-complete: v4 trace
fields, clean-binary re-run, representative writeups, overhead benchmark) — advisor checkpoint 2
applies if any test-assertion/threshold change is needed along the way; checkpoint 3 (hard stop
if advisor is down) applies before Phase 2's design work begins.

## Session handoff, 2026-07-11 22:xx — a second execution session picked up this plan

This file was written by one session executing Phase 0 (advisor unavailable, self-review
fallback per the plan's own clause). A **separate** session, on the same branch/HEAD
(`e399a5b`, clean tree), was independently told to execute the same plan file
(`C:\Users\kaust\.claude\plans\can-you-audit-the-jolly-sutherland.md`) starting from Phase 1.
Advisor checkpoint 1 (session start) was attempted again in this second session and errored
again, matching the first session's experience exactly - proceeded per the plan's documented-
self-review fallback (permitted for checkpoint 1). Self-review: read this file in full, verified
`git log`/`ctest -N`/Linear KL-107 all showed Phase 0 genuinely complete and matching the plan's
own accept criteria before starting Phase 1, rather than assuming it from the plan text alone.

No native GPU process was running at handoff (`Get-Process bomber_alphazero_native` empty); the
two `.trainer.lock` files under `results/` are stale content from training runs that finished
hours earlier (Windows releases the underlying OS-level exclusive handle on process exit -
`ProcessLock` in `trainer.cpp` - so a leftover lock **file** does not mean a leftover lock
**hold**; the file's continued existence is not itself a hazard, only a currently-running
process holding its handle would be). Continuing to check for a running process before every
GPU-touching step here, as the standing one-process rule requires regardless of how many
sessions might be involved.

## Phase 1a: v4 trace fields (`opponent_modeled_as`, `learner_moved`)

Added to `--trace-output` (`trace_format_version` 3→4, `src/training/native/trainer.cpp`):

- **`opponent_modeled_as`**: what `SearchConstraint` told the search to assume for the opposing
  seat during this step's lookahead - `"self"` when `fixed_opponent_seat == -1` (meaning
  `expand_and_backup` used the network's own policy for BOTH seats internally, regardless of who
  the outer match is actually being scored against) or the fixed baseline agent's name otherwise.
  Read directly off `constraints[active_index]`, already in scope at the trace call site - no
  new computation, just surfacing an existing decision.
- **`learner_moved`**: whether the learner's board position (`match.env.state.agents[seat].x/y`)
  actually changed this step. WAIT and PLACE_BOMB never move the agent by construction (asserted
  as a hard, fixture-independent invariant in the regression test, not just observed). A movement
  action with `learner_moved == false` means it was blocked - by terrain/a bomb/the opponent, or
  by losing a simultaneous-move collision resolution - which is exactly the "effective idle"
  category the original audit flagged as invisible to WAIT% alone (its concrete example: two
  agents repeatedly attempting the same tile, the joint resolver rejecting both for ~40 ticks).

**Structural note, not a bug:** `learner_moved` needs the POST-step position, but the rest of the
row (raw policy/value, masked prior, Q values) is captured PRE-step, and no existing hook in
`expand_and_backup` exposes post-step state at the pre-step capture point. Rather than
restructure the search internals, the trace row is now built in two pieces within the same
per-`active_index` loop iteration: everything through `opponent_modeled_as` is written and the
JSON object is left deliberately open (no closing brace, no flush) at the point where `--trace-
output` used to close it; `env_step_joint` runs (unchanged); then a second `if (trace_log)`
block appends `learner_moved` and closes/flushes the row. This loop body is not
OpenMP-parallelized (confirmed by re-reading the function - only the earlier tree-descent root
loop inside `search()` carries `#pragma omp parallel for`), so holding a partially-written
`ofstream` object open across the intervening `env_observe`/`agent_act`/`env_step_joint` calls
within one iteration is safe - nothing else writes to `trace_log` concurrently.

**Also found while implementing:** with the current wiring, `opponent_modeled_as` will read
`"self"` on **every row of every trace file this code can currently produce**, always - because
`--trace-output` is only ever threaded into the MCTS-baseline `evaluate_baseline()` call (Phase
0d / F6), which forces `type == AGENT_MCTS`, which forces `modeled_seat = -1` for every traced
match. This is not a bug or a wasted field: it converts the previously-qualitative caveat already
documented in `docs/NATIVE_ALPHAZERO.md` ("neural PUCT does not recursively invoke a real MCTS
opponent at internal search nodes during vs-MCTS evaluation") into a concrete, per-row,
mechanically-verified fact rather than a design-doc assertion - and it future-proofs the trace
format for if/when trace capture is ever extended to the random/heuristic evaluate path, where
this field would then show real variance. Recorded here explicitly so a future reader (including
a future session) doesn't mistake "this field is always the same value" for "this field must be
broken" - exactly the shape of misreading this whole KL-107 effort exists to prevent.

**Test coverage** (`tests/test_native_alphazero_trace_raw_check.py`, `trace_format_version`
bumped to 4 in the assertion): the fixture uses `--eval-mcts`, so every row's
`opponent_modeled_as` is asserted to equal exactly `"self"` (a fixture-specific exact-value
check, not just a type/non-empty check - matches this file's established property-testing
philosophy). `learner_moved` is asserted to be a bool, and `chosen_action in {BOMB, WAIT} =>
learner_moved is False` is asserted as a hard invariant on every row.

**Docs updated:** `trainer.h`'s `trace_output` field comment, the CLI `--help` text for
`--trace-output`, and both analyzer tools' module docstrings.

**Verification:** full reconfigure (`cmake -S . -B build-native-gpu`, per the Phase 0b lesson
that `cmake --build` alone reuses a stale configure-time git SHA) + `cmake --build ... --config
Release -j 12` - clean, zero errors. `ctest --test-dir build-native-gpu -C Release`: **43/43
passed**, including the extended `test_native_alphazero_trace_raw_check` exercising both new v4
assertions. `ctest --test-dir build -C Release` (dependency-free): **22/22 passed**, unaffected.

## Phase 1d: trace overhead benchmark

Ran the same small MCTS eval (control03-iter130, 2 games × 2 seats = 4 matches, 96 search
simulations, seeds 900001/1300001 - identical in every flag) twice against the freshly-rebuilt
v4 binary: once without `--trace-output`, once with.

```
without trace: 230.4 sec
with trace:    208.3 sec  (466 trace rows)
```

**Both runs produced byte-identical game outcomes** (2W-0D-2L vs MCTS, WAIT=72.3%, bomb-kill=0/2
wins by arena-crush, same heuristic/random breakdowns) - confirming trace capture is genuinely
read-only with respect to the actual search/action-selection RNG streams, not just designed to
be. This is itself useful evidence, independent of the timing question: turning tracing on does
not perturb what the checkpoint does.

**On overhead specifically: not measurably distinguishable from run-to-run wall-clock noise at
this scale.** The traced run was nominally *faster* (208s vs 230s, ~10%) - this is not a real
speedup (an extra single-position forward pass per step cannot make a superset of the same work
faster) but ordinary variance (OS scheduling, GPU clock state, disk I/O timing) that exceeds
whatever the true per-step trace cost is at this configuration size (128 channels, 10 blocks,
4 matches). A single paired run is the plan's own stated bar ("benchmarked... report per-step
overhead once") and that bar is met, but reporting a fabricated per-step millisecond figure from
noise this size would be a precision claim this data doesn't support - the honest report is "no
measurable overhead at this scale in one paired trial," not a specific number. Multiple repeated
trials would be needed for a real per-step estimate; not done here as out of scope for a
one-time benchmark check.

**Accept criteria met:** benchmarked once, with/without comparison reported, disable-by-default
zero-cost behavior already established in the v3 work and reconfirmed structurally unchanged
here (both new v4 fields are declared/computed only inside `if (trace_log)` guards).

Both 1a and 1d committed together (one coherent slice: the v4 field addition, verified twice -
once via the CTest fixture, once via a real-checkpoint timing/outcome check - not two separate
semantic changes).

## Phase 1b, attempt 1: self-caught repeat of the exact Phase 0b provenance mistake

Wrote `tools/launch/kl107-v4-diagnostic-rerun.ps1` (four sequential `evaluate --trace-output`
calls: crush01-130, control03-130, lever2-160 native SD160, and a new lever2-160-forced-to-SD120
variant for cross-checkpoint uniformity), launched it in the background, and it completed
cleanly in **~53 minutes total** - far faster than doc 11's ~3h estimate (dominated by a ~2h
lever2-SD160 leg last time; this run's equivalent leg took 16.8 min). All four legs' win/loss/
draw and WAIT-fraction numbers looked sane on inspection. No investigation into *why* it was
faster this time was pursued beyond confirming the data itself was intact (row counts, header
provenance) - faster and clean does not carry the same "is this actually still running"
uncertainty slower or hung would, so the CPU-time-delta liveness check this project uses for
suspiciously slow runs wasn't needed here.

**Then the header check caught a real problem:** every trace file's `git_commit` read
`e399a5b7be3a-dirty` - the *sibling session's* Phase 0d commit, not this session's Phase 1a
commit (`57f48dc`, already made and pushed by the time this run started). Root cause: the
binary used for this run was built (reconfigure + compile) *before* the Phase 1a commit, while
those exact same changes sat uncommitted in the working tree - so CMake's dirty-check correctly
saw a modified tree relative to `e399a5b` and stamped it `-dirty`, and committing afterward
didn't retroactively fix an already-built binary's embedded string. This is the identical
failure shape Phase 0b just fixed and documented one section above in this same file - the
binary's *content* was correct (the diagnostic data is from the real v4 code), only its
self-description was stale, but producing Phase-1b evidence that repeats the exact provenance
gap Phase 1b exists to supersede would have been a real (if minor) embarrassment, not just a
cosmetic slip, given this project's own standing rule about exactly this.

**Rebuilding at HEAD didn't immediately fix it either - a second, smaller wrinkle.** A
reconfigure + rebuild at `57f48dc` (tracked files all committed) still stamped `-dirty` on the
CI fixture smoke-check. Cause: `CMakeLists.txt`'s dirty-check uses `git status --porcelain
--untracked-files=all`, which counts untracked files as dirty too - and this session's own
`tools/launch/kl107-v4-diagnostic-rerun.ps1` was sitting untracked in the tree the whole time.
Fix: committed the launch script first (`f81cecc` - it belongs in the repo regardless, per the
plan's own "archive per 0c" instruction for this evidence), *then* reconfigured/rebuilt.
Confirmed clean: the CI fixture's trace header now reads `"git_commit":"f81ceccf830a"` with no
`-dirty` suffix, exact match to HEAD. 43/43 native CTest still green. Deleted the attempt-1
trace/agg files (dirty-stamped, otherwise-valid data - not archived, not cited) and re-launched
all four configs against the properly-stamped binary.

**Lesson, stated plainly for next time:** the correct order is commit -> reconfigure/build ->
generate evidence, never build -> generate evidence -> commit. Phase 0b's own fix message said
almost exactly this ("a git_commit stamp... is only trustworthy if `cmake -S . -B <builddir>`
was explicitly re-run since the last commit") but didn't spell out that *any* uncommitted
change - tracked or untracked - invalidates the stamp, which is what actually tripped this up.
Caught by this session's own header-verification habit (checking `trace_format_version`/
`git_commit`/`executable_sha256` on the output before treating it as evidence), not by a
process failure or test failure - both attempts' underlying computation was fine throughout.

## Phase 1b, attempt 2: clean-binary re-run — complete

Re-launched with the properly clean-stamped binary (commit `f81cecc`, executable SHA-256
`836d3a3d91c7906e...`). All four configs completed in **~47.5 minutes total** (crush01 10.4min,
control03 10.5min, lever2-SD160native 15.1min, lever2-SD120uniform 11.3min) - consistent with
attempt 1's ~53 minutes (not the ~3h originally estimated from doc 11's historical lever2-SD160
timing; no investigation pursued into why this session's runs are faster, since the data itself
verified sound - see "what to check" note below). Header provenance verified directly on all
four trace files before treating any of it as evidence: `trace_format_version:4`,
`git_commit:"f81ceccf830a"` (exact HEAD match, no `-dirty`), same `executable_sha256` across all
four (confirms one consistent binary produced everything).

**Row counts and every outcome number are byte-identical to attempt 1's dirty-stamped run**
(4104/4004/5349/4051 trace rows; identical W-D-L, WAIT%, and win-cause breakdown on all four
configs) - this is expected (attempt 1's underlying computation was already correct, only its
self-description was stale) but was verified directly rather than assumed, closing the loop on
the attempt-1 mistake with actual evidence that no data was lost or altered by the rebuild.

**Exact reproduction of the v3 evidence**, for the three overlapping configs (crush01-130,
control03-130, lever2-160-SD160): WAIT%/forced%/forced_lcb%/chosen%/chosen_lcb% match the
original systematic-pass table to the tenth of a percent on every figure, every checkpoint -
not "within noise," genuinely identical. Confirms the v3→v4 field addition didn't touch the
existing computation path (only added new fields), and that the v3 evidence's `-dirty` stamp
(Phase 0b) was purely a self-description problem, never a data problem.

### The v4 effective-idle table (new)

```
checkpoint                    WAIT%  blocked_move%  combined_idle%  combined_lcb  mean_streak  max_streak  streaks>=10  streaks>=40
crush01-iter130                62.0%           0.3%           62.3%         60.8%         38.8         111        30/32        13/32
control03-iter130              62.5%           0.4%           62.9%         61.4%         44.9         104        31/32        19/32
lever2-iter160-SD160native     64.2%           0.6%           64.8%         63.5%         63.3         144        31/32        20/32
lever2-iter160-SD120uniform    68.0%           0.4%           68.4%         67.0%         57.5         112        31/32        19/32
```

**Two distinct findings here, worth separating clearly:**

1. **Blocked movement's contribution to the instantaneous idle RATE is small.** `combined_idle%`
   (WAIT + blocked) exceeds `WAIT%` alone by only 0.3-0.6 percentage points across all four
   configs. This is a real, measured correction to the last audit's "explicit WAIT understates
   passivity" claim, but the honest magnitude is modest at the aggregate level - explicit WAIT
   was already capturing the large majority of what "idle" means here. Reporting this precisely
   rather than assuming the earlier qualitative flag implied a large hidden effect.
2. **The STREAK structure is the actually striking finding, and it's not visible from any rate
   alone.** 94-97% of traced games (30-31 of 32) have at least one run of 10+ consecutive
   combined-idle steps; 41-63% (13-20 of 32) have a run of 40+ consecutive steps - directly
   corroborating, and now quantifying across every traced game rather than one anecdote, the
   original audit's qualitative observation ("the joint resolver rejected both for about 40
   ticks"). Mean longest-streak length is 39-63 steps per game (games run roughly 125-170 steps
   on average per the earlier win-cause tables), and the single longest observed streak was 144
   steps - meaning idle time is not spread evenly through a game, it clumps into long stalls that
   dominate large fractions of many individual games even though the aggregate rate looks similar
   to WAIT% alone.

**A new, unplanned observation:** `lever2-iter160-SD120uniform` (the checkpoint forced OUT of
its own trained SD160 environment into the common SD120 one) shows the **highest** WAIT/passivity
numbers of all four configs (68.0% WAIT vs. 64.2% for the same checkpoint at its native SD160,
69.0% raw-argmax==WAIT, mean raw P(WAIT)=0.436 - all four of these are the highest value in their
column). This was not predicted by the plan and deserves a plain caveat rather than a causal
story: this is one checkpoint, one diagnostic pass, evaluated in an environment it wasn't trained
for - consistent with "an off-distribution environment makes this checkpoint more passive," but
equally consistent with the SD-timing lever itself (already known to be causally unresolved per
KL-98) doing something specific to this one checkpoint's policy. Flagging as a new data point for
KL-105's design, not a conclusion.

**What to check:** the run finished roughly 4x faster than doc 11's historical estimate for the
equivalent lever2-SD160 leg (15.1min here vs. ~2h there) despite identical `--mcts-eval-games`
and simulation counts. Both this session's runs (attempt 1 and 2) showed the same fast timing,
and the actual trace row counts/outcomes are internally consistent and byte-identical between
attempts, so there's no evidence of truncated or corrupted work - but the cause of the speedup
(different system load, GPU/driver state, or something specific to doc 11's original session)
was not investigated and remains unexplained. Not blocking, but worth knowing before treating any
future timing estimate in this project's docs as load-bearing.

**Evidence archived** at
`docs/experiment-memory/evidence/kl107-wait-diagnostic-v4-2026-07-11/` (four agg JSONs +
SHA256SUMS + README, raw traces stay local per the user's standing decision on evidence
preservation) - same pattern as Phase 0c.

**Accept criteria met:** v4 evidence at a clean commit hash (`f81cecc`, confirmed via direct
header inspection, not assumed); WAIT/forced/chosen stats reproduce v3 values exactly (stronger
than the plan's own "within noise" bar); effective-idle quantified with both a rate table and a
streak-structure table - the "explicit WAIT understates passivity" claim from the original audit
now has real numbers on both counts, with the streak structure being the more informative of the
two.

## Phase 1c: representative win/loss/draw writeup

The Phase 1b run didn't request `--per-match-output`, only `--trace-output`, so there was no
direct win/loss/draw/cause label to select from. Rather than re-run the expensive trace capture
again, ran one additional cheap `evaluate --per-match-output` pass (control03-iter130, identical
seed base 1300001, identical everything else, no `--trace-output` so it only took as long as a
normal MCTS eval) and cross-referenced its per-match outcome rows against the **already-captured**
`control03-iter130-trace.jsonl` by `(seed, learner_seat)`. This is only valid because
reproducibility was already proven exactly (Phase 1b's attempt-1-vs-attempt-2 comparison, and
this pass's own aggregate W-D-L/WAIT numbers, which matched the original run identically) - same
seed base + same checkpoint + same config deterministically produces the same games regardless of
which optional output flags are set, confirmed rather than assumed.

All three examples below are from `control03-iter130`, seed block `1300010`/`1300005` (part of
the standing diagnostic block, not a burned holdout), against MCTS-256. Full step-by-step data:
`tools/analyze_neural_trace.py results/kl107-wait-diagnostic-v4-2026-07-11/control03-iter130-
trace.jsonl --seed SEED --seat SEAT --steps`.

### Win: seed=1300005, seat=0, bomb-kill, 72 steps

The **only bomb-kill win in this entire 32-game batch** (aggregate: `win_by_bomb: 1` out of 25
wins) - chosen deliberately over an arena-crush win for exactly that reason. WAIT fraction 29%,
well below this checkpoint's ~63% average - already a hint that this game looks different from
the typical pattern.

Value trajectory: deeply negative for the first ~50 steps (-0.86 at step 0, still -0.32 at step
19) despite placing several bombs early (steps 1, 5, 9-10, 13-14, 18, 22, 26, 30) - these read as
ordinary crate-clearing, not threats, and didn't move the value needle much individually. The
turn: step 20 (DOWN) crosses to +0.322 for the first time, then **three explicit search-overrides-
raw-prior events in a tight window** - step 51 (RIGHT, masked/mcts favor RIGHT 78 visits vs. raw's
top pick WAIT), step 52 (BOMB, mcts 39 visits vs. WAIT's 36 - search picks the LOWER-prior action
here specifically because its backed-up value is better), and step 56 (another BOMB, 51 visits vs.
WAIT's 27). Value locks in at +0.938 by step 55 and +0.996-0.999 from step 57 onward - the window
of these three overrides is where the game was actually won, not the many earlier bombs.

**Reading:** this is a positive counter-example to the aggregate "prior dominates, search rarely
overrides it" finding (Phase 0a/1b tables: raw==masked_top ~99.6-99.7% of all steps) - in the
handful of steps that mattered most for the outcome, search DID override a passive/ambivalent
prior three separate times, each time toward the eventually-correct aggressive action. This
doesn't contradict the aggregate finding (it's still true search rarely overrides the prior
overall - three overrides out of 72 steps is itself only ~4%) but it's suggestive that WHEN search
does override, it's disproportionately likely to happen at genuinely decisive moments rather than
uniformly at random - consistent with, but not proof of, PUCT's value-driven exploration doing its
job correctly on the rare occasions the prior leaves it enough visit budget to matter. One game is
not enough to generalize this; flagging as a hypothesis for KL-105, not a finding.

### Loss: seed=1300010, seat=0, self-kill, 58 steps

Chosen over the more common arena-crush loss cause (4/5 of this batch's losses; this is the only
self-kill) because it has a genuinely analyzable tactical structure - an arena-crush loss is a
diffuse ~120-step mutual stall ended by the closing wall, without a clean single "mistake."

Steps 0-44: comfortably ahead (value +0.5 to +0.85 throughout), including an uninterrupted run of
**30 consecutive WAIT steps (15-44)** while value gradually drifted down from +0.77 to +0.45 -
textbook "chosen idling while ahead" from the effective-idle table, in a single continuous streak
close to this checkpoint's own mean_streak (44.9) for this exact config.

**The most interesting single moment in all three examples:** at step 45, `search_value_estimate`
(the backed-up search average) crashes from +0.453 to -0.062, while `value_head_raw_recomputed`
(the raw, pre-search value at the same position) still reads **+0.520** - confidently fine.
Search's lookahead detected the danger a full move before the raw value head did. Despite this,
the position **still chose WAIT for four more steps** (45-48, masked_prior favoring WAIT at
0.52-0.64 throughout) while `search_value_estimate` kept falling (-0.062 -> -0.367 -> -0.423 ->
-0.401) - a directly observed instance of the aggregate Q-gap finding (Phase 0a: Q(WAIT) loses to
the best visited alternative on ~55% of comparable rows): search's own value judgment had already
turned against WAIT, but the prior's visit-count weight kept winning anyway.

Step 49 finally breaks the streak (RIGHT, 71 visits vs. WAIT's 13 - the clearest single override
in this game), and steps 50-52 look like a real escape attempt (value recovering to +0.65-0.71).
But steps 53-56 (RIGHT, LEFT, RIGHT, LEFT - all search-overrides, value falling steadily: +0.236,
+0.084, -0.468, -0.968) show the escape running out of room, not finding it: by step 56,
`safe_action_count` had dropped to 2, and by the final step 57, to exactly **1** - the position
only had BOMB left as a tactically safe action (`policy_prior_after_safety_mask` = [BOMB: 1.00],
96/96 visits), which killed the agent. Structurally this final step is "BOMB-forced" in the same
sense `wait_forced` means "WAIT-forced" (this project's trace format doesn't currently generalize
`wait_forced` to other actions - a possible small Phase-1-adjacent follow-up, not done here).

**Reading:** the actual failure isn't the final bomb - it's the 8-step gap between when search's
own value estimate first flagged danger (step 45) and when the position finally moved (step 49),
during which the WAIT streak's opportunity cost consumed the room needed to actually escape.
Directly ties the effective-idle streak finding to a real outcome, not just an aggregate rate.

### Draw: seed=1300010, seat=1, mutual death, 137 steps

Same starting seed as the loss above, opposite seat - a genuinely different trajectory (the
learner occupies the other starting position and the MCTS opponent's internal RNG differs by
seat), not a duplicate.

Comfortably ahead essentially the whole game (value +0.75 to +0.85 from step 0 through step 121,
including another long chosen-WAIT stretch around steps 118-120). Then: step 122, a BOMB
placement that **overrides the raw prior's WAIT preference** (masked/mcts favor BOMB 43 visits vs.
WAIT's 41, raw itself still preferred WAIT at 0.41) - value immediately crashes to -0.367 and
never recovers, unlike the win example's analogous overrides. A chaotic 10-step sequence follows
(123-132, value oscillating -0.10 to -0.25) before the game **locks into `wait_forced=true` for
the final four traced steps (133-136)**, `search_value_estimate` pinned at **exactly -0.200** -
which is this checkpoint's configured `mutual_death_value` (visible in the agg JSON's
`resolved_semantics`). Search had fully converged on "this specific position is a forced mutual
death" several steps before the game actually ended, with zero remaining safe alternatives to
WAIT.

**Reading:** a clean, unambiguous example of genuinely *forced* idling (the last four steps)
immediately preceded by a *chosen* aggressive move (step 122) that made things worse, not better -
the mirror image of the win example's three overrides, where search's override toward aggression
was well-timed and correct. Together the win and draw examples show search-overriding-the-prior is
not uniformly good or bad; it's a real, if infrequent, lever that can cut either way, and this
single-game evidence doesn't establish which is more common in aggregate (a question for a future,
purpose-built pass, not this one).

### What these three examples do and don't establish

All three are single-game anecdotes from one checkpoint (control03-130) - illustrative of
mechanisms already visible in the aggregate tables (chosen-WAIT streaks, search overriding the
raw prior in both directions, forced-vs-chosen WAIT, the raw-value-vs-search-value divergence),
not new statistical claims on their own. Their value is making the aggregate numbers concrete and
inspectable, and surfacing two candidate hypotheses (search-overrides cluster near decisive
moments; raw value head can lag search's own danger detection by at least one step) that a
designed experiment, not three cherry-picked-for-legibility examples, would be needed to actually
test.

---

## Phase 1 complete — summary and handoff to the Phase 2 gate

Phase 1 (1a v4 trace fields, 1b clean-binary re-run + effective-idle table, 1c representative
writeups, 1d overhead benchmark, 1e Linear hygiene) is done. KL-107 and KL-105 updated in Linear
with the full detail; this doc has the complete narrative including two self-caught mistakes
(the Phase 1b provenance stamp issue, fixed before any evidence was archived) and their fixes.
Every commit in this phase is pushed to `polish/viz-and-policy-clarity`
(`57f48dc`..`b6b8f5f`, plus the Linear-only updates that don't have a corresponding commit).

**The plan's checkpoint 3 now applies: Phase 2 design review (the KL-105 design doc +
tactical-gate mechanism) requires either advisor sign-off or a direct user check-in before any
of it starts** - the plan's own fallback clause is explicit that this specific checkpoint hard-
stops when advisor is unavailable, unlike checkpoints 1/2/6 which may proceed on documented
self-review. Advisor was attempted once at the start of this session's work (session-start
checkpoint 1) and errored, matching the prior session's identical experience - two independent
sessions now, both unable to reach it. Per the plan: **stopping here, surfacing this to the user
directly, not proceeding into Phase 2 on my own judgment.** No code changes, design decisions, or
further Linear updates for KL-105's design are planned until that happens.

## Checkpoint 3 resolved: the user invoked the orchestrator pattern with Fable at the helm

The user's response to the hard stop above was to switch the session model to Fable and invoke
the `/orchestrator` skill (Planner → Executor → Evaluator): Fable plans and reviews in the main
session, Sonnet subagents execute the code units. This resolves the advisor-unavailable
deadlock structurally — the planner/evaluator role in the main session IS the design-review and
launch-gate authority the plan's checkpoints 3/4/5 called "advisor," per the user's standing
decision that advisor sign-off suffices for bounded training launches. Recorded here as the
authorization chain for everything below.

## Phase 2a: experiment design (doc 13, planner-written)

`docs/experiment-memory/13-kl105-experiment-design.md` (commit `851db37`): six tactical gates,
the capped cause-balanced replay intervention (single-variable treatment vs cap=0 control),
both arms forked from control03-130 with an identical explicit `--lr-schedule-updates 33280`
override (both would otherwise inherit the floor-LR bug's 1e-5, muting any treatment effect),
24 iterations/arm at measured ~8min/iter throughput, and DRAFT success/failure criteria to be
frozen at the launch gate.

## Phase 2b: tactical gates (Unit A, Sonnet executor + planner review) — commit `58f688b`

Executor delivered the `gates` subcommand per spec (six scenarios with ASCII-diagrammed
layouts and blast-arm math cross-checked against `danger_would_trap_agent`, search+raw modes,
`--gates-agent` achievability reference, write-once KL-101 evidence, 4 CI tests incl.
byte-identical-rerun determinism; found and fixed a real fixture-truncation bug — CI fixture's
`max_steps=8` silently capped every scenario — and used a strict whitelist for `--gates-agent`
instead of `agent_parse_type`'s silent fallback-to-random). Planner review verified the diff
directly (not the self-report), found one cosmetic diagram error (fixed), and made one
substantive ruling the executor had correctly flagged instead of deciding: **stall-break gains
a demonstrated-kill pass path** (`death_owner == learner`, the trap-gate standard) — both
reference agents independently bombed the defenseless CONSTANT blocker, and killing the thing
blocking the chokepoint is decisive aggression, the opposite of the passive-stall failure mode
the gate probes. Heuristic reference: 5/6 (fails only flame-timing, an identified
priority-order gap; MCTS-32 solves it, so the gate is achievable). 47/47 + 22/22 green.

## Gates baseline (pre-intervention "before" numbers) — evidence archived

`docs/experiment-memory/evidence/kl105-gates-baseline-2026-07-12/` (clean stamp `58f688b`,
`--eval-simulations 96`): **all three checkpoints pass exactly 1/6 deployed** (corridor-clear —
crate clearing, the one constantly-rewarded skill; zero pass any gate requiring opponent
engagement). References: heuristic 5/6, MCTS-256 4/6; every gate achievable by at least one.

**Load-bearing new observation:** on control03-130 AND lever2-160, raw-argmax passes the trap
gate in 5 steps while 96-sim search FAILS it — the bare policy head takes the kill; adding
search removes it. This inverts the aggregate "search is net slightly anti-passive" direction
in at least this constructed kill position and shows the search/value side actively suppressing
aggression the head would take. It sharpens the intervention mechanism: cause-balanced replay
retrains both heads (bomb-win-side samples carry +1 value targets), not just the prior.

## Phase 3 Unit B: cause-tagged replay + capped cause-balanced sampler — commit `fa72b64`

Executor (Sonnet) delivered per doc 13 section 3; planner verified the load-bearing hunks
directly (sampler's cap=0 branch is byte-for-byte the pre-existing path; mirror tagging's seat
parity matches the value-assignment parity exactly; league taxonomy is "how the game ended"
consistently; legacy-load tag defaulting is faithful-inheritance with an explicit one-line
note). Executor verified the sampler's two regimes (cap-limited and boost-limited) by exact
arithmetic on ad-hoc runs, since fixture-scale games produce no bomb kills (expected — CI
asserts machinery + liveness instead). Planner review added one thing the executor had
flagged: `replay_cause_balance_cap` in the evaluate-evidence `resolved_semantics` object, so
arm evidence files carry their treatment/control identity directly. Planner accepted the
committed 3.6MB pre-tag fixture checkpoint (snapshotted at `58f688b` with a full provenance
sidecar) as the only durable way to CI-cover the genuine missing-archive-key compat path.
54/54 native CTest (7 new), 22/22 dependency-free — re-run by the planner, not taken from the
executor's report. Two small pre-existing issues noted for backlog, not touched: `--fresh`
does not truncate `semantic-fork-log.jsonl` (unlike metrics/config-history), and no standing
test exercises the pool_a_draws>0 sampler branch (covered by ad-hoc exact-arithmetic runs and,
imminently, by the live treatment arm's own metrics).

## Phase 3 launch gate (checkpoint 4) — planner sign-off, 2026-07-12

Authorization: the user's standing decision ("advisor sign-off suffices" for bounded training
launches, ≤~40 iters/arm) + the orchestrator arrangement recorded above (planner = acting
advisor). Everything below was verified before launch, not assumed:

- **Arms**: `tools/launch/kl105-arm-bootstrap.ps1` / `kl105-arm-resume.ps1` (Unit C,
  planner-written — the exact commands are the gate artifact). Fork parent
  `control03-from102/iteration_000130.pt`, SHA-256 `ab5771d3...` (verified on disk), manifest-
  verified (no legacy flag — if the fork fails closed on semantics, that is a bug to
  investigate, not a flag to add). Arm config mirrors the parent's true resume config
  (`--batch-size 512` — note the 1024 in historical agg signatures is the EVALUATE process's
  struct default, not what training used; caught at planner review of Unit C).
- **The two explicit semantic overrides, identical story both arms**:
  `--lr-schedule-updates 33280` (both arms; expect a SEMANTIC FORK line 13184 → 33280 at
  bootstrap; LR resumes at ≈1.05e-4 annealing to ≈7.8e-5 by 154) and
  `--replay-cause-balance-cap` 0.25 (treatment; expect a second fork line 0 → 0.25) vs 0
  (control; matches the faithful-inherited default — no fork line, still explicit in argv and
  recorded in the manifest going forward).
- **Length/budget**: 24 iterations/arm (131 bootstrap + resume to 154), sequential under the
  process lock, watchdog-wrapped resumes, ~3.3h/arm at measured throughput (~8 min/iter incl.
  eval spikes) → well inside the authorized bound.
- **Criteria FROZEN** in doc 13 section 3 before launch (bomb-kill ≥6/64 AND ≥control+4;
  chosen-WAIT ≤control−8pp; SD-off draws ≤control+5pp; per-seat direction consistency;
  DIRECTIONAL allows exactly one follow-up; FLAT kills the lever). The paired-eval battery was
  amended to 32 games (64 matches) at the gate — before launch — to match the criteria's units
  and the historical lever-gate N.
- **Post-launch verification planned (KL-100 item 7)**: resolved LR at matched iterations from
  both arms' metrics.jsonl, manifest/fork-log diff showing exactly one differing semantic
  field between arms, same executable SHA-256 in both run dirs' evidence, pool-A/realized-
  fraction telemetry live in the treatment arm's metrics from iteration 131 onward.

## Launch attempt 1 failed closed — KL-101's fork validation caught real pre-fix corruption in the wild

The first treatment bootstrap was rejected: `fatal: parent champion artifact does not match
inherited best_iteration=10 (artifact iteration=102, lineage=10)`. Diagnosis: control03's own
`best.pt` is v6-league's iteration-102 weights relabeled as its iteration-10 champion — the
exact pre-d6eeb4e relabeling bug, sitting in the historical parent's run dir all along, and the
new fail-closed fork validation (KL-101 Part C / d6eeb4e) correctly refused to inherit it. The
two overrides otherwise behaved exactly as the gate predicted (SEMANTIC FORK line for
`learning_rate_schedule_updates: 13184 -> 33280`; the faithful-inheritance NOTE for the
pre-Phase-3 manifest lacking `replay_cause_balance_cap`).

**Resolution — `--fork-reset-champion` (planner-implemented, not a bypass):** an explicit,
fork-only option where the child's champion history starts at its own fork point: `best.pt` is
a FRESHLY-SAVED checkpoint of the just-loaded fork-point weights with `best_iteration` set to
the fork iteration first (self-consistent by construction — deliberately NOT a file copy of
the parent checkpoint, whose embedded selection metadata still carries the inconsistent
champion claims and would fail the child's own later reconcile checks), score/promotions
zeroed, `"champion_reset": true` recorded in fork-manifest.json. The parent's run dir — 
retained evidence — is not touched. Champion state never feeds collection/optimization in this
trainer (it only gates promotion telemetry), and both arms get the identical reset, so this is
behaviorally inert for the comparison. Fails closed outside `--fresh --fork-from`.
`test_native_alphazero_corrupt_best_check` extended to cover the full lattice: corrupt resume
rejected, corrupt-parent fork rejected (the exact production failure), reset fork succeeds
with a self-consistent lineage, and the reset fork's own subsequent resume stays healthy.
54/54 green.

## Treatment arm launched — bootstrap verified end-to-end (commit `b48838b`, binary `17d02287ef90...`)

Attempt 2 of the treatment bootstrap ran to completion. One operational mistake by the planner,
recorded honestly: the bootstrap was foregrounded through `... | Tee-Object | Select-Object
-First 45`, and Select-Object's early pipeline stop broke the output pipe mid-iteration — the
console capture (and the Tee file) end at league-game 18/64, and it initially looked like the
process had been killed. It hadn't: the trainer kept running detached from the broken pipe,
completed iteration 131, checkpointed, and exited cleanly at its target (latest.pt + metrics
written minutes after the pipe died). Verified from the run dir's own `train-console.log`
(the KL-101 durable tee — this exact "console was lost, what actually happened" scenario is
what it exists for) and metrics.jsonl, not from the truncated live capture. Lesson: never put
`Select-Object -First N` downstream of a long-running native process; long launches go through
`run_in_background` + the run dir's own durable logs.

**Iteration-131 metrics validate every design prediction at once:**
- `learning_rate = 1.0386e-4` — the explicit 33,280-update horizon resumed the cosine almost
  exactly at the predicted ≈1.05e-4 (vs the parent's floor 1e-5).
- `replay_cause_pools` after one iteration: 20,577 newly-tagged samples (bomb=5,914,
  arena_crush=9,108, selfkill=1,464, mutual_death=4,091) + 179,423 legacy `unknown` = exactly
  200,000. Mirror self-play's own console line reported `bomb=0` decisive kills, so every
  bomb-cause sample came from league wins — which is exactly why `bomb_win_side_pool == bomb
  count` (5,914): league bomb-wins tag the whole (learner-seat-only) trajectory as winner-side.
  Internal consistency, not coincidence.
- `realized_pool_a_batch_fraction = 0.25` — the cap is already binding (pool A ≈3% of the
  buffer, 16× boost → 0.47 > cap): 25% of every optimization batch is demonstrated-kill
  winner-side samples vs a ~3% natural rate, ≈8.5× effective reinforcement of exactly the
  signal H1 says the head never sees.
- fork-manifest.json: `champion_reset: true`, clean `git_commit: b48838b5a2a7`, resolved
  semantics carrying `replay_cause_balance_cap=0.25` and `learning_rate_schedule_updates=33280`.

Resume to iteration 154 launched under the watchdog (backgrounded properly this time),
watchdog-attempts.jsonl shows attempt 1 running. Control arm follows sequentially after.

## Phase 3 verdict: FLAT/NEGATIVE — the capped cause-balanced replay lever is killed

Both arms completed 131→154 cleanly (one watchdog attempt each; final LR identical to the
digit at 7.7844e-5; the control's iteration-131 collection was bit-identical to the
treatment's, and the evidence files' resolved_semantics differ in exactly one key,
`replay_cause_balance_cap` — verified programmatically). The frozen criteria applied verbatim:
(a) bomb-kill 1/64 vs required ≥6 — FAIL; (b) chosen-WAIT −6.3pp vs required −8pp — FAIL
(real attenuation, 1.7pp short of the frozen bar; the bar does not move after the fact);
(c) SD-off draws 92.2% vs 93.75% — PASS. SUCCESS and DIRECTIONAL both unmet → FLAT/NEGATIVE
per the pre-registered ladder. No re-runs.

**The genuinely new finding is the control arm's own drift:** 24 iterations of ordinary
v6-league training at a restored ~1e-4 LR made the control +11.7pp more passive than the
shared base (chosen-WAIT 61.9% → 73.6%) with zero bomb-kills. The data distribution itself
teaches waiting faster than an ~8.5× kill-sample boost can counteract — strong, controlled
evidence for prioritizing generation-side change (league diversification / opponent mix,
KL-105 ordered-scope item 3's other half, or H2/H3) over further sampler-side reweighting.

Evidence archived at `docs/experiment-memory/evidence/kl105-arm-eval-2026-07-12/` (decision
table + descriptive findings in its README). One operational bug caught mid-battery and fixed
before any evidence existed: the paired-eval script's step function had a parameter named
`$args`, which PowerShell's automatic variable shadowed into an empty splat — the binary ran
with no arguments, printed help, exited 0, and all three steps "passed" in one second. Fixed
with a rename + per-step expected-output existence checks (commit `43cd75b`); the no-op run
wrote nothing.

**Phase 3 complete. The plan's scope (Phases 0-3) is fully executed.** Next decision — which
lever gets the next design pass (league diversification, H2 value/objective, or H3
opponent-model) — is deliberately NOT taken autonomously: the frozen protocol ends here, and
lever selection is a fresh design decision for the user/planner with these results in hand.

## Post-verdict corrections + same-binary base re-evaluation (2026-07-12, KL-110 Phase A)

An independent closeout of the Phase 3 report caught two reporting errors and one wording
overclaim; all were re-verified against primary data and a same-binary base re-evaluation was
run BEFORE any post-verdict rebuild (preflight-confirmed: working exe hash 17d02287... ==
the arms' training/eval binary, tree clean at 227419e). Corrections, old -> new:

1. **Drift "+11.7pp" retracted — canonical figure +8.603pp.** The original compared a 16-seed
   base span against a 32-seed arm span (seeds 17-32 are more passive for the base too). On
   the identical binary and identical 64-match span: base chosen-WAIT 65.016% -> control
   73.619% (+8.603pp), treatment 67.296% (+2.280pp); treatment-vs-control -6.323pp
   (unchanged — that comparison was always like-for-like). Binary sensitivity measured nil
   (same 16 seeds, old vs arm binary: 61.888% vs 61.863%). Evidence:
   `evidence/kl105-arm-eval-2026-07-12/base-iter130-SDon-agg.json`.
2. **Dose "~8.5×" retracted — average expected exposure 1.867×** (42.508% vs 22.771%
   run-average bomb-win-side batch share; 9.205× = 27.218% vs 2.957% at iteration 131 only).
   Also reclassified as *expected* exposure computed from pool sizes: the metrics field
   `realized_pool_a_batch_fraction` is the forced quota only (uniform remainder draws also
   land in pool A); a forced-vs-total split is queued as post-battery hygiene.
3. **"Bit-identical iteration-131 collection" softened to field-level identity** (cause-pool
   counts, new_samples, learning rate identical; timing fields differ, as they must).
4. **Scope statement added:** the sampler reweighted whole winning trajectories including
   passive lead-up — the verdict kills whole-trajectory cause reweighting, not local-credit
   sampler designs, which were never tested.

The FLAT/NEGATIVE verdict itself is unaffected (the independent closeout concurred it is
valid and the arms genuinely matched). Small arm artifacts (metrics/config/config-history/
fork-manifest/semantic-fork-log/watchdog/train-console) are now archived under the evidence
directory with a normalized repo-root-relative SHA256SUMS, so the LR-equality and
single-attempt claims no longer depend on git-ignored local run dirs. Next: KL-110 Phase B,
the aligned-opponent gates ablation (no training in any branch).

## KL-110 Phase B result: `mixed` — H3a's mechanism confirmed, prior starvation gates it

The 8-run aligned-opponent ablation (4 checkpoints x self|aligned, one binary `1e734ba`,
frozen discriminator applied machine-readably by `tools/analyze_h3a_verdict.py`) returned
**mixed**: exactly one counted restoration (control03-130 — trap self-FAIL -> aligned-PASS in
4 steps with the BOMB root-visit lead, 37/96 at step 0) among the three required checkpoints;
lever2-160 (descriptive) also restored (53/96); neither 154 arm places a bomb at all even
action-exact-aligned; zero corridor regressions (treatment-154's corridor actually improves
under aligned). Per the pre-registered ladder: traces inspected, NO training launched.

The per-step records decompose the mechanism exactly (full table in
`evidence/kl110-h3a-ablation-2026-07-12/README.md`): step-0 priors are mode-identical, so the
whole effect is visit allocation — aligned value flips BOMB into the visit lead wherever the
prior leaves search room (0.105/0.149 on the pre-drift checkpoints) and cannot where drift
pushed it below ~0.08 (0.084 control-154, 0.071 treatment-154 — the kill-enriched arm has the
LOWEST bomb prior). Findings: (1) the original raw-pass/search-fail trap inversion WAS
opponent-model mismatch on the checkpoints where it was observed; (2) continued v6-league
training erodes exactly the prior that opponent-aware search needs, so search-side and
generation-side fixes are complementary, not alternatives. Scope note per the verdict JSON:
this does not kill H3 globally — leaf values remain self-play-conditioned everywhere.

Executor note for the record: Unit B caught a miscount in the KL-110 issue text ("four"
NONE/CONSTANT scenarios; the table has five — 4 NONE + 1 CONSTANT), implemented the rule
rather than the count, and flagged it. Internal-node enforcement was mutation-tested (inverted
comparison fires on all five action_exact scenarios; reverted; CI pins zero violations).

## v7 Stage 0 complete (KL-111) — commits `ea41f2b`, `986c061`, + Unit C

All five Stage-0 items from doc 14 are landed, each Sonnet-executed and planner-reviewed:
temperature-anneal semantics (0.1/0.3; anneal=false reproduces the old step function exactly;
alpha stays default-0.3 with 1.5 explicit in the v7 launcher per explicit-over-default),
KataGo forced playouts + policy-target pruning (0.2; root-only, collection-only, off-path
identical by construction; always-on floor-violation telemetry measured 0; the executor caught
a contradiction in the planner's own pruning brief and resolved it to the KataGo-consistent
form), the search-contempt prototype (0.4; gates-only, default off, live path proven 6/6
scenarios differing at nscl=1, training paths unreachable), and the drift canary (0.5; all six
gates + trap step-0 BOMB prior in metrics.jsonl every eval interval on a dedicated
constant-seeded rng — the +8.6pp drift class can never again be invisible). The gated
v7-stage1-bootstrap.ps1 carries every v7.0 semantic explicitly and throws without
-IAcknowledgeLaunchGate. Suite grew 61 -> 79 native tests across the three units; every
existing byte-identical/exact-value test stayed green throughout, which is the standing proof
that no v6-era behavior changed.

**Next gate: Stage-1 IL design** (MCTS-256 teacher collection mode, IL loss path, staged value
warmup, entropy floor — the "Bombing Collapse" guards) as its own design pass; its teacher-
collection GPU run gets a documented launch-gate sign-off before anything executes.
