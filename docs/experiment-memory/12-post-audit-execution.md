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
