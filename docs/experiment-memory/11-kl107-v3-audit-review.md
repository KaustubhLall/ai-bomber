# Review doc: KL-107 v3 raw-trace + systematic WAIT-diagnostic pass

**Scope of this review:** everything done in this session *after* your correction
commit `d6eeb4e` ("Repair experiment provenance and causal audit"). It does not
re-litigate what you already reviewed and fixed before that commit — that's
covered by the diff of `d6eeb4e` itself and my prior summary of it. This doc
covers five commits, all on `polish/viz-and-policy-clarity`, HEAD at `b07dfe1` as
of writing:

```
54a2a42  KL-107 v3: genuine raw policy/value, safe-action mask, forced-idle, root Q
63c246a  Narrative log: record KL-107 v3 raw-trace increment
4755da6  Loosen trace-raw divergence check from all-rows to majority
55aae15  Add tools/analyze_wait_diagnostic.py for the KL-107 systematic pass
b07dfe1  Narrative log: record the systematic WAIT-diagnostic pass result
```

Working tree is clean, all five are pushed. No training was launched at any
point in this window — everything is either a code change (tested, not run
against the GPU beyond CTest) or a read-only `evaluate` against checkpoints that
already existed before this session started.

## TL;DR

1. Corrected my own prior over-claiming (KL-98/KL-108/KL-100 in Linear) to match
   what your `d6eeb4e` fix actually revealed — I'd only caught one of the three
   problems your commit fixed on my first pass; caught the other two on a second,
   more careful read of your diff. Covered in Part 1.
2. Added genuinely raw (pre-safety-mask) policy/value capture to `--trace-output`
   (v2 → v3), because v1/v2 never actually had it despite the whole session
   existing because of a mislabeling bug in that exact area. Covered in Part 2.
3. Ran a systematic (not spot-check) measurement across all three checkpoints
   this session produced. Finding: passivity is the network's own raw policy
   preference, not the safety mask, not search, not the environment forcing
   idleness. Consistent within ~2 points across three differently-trained
   checkpoints. Covered in Part 4.
4. Section 5 below is a flat list of every judgment call I made without asking
   you first — start there if you want the fastest path to anything worth
   pushing back on.

---

## Part 1: Correction of my own prior reporting (Linear, no code)

Before this session's code work started, I re-read your `d6eeb4e` diff in full
and found my *first* correction pass (which I'd already posted to Linear KL-98)
had only caught one of three things your commit fixed:

1. **LR-schedule confound** (control03 frozen at 1e-5 vs crush01 actively
   annealing) — I *had* caught this in my first pass.
2. **Lever2 has no iteration-matched control** — compared against control03's own
   pre-fork starting point, so 30 extra training iterations are entangled with
   the sudden-death-timing change. I had **not** caught this in my first pass. I
   only found it by reading your rewritten executive summary in
   `10-autonomous-brick-execution.md` line-by-line rather than trusting my own
   memory of what the bug was.
3. **Trace fields were mislabeled** (`raw_policy` was actually the post-mask
   value) — I had described this as "unaffected by the LR bug" in my first pass,
   which is wrong; it's a separate, independent problem. Caught on the same
   re-read.

I rewrote KL-98 a second time to fold in all three
(`Linear KL-98`, "CORRECTION 2026-07-11 (v2 — my first correction pass was
itself incomplete)"). Added a "measurement lesson" entry to KL-100 (item 7:
causal/paired claims need every non-treatment variable *verified* identical, not
assumed). Left KL-108 untouched since your own version of it was already more
precise than anything I'd have written.

**What to check:** whether KL-98's corrected text actually matches your intent —
I did not ask you to confirm my reading of your diff before posting it, only
before starting the code work that followed. If I mischaracterized any part of
what you fixed, that's now baked into three Linear issues and should be fixed
directly there, not just noted.

---

## Part 2: KL-107 v3 code changes

### The problem being solved

`--trace-output` (v1, then renamed in your `d6eeb4e` fix to v2) only ever
captured `search_result.priors` — the network's policy **after** the engine's
own safe-action mask and joint renormalization (`trainer.cpp` line ~2205's
`expand_and_backup`, specifically lines 748-767 in the current file, where
`seat_policy[seat][action] = safe[action] ? policy[...] : 0.0f` zeroes out
unsafe actions before anything gets stored on the `Node`). There was **no** field
anywhere containing the actual unmasked softmax output. v1 called the masked
value `raw_policy` anyway, which is the bug your commit fixed by renaming it to
`policy_prior_after_safety_mask`. That rename made the schema honest but didn't
add the thing it was now honestly saying was missing.

### What I added (all in `src/training/native/trainer.cpp` unless noted)

- **`safe_action_mask_for()`, line 507** — new free function, extracted out of
  `expand_and_backup`'s inline safe-mask computation (previously duplicated
  inline at what's now line 759). Returns the `kActions`-sized 0/1 safe mask and
  a count, with the same "fall back to plain legality if the tactical safe-check
  reports zero safe actions" behavior the original inline code had. **Why
  extracted:** so the trace's `safe_action_mask` and the search's own masking
  decision can never drift apart — they now call the literal same function.
- **The v3 capture block, lines ~2170-2255** inside `evaluate_baseline`'s match
  loop, gated behind `if (trace_log)` (i.e., completely skipped, zero cost, when
  `--trace-output` isn't passed — same as v1/v2's existing behavior):
  - Encodes `match.env` (the *pre-step* game position — the exact position the
    search just evaluated) for the learner's seat via
    `bomber_training_encode_env`, runs it through `model->forward()` directly
    (same `model`/`device` the trainer already holds, same
    `torch::InferenceMode`/`AutocastGuard`/BF16 pattern `expand_and_backup`
    uses), softmaxes the logits. This is `policy_head_raw_recomputed` /
    `policy_head_raw_recomputed_entropy` / `value_head_raw_recomputed`.
  - Calls the new `safe_action_mask_for()` for that same position/seat →
    `safe_action_mask`, `safe_action_count`.
  - `wait_forced = (safe_action_count == 1 && safe_action_mask[WAIT] == 1)`.
  - `search_root_q_values`: per-learner-action root Q, computed as
    `marginal_distribution(value_sum, seat) / marginal_distribution(visits, seat)`
    elementwise (0 where visits are 0) — reuses the existing seat-aware
    `marginal_distribution` helper (line ~840) rather than indexing the joint
    arrays by hand. This mattered because `select_joint` (lines 642-665) indexes
    the joint arrays differently per seat (`action*kActions+opponent` for seat 0,
    `opponent*kActions+action` for seat 1) — a hand-rolled version could have
    silently computed a transposed, plausible-looking, wrong Q vector for one
    seat. I did not catch this risk myself; it was raised by the advisor
    consultation before I wrote the code, and I asked the advisor to
    specifically re-check the final marginalization against `select_joint`'s own
    indexing after the code was written. It confirmed no transpose bug — see
    Part 3.
  - `trace_format_version` bumped 2 → 3.
- **`src/training/native/trainer.h`**: doc comment on `trace_output` updated to
  describe the v3 fields and their semantics (what's recomputed vs. what's
  captured from the search).
- **CLI `--help` text** (`trainer.cpp`, the `--trace-output` entry): updated to
  mention the new fields.
- **`tools/analyze_neural_trace.py`**: reports the v3 fields when present
  (raw-vs-masked top-action agreement, forced-vs-chosen WAIT split, per-step raw
  top actions under `--steps`), falls back cleanly to v1/v2-only output on older
  files via `.get(new_key, old_key)`.

### Why this specific implementation shape (recompute, not capture)

`expand_and_backup` masks and renormalizes **every** node it expands — root and
interior alike — via one shared code path with no existing conditional that
distinguishes "this is a root, capture the pre-mask value" from "this is any
other node, don't bother." I considered two approaches (discussed with the
advisor before writing code):

- **Option A** — add an optional output parameter to `expand_and_backup`,
  populated only at its one root-only call site (`search()`'s `initial` call,
  line 564), leaving the hot per-simulation call site untouched.
- **Option C (what I built)** — at the trace call site, re-run a single-position
  forward pass on `match.env` (the untouched pre-step position) directly,
  completely outside `BatchedMcts`.

I chose C for smaller surface area (zero changes to `BatchedMcts`/`Node`/
`SearchResult`). The advisor agreed C was right, but flagged that my original
reasoning ("A doesn't respect trace-off-costs-nothing") was actually wrong — A
would have been equally free when trace is off. The real reason to prefer C is
just smaller blast radius, not the cost argument. I'm noting the correction here
so the commit message/comments aren't taken as the real justification if you
read them later — they weren't corrected after the advisor call, only my
understanding was.

**What to check:** whether you'd have made the same A-vs-C call. Recomputing
means the exact numeric floating-point path differs slightly (see faithfulness
cross-check below) from what search internally computed — I judged this
acceptable, that judgment is yours to override.

---

## Part 3: Verification performed

All commands below were actually run this session, not just described.

- **Build:** `cmake --build build-native-gpu --config Release -j 12` — clean,
  zero errors (warnings only, and only pre-existing LibTorch-header template
  instantiation noise unrelated to this session's changes — grepped for `error`
  after each build, confirmed none).
- **Full native suite:** `ctest -C Release` in `build-native-gpu` — **43/43
  passed** (up from 41 before this session; 2 new tests,
  `test_native_alphazero_trace_raw_evaluate` and
  `test_native_alphazero_trace_raw_check`).
- **Dependency-free suite:** `ctest -C Release` in `build` — **22/22 passed**,
  unaffected (all this session's changes are behind
  `AI_BOMBER_BUILD_NATIVE_ALPHAZERO`).
- **New regression test** (`tests/test_native_alphazero_trace_raw_check.py`,
  chained after a new tiny fixture evaluate in `tests/CMakeLists.txt`): checks
  *properties*, not well-formedness, specifically because a well-formedness-only
  test (arrays are length 6, distributions sum to ~1) would have passed on the
  original v1 mislabeled data too. It asserts:
  - `safe_action_count == sum(safe_action_mask)` on every row;
  - wherever the mask is 0, `policy_prior_after_safety_mask` is exactly 0 there
    (an invariant that must hold regardless of what "masked" means);
  - `wait_forced` never fires with more than one safe action;
  - **the decisive check**: on rows with a masked action, `policy_head_raw_
    recomputed` retains real (>1e-6) probability mass on the masked-out action
    for at least half of them (originally I asserted *all* of them; loosened
    after the advisor flagged that a converged/confident checkpoint could
    legitimately push a disfavored action's raw probability below 1e-6 without
    being exactly zero — see commit `4755da6`);
  - the faithfulness cross-check (below) stays under a 0.05 tolerance.
  On the CI-scale fixture (16-channel, 1-block, tiny), this test observed
  **48/48** masked rows showing real divergence and a max cross-check diff of
  **0.0001**.
- **Faithfulness cross-check, manually run against a real checkpoint** (not just
  the tiny CI fixture): evaluated crush01's `iteration_000130.pt` with
  `--trace-output` (2 eval-games, 2 mcts-eval-games, seed base 900001 for
  random/heuristic, `--legacy-accept-unverified-semantics
  --arena-crush-win-value 0.1` since this checkpoint predates KL-101). Took the
  492 resulting trace rows, renormalized `policy_head_raw_recomputed` by
  `safe_action_mask`, and compared to the search's own
  `policy_prior_after_safety_mask` on the same rows: **max divergence 0.0072,
  mean 0.0011** across all 492 rows. This is the empirical evidence that
  recomputing outside the search (Option C) is actually faithful to what the
  search itself evaluated, not just plausible in theory.
- **Advisor consultation, after the code was written**, specifically re-checked
  the Q-marginalization against `select_joint`'s own accumulation (lines
  642-665) for both seats independently and confirmed no transpose bug, and
  reviewed the whole diff for soundness. Its exact words: *"The safe-mask
  refactor preserves the `count` fallback semantics. This is clean and already
  shipped correctly."* I'm reporting this as advisor output, not as independent
  verification by me beyond what's described above — you may want to spot-check
  the Q-value logic yourself given it's the one piece I didn't build an explicit
  regression assertion around beyond "finite and zero where masked."

**What to check:** the regression test's 0.5 (majority) and 0.05 (cross-check
tolerance) thresholds are my numbers, chosen to be generous rather than tight. If
you want tighter guarantees, both are one-line changes in
`tests/test_native_alphazero_trace_raw_check.py`.

---

## Part 4: Systematic WAIT-diagnostic pass

You picked this option explicitly (over "design a KL-105 experiment" or "stop")
when I asked after KL-107 v3 landed.

### Method

New tool: `tools/analyze_wait_diagnostic.py`. Takes one or more
`label=path.jsonl` trace files, reports per-checkpoint: total traced steps, WAIT
frequency, forced-WAIT fraction (only safe action) with a one-sided 95% Wilson
lower confidence bound, chosen-WAIT fraction (WAIT picked with real alternatives)
with its own Wilson LCB, and how often the safety mask leaves the raw policy's
own top-ranked action unchanged.

Ran real `evaluate --eval-mcts --trace-output` against three checkpoints, all
already-trained (no new training):

| checkpoint | run-dir | flags needed | games | seed base |
| --- | --- | --- | --- | --- |
| crush01 iter130 | `results/alphazero-native-superhuman-v6-league-crush01` | `--legacy-accept-unverified-semantics --arena-crush-win-value 0.1` (predates KL-101 manifest) | 16 MCTS games (32 matches) | 1300001 (MCTS diagnostic block) |
| control03 iter130 | `results/alphazero-native-superhuman-control03-from102` | none — loads cleanly, postdates KL-101 | 16 MCTS games (32 matches) | 1300001 |
| lever2 iter160 | `results/alphazero-native-superhuman-lever2-sdstart160` | none — loads cleanly | 16 MCTS games (32 matches) | 1300001 |

`--baseline-mcts-simulations 256 --baseline-mcts-depth 16` for all three (the
project's standard MCTS baseline strength).

**Judgment call:** lever2 was evaluated with `--sudden-death-start 160` (its own
native, trained-under value), not `120` like the other two. My reasoning: this
pass makes *absolute* claims about each checkpoint's own behavior, not causal
comparisons between checkpoints, so evaluating each in its own designed
environment is more honest than forcing lever2 into an environment it wasn't
specifically trained for. This means lever2's numbers aren't perfectly
apples-to-apples with the other two's environment, though the *finding* (raw
policy drives passivity) held regardless. **This is worth you double-checking**
— if you'd rather have forced SD120 uniformly for tighter comparability, that's
a quick re-run, not a code change.

Raw evidence, all under `results/kl107-wait-diagnostic-2026-07-11/`:
`crush01-iter130-trace.jsonl` (4105 lines incl. header), `control03-iter130-
trace.jsonl` (4005 lines), `lever2-iter160-trace.jsonl` (5350 lines), plus a
`*-agg.json` aggregate file per checkpoint. **These are not committed to git**
(results/ is git-ignored per project convention) — they exist only on this
machine. Flagging in case you want them preserved somewhere more durable before
they're at risk of local cleanup.

### Result

```
checkpoint              steps  WAIT%  forced forced_lcb  chosen chosen_lcb  raw==masked_top
crush01-iter130-SD120    4104  62.0%   0.8%     0.6%      61.2%    59.7%       99.7%
control03-iter130-SD120  4004  62.5%   0.6%     0.4%      61.9%    60.4%       99.6%
lever2-iter160-SD160     5349  64.2%   0.6%     0.5%      63.6%    62.3%       99.7%
```

Reading, spelled out:

- **"forced"** = WAIT was the position's only *safe* action per the tactical
  safety model (not merely legal — an action can be legal but tactically unsafe,
  e.g. walking into an active blast radius; these are not the same check). This
  is under 1% everywhere with a tight LCB — the environment essentially never
  forces idleness.
- **"chosen"** = WAIT picked when real alternatives existed. This is 61-64% of
  **every traced step** (not just WAIT steps), Wilson floor above 59% in all
  three. This is the number that says the passivity is a real, repeated
  preference, not sampling noise, in every checkpoint tested.
- **"raw==masked_top"** = how often the network's own top-ranked raw action
  survives the safety mask unchanged. ~99.6-99.7% everywhere — the mask is
  essentially never the thing removing an aggressive top choice.

Chain of custody for the conclusion: raw policy's own top pick survives masking
~99.7% of the time → search mostly agrees with the (now-established-as-
essentially-unmasked) prior (the ~72-93% agreement figure from earlier this
session, unaffected by tonight's changes) → the final chosen action reflects
that same preference. All three links point the same direction, across three
checkpoints that differ in reward lever, LR-schedule validity, and
sudden-death timing, but share the same v6-league self-play lineage.

**What to check:** whether you agree "consistent across three checkpoints with
different confound profiles" is legitimate corroborating evidence, or whether
you'd want a fourth, cleaner data point (e.g., a fresh non-confounded checkpoint)
before trusting this for a KL-105 design decision. My read is that three
checkpoints landing within 2 points of each other despite their real
differences is meaningfully more convincing than any one of them alone, but
this is exactly the kind of claim this whole project's discipline says should
get a second set of eyes before being load-bearing.

### An operational surprise worth knowing about

lever2's run took roughly 2 hours against ~15-20 minutes for the other two. I
verified via repeated CPU-time-delta checks (the technique established earlier
in this project) that it was genuinely computing the whole time, not hung — and
the eventual data explains why: lever2's games in its SD160 environment averaged
~167 steps/match vs. crush01's ~128, i.e., games ran close to the 200-step
cap far more often. I did not interrupt it; I let it run to completion and
extended my check-in interval instead. No data was at risk either way since
`trace_log->flush()` happens after every single step, but you should know a
future evaluate run against SD160-trained checkpoints at this game count may
take a similar amount of wall-clock time.

---

## Part 5: Every judgment call I made without asking first (flat list)

For fast audit — everything here is a place I decided something myself that
reasonable people could disagree on:

1. Reading your `d6eeb4e` diff and inferring the three separate bugs it fixed,
   rather than asking you to summarize them first (Part 1).
2. Choosing Option C (recompute) over Option A (thread through
   `expand_and_backup`) for capturing raw policy/value (Part 2).
3. The exact new field names (`policy_head_raw_recomputed`, `wait_forced`,
   `search_root_q_values`, etc.) — chosen for explicitness given the project's
   history with mislabeled trace fields, not run past you.
4. Bumping `trace_format_version` 2→3 rather than treating this as additive
   without a version bump.
5. The regression test's specific thresholds: 0.05 cross-check tolerance, ≥50%
   divergent-row majority (Part 3).
6. Scope of the systematic pass: 3 checkpoints (crush01, control03, lever2), not
   also the pre-lever v6-league baseline or any others; 32 matches each, not
   more or fewer.
7. Evaluating lever2 at its native SD160 rather than forcing SD120 for
   cross-checkpoint uniformity (Part 4).
8. Letting lever2's 2-hour run complete rather than interrupting it after,
   say, 30-45 minutes of unexpected slowness.
9. Writing the KL-105 Linear update characterizing this finding as pointing at
   "the shared self-play data/curriculum" rather than some other explanation I
   didn't consider.
10. Creating this review doc as a new numbered file
    (`11-kl107-v3-audit-review.md`) rather than appending further to
    `10-autonomous-brick-execution.md`, and choosing what counts as in-scope for
    it (everything after `d6eeb4e` specifically).

---

## Part 6: What's not done

Pulled directly from the Linear KL-107/KL-105 acceptance-criteria updates, for
one clean list:

- No native viewer UI panel — trace remains JSON-export-only.
- No representative single-game win/loss/draw writeup with an identified
  tactical turning point (the systematic pass answers the same underlying
  question at the aggregate level instead, but the specific acceptance-criteria
  item as originally scoped is not done).
- Trace overhead not benchmarked when enabled (confirmed zero-cost when
  disabled; the added forward pass's cost when tracing is on is unmeasured).
- Value head and search-backup paths are not separately isolated from each
  other the way raw-policy-vs-mask now is — the raw-policy finding is strong but
  doesn't by itself rule out the value head or search having independent,
  additional issues.
- Opponent-model identity/assumption is documented as a known limitation but
  not yet a trace field.
- No KL-105 code, curriculum design, or training launch of any kind — explicitly
  out of scope per your "stop here" three turns before this window started, and
  I did not treat "let's proceed from here" as lifting that specific boundary
  (confirmed with the advisor before starting any of this work).
