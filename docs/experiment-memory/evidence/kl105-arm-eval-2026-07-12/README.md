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

1. **Continued v6-league training at a healthy LR made BOTH arms substantially more passive
   than their shared base** (chosen-WAIT 61.9% at the base → 73.6% control, 67.3% treatment).
   The control arm isolates this cleanly: +11.7pp passivity drift from 24 iterations of
   ordinary training on this data distribution at ~1e-4 LR. This is a *new* finding the
   uncontrolled historical runs could never show.
2. The treatment **attenuated the drift by 6.3pp but did not reverse it** — an ~8.5× batch
   over-representation of demonstrated-kill samples (≈47% vs ≈28% bomb-win batch content) was
   outrun by whatever the rest of the distribution teaches. It moved the raw head's WAIT
   preference (66.1% vs control's ~74% argmax-WAIT) without moving kill behavior (1 vs 0
   bomb wins) or deployed tactical competence (gates 0/6 vs 1/6; treatment's raw mode still
   takes the trap kill in 5 steps while search suppresses it — the suppression pattern
   survived the entire diet).
3. Dose note: `realized_pool_a_batch_fraction` reports FORCED draws only (0.25); total
   bomb-win-side batch content ≈ 0.25 + 0.75 × pool-share ≈ 47% by end of run, since the
   remainder samples uniformly from a buffer that is itself ~29% pool A.

## Files

Committed here: both arms' SD-on/SD-off aggregate JSONs + gates JSONs (each embeds argv,
hashes, resolved semantics incl. the cap) + `SHA256SUMS` (also covering the local-only trace
and per-match JSONLs under `results/kl105-arm-eval-2026-07-12/`). Arm run dirs
(`results/kl105-arm-{treatment,control}-from-control03-130/`) retain fork manifests, metrics,
checkpoints — local only. Reproduce: each agg's `invocation_argv`, or
`tools/launch/kl105-paired-eval.ps1`.
