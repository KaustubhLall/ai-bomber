# Evidence: KL-105 first bounded intervention — paired arm evaluation, 2026-07-12

**Verdict: FLAT/NEGATIVE per the criteria frozen at the launch gate (doc 13 section 3). The
capped cause-balanced replay lever is killed and retained as a negative result.**

Experiment: two arms forked from the same base (`control03-from102/iteration_000130.pt`,
SHA-256 `ab5771d3...`), identical explicit `--lr-schedule-updates 33280`, identical seeds,
binary, and horizon (iterations 131→154), differing in exactly one semantic field —
`replay_cause_balance_cap` 0.25 (treatment) vs 0 (control) — **verified programmatically from
the evidence files' own resolved_semantics** (the only differing key), not assumed from launch
commands. Both arms: single clean watchdog attempt, final LR identical to the digit
(7.7844e-5), full replay turnover (unknown-tag pool = 0). Same binary trained and evaluated
both arms (`17d02287ef90...`, clean stamp `b48838b5a2a7`). Design + frozen criteria: doc 13.
Full narrative incl. two operational bugs caught and fixed mid-experiment: doc 12.

## Decision table (frozen criteria, iteration 154, MCTS-256, 64 matches/arm, diagnostic block 1300001)

| | treatment (cap 0.25) | control (cap 0) | criterion | result |
|---|---|---|---|---|
| SD-on bomb-kill wins | **1**/64 | 0/64 | (a) ≥6 AND ≥control+4 | **FAIL** |
| chosen-WAIT (SD-on trace) | **67.3%** | 73.6% | (b) ≤ control − 8pp | **FAIL** (−6.3pp) |
| SD-off timeout draws | **92.2%** | 93.75% | (c) ≤ control + 5pp | PASS |
| SD-on W-D-L (score) | 36-6-22 (0.609) | 35-6-23 (0.594) | descriptive | crush-driven both (35/36 and 35/35 crush wins) |
| gates, search mode | 0/6 | 1/6 (corridor-clear) | descriptive | treatment lost the base's one pass |

SUCCESS required (a)+(b)+(c)+(d); DIRECTIONAL required (a) or (b). Neither met → FLAT/NEGATIVE.

## What the paired design actually revealed (descriptive, below the criteria bar)

*(Numbers corrected 2026-07-12 after an independent closeout caught two reporting errors and
prompted a same-binary base re-evaluation — original figures retained inline for the record.)*

1. **Continued v6-league training at a healthy LR made BOTH arms more passive than their
   shared base.** Canonical like-for-like numbers (all three checkpoints on the identical
   binary `17d02287...`, identical 32-game/64-match seed span, `base-iter130-SDon-*` in this
   directory): base chosen-WAIT **65.016%** → control **73.619% (+8.603pp)**, treatment
   **67.296% (+2.280pp)**. The originally-reported +11.7pp mixed a 16-seed base span with a
   32-seed arm span (seeds 17-32 are more passive for the base too); the interim +7.507pp was
   the 16-overlapping-seed figure. Binary sensitivity was measured, not assumed: the base's
   old-binary vs arm-binary chosen-WAIT on the same 16 seeds differs by 0.025pp (61.888% vs
   61.863%).
2. The treatment **attenuated the drift (−6.323pp vs control) but did not reverse it** —
   and the dose language is corrected: **expected** (not measured-realized) bomb-win-side
   batch content averaged **42.508% (treatment) vs 22.771% (control) over the run — 1.867×**;
   the 9.205× ratio (27.218% vs 2.957%) held only at iteration 131 while the control's pool
   was still nearly empty. The originally-quoted "~8.5×" was that first-iteration figure
   misapplied to the whole run. The enrichment moved the raw head's WAIT preference (66.1%
   argmax-WAIT vs control's ~74%) without moving kill behavior (1 vs 0 bomb wins; base itself
   scored 2/64 on this span) or deployed tactical competence (gates 0/6 vs 1/6; treatment's
   raw mode still takes the trap kill in 5 steps while search suppresses it).
3. Dose bookkeeping: the metrics field `realized_pool_a_batch_fraction` reports the FORCED
   quota only (0.25); uniform remainder draws also select pool A, so total-selected share is
   higher and the figures in (2) are expectations computed from pool sizes, not per-batch
   measurements. (A forced-vs-total split in the metrics is queued as post-battery hygiene.)
4. **Scope of the negative result:** pool A contained every frame of a bomb-winning
   trajectory — including its passive lead-up. This verdict kills *whole-trajectory* cause
   reweighting, not sampler/local-credit approaches generally (kill-local weighting was never
   tested).

## Files

Committed here: both arms' SD-on/SD-off aggregate JSONs + gates JSONs + the **same-binary base
re-evaluation** (`base-iter130-SDon-agg.json`, run 2026-07-12 on the preserved arm binary
BEFORE any post-verdict rebuild, exactly for the like-for-like drift above) + each arm's small
run artifacts under `arm-run-artifacts/{treatment,control}/` (metrics, config,
config-history, fork-manifest, semantic-fork-log, watchdog-attempts, train-console.log — the
files carrying the LR-equality, iteration-131 field-identity, champion-reset, and
single-attempt claims) + `SHA256SUMS` (one repo-root-relative convention; also covers the
local-only trace/per-match JSONLs under `results/kl105-arm-eval-2026-07-12/`). Arm run dirs
retain checkpoints locally. Reproduce: each agg's `invocation_argv`, or
`tools/launch/kl105-paired-eval.ps1`.

Wording note (corrected): the arms' iteration-131 *collection fields* (cause-pool counts,
new_samples, learning rate) are identical — full metrics rows are not bit-identical (timing
fields legitimately differ).
