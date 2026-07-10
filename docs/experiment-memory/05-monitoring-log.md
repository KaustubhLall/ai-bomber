# 05 — Overnight Monitoring Log

Hourly health checks for the native run (KL-94, PID 14104, run-dir
`results/alphazero-native-grokking-v1`). Target: 5 checks, ~1 hour apart. Newest at top.

## Latest status: 🏁 COMPLETED CLEANLY — 100/100 iterations · all 5 checks done

Monitoring complete: checks #1 HEALTHY (03:21, iter 49) → #2 HEALTHY (04:34, iter 79) →
#3 CLEAN COMPLETION (05:34, iter 100) → #4/#5 post-completion stable (06:34/07:34).
The run finished normally at ~05:33 (no `emergency.pt`, empty stderr, exactly 100 metrics
rows, `iteration_000100.pt` + `latest.pt` written) and its artifacts stayed untouched for
2+ h afterward (no restart/corruption). **Final model = `best.pt` (iteration 70, clean eval
0.656 vs heuristic).** Outcome: plateau vs heuristic (~0.64–0.66, 0 losses), draws vs MCTS
— **ordinary learning, no grokking**. Honestly establishes the next bottleneck rather than
a strength jump. Optional next step: a final untouched holdout (`--eval-seed-base 1500001`).

---

### Check #5 — 2026-07-08 07:34 local — 🏁 stable (final, post-completion)

Probe: `proc=DEAD cpu=0 iter=100/100 metrics_age_min=121 tmp=0`. Process gone; run-dir
unchanged since the 05:33 finish (metrics 2 h old); no orphan `.tmp`. Fifth and final
check — the run is cleanly completed and its artifacts are intact. Monitoring closed.

---

### Check #4 — 2026-07-08 06:34 local — 🏁 stable (post-completion)

Probe: `proc=DEAD cpu=0 iter=100/100 metrics_age_min=61 tmp=0`. Process still gone; run-dir
artifacts unchanged since the 05:33 finish (metrics 61 min old = no restart, no late
writes); no orphan `.tmp`. Nothing to act on — the run stays cleanly completed.

---

### Check #3 — 2026-07-08 05:34 local — 🏁 COMPLETED CLEANLY

Probe: `proc=DEAD cpu=0 iter=100/100 metrics_age_min=1 tmp=0`

| Signal | Value | Verdict |
| --- | --- | --- |
| Process 14104 | gone | ✅ expected (finished) |
| Last iteration | **100 / 100** (target reached) | ✅ clean completion |
| metrics.jsonl | exactly 100 well-formed rows, final row has full random/heuristic/MCTS evals | ✅ |
| `emergency.pt` | absent | ✅ not interrupted (natural finish) |
| stderr | 0 bytes | ✅ no errors |
| Checkpoints | `iteration_000100.pt` + `latest.pt` @ 05:33:27; `best.pt` = iter 70 | ✅ |
| Orphan `.tmp` | none | ✅ |
| Final eval (iter 100, clean) | random 32-0-0 (1.00) · heuristic 9-23-0 (0.641, 0 L) · MCTS 1-7-0 (0.563) | ✅ |
| best_score | 0.65625 (iter 70) | — |

**Verdict:** healthy natural completion of all 100 iterations. During the run it also
auto-resumed once (iter 10) — recovery path proven in practice. The distinguishing check
(clean finish vs near-100 crash) passed on every signal: 100 rows, no emergency file, empty
stderr, final snapshot written. No action required; an *optional* next step is a final
untouched holdout via `evaluate --eval-seed-base 1500001` (distinct from the 900001
selection block) — see [04-code-audit.md](04-code-audit.md) finding #11.

---

### Check #2 — 2026-07-08 04:34 local — ✅ HEALTHY (+ mildly encouraging)

Probe: `proc=ALIVE cpu=177572 iter=79/100 metrics_age_min=1 tmp=0`

| Signal | Value | Verdict |
| --- | --- | --- |
| Process | PID 14104 alive, CPU 110,806 → 177,572 s since check #1 | ✅ actively computing |
| Progress | iter 55 (03:34) → **79/100** (04:34) ≈ 24 iters/hr (~2.5 min/iter) | ✅ on pace |
| metrics freshness | 1 min old | ✅ |
| Orphan `.tmp` | none | ✅ |
| Losses | total ~1.19, value ~0.033; LR decayed to 3.9e-5 (→ min 2e-5) | ✅ sane |
| Clean heuristic evals (iter≥30) | 30:0.531 · 40:0.609 · 50:0.625 · 60:0.594 · **70:0.656 ✔promoted** | ✅ slow upward drift |
| MCTS eval (iter 60) | 0-8-0 (0.500) | ✅ draws (upper boundary holds) |
| best_score | **0.65625** (iter 70, clean eval, 10-22-0 vs heuristic, 0 losses) | ✅ confound resolved for current best |
| Self-play | draw-heavy & rising (draws up to 100/128 at iter 79); W≈L | ✅ expected (symmetric) |

**Notes:** The iter-70 promotion is a **clean** eval (bootstrap weight 0 for iter≥30), so
the current `best.pt` is no longer the confounded iter-20 model — good. Still no grokking
*jump*; this is slow, genuine improvement that now legitimately beats the heuristic
(0 losses across 32 games) while drawing MCTS. ETA ~05:25. No action needed.

---

### Check #1 — 2026-07-08 03:21 local — ✅ HEALTHY

| Signal | Value | Verdict |
| --- | --- | --- |
| Process | PID 14104 alive, 39 threads, WS 2.5 GB | ✅ |
| Actively computing | CPU 93,600 s → 95,332 s across two samples (~2 min apart) | ✅ progressing |
| Wall since start | 1.37 h (started 01:58:46) | ✅ |
| Latest iteration | 49 / 100 | ✅ |
| metrics.jsonl freshness | last write 03:16:44 (4.3 min ago) | ✅ fresh |
| Checkpoints | latest.pt 03:16, snapshots 10/20/30/40, best.pt = iter 20 | ✅ |
| Orphan `.tmp` | none | ✅ |
| Losses | total 1.21, value 0.031 (declining) | ✅ sane |
| Random eval | 32-0-0 (1.00) at last eval (iter 40) | ✅ clears 90% floor |
| Heuristic eval | best 0.641 (iter 20), 0.609 at iter 40 | ⚠️ plateaued (not a health issue) |
| MCTS eval | 1-7-0 (0.563) at iter 40 | ✅ competitive/draws |

**Notes:** Confirmed the run **auto-resumed** from an iter-10 checkpoint (recovery path
works). No errors; stderr/stdout empty by design (`--no-progress`). Only non-health
observation is the heuristic-score **plateau** — an experiment-outcome finding (no
grokking yet), not a health problem. No action needed.

<!-- Append check #2..#5 below as they happen. Keep the "Latest status" line at top in sync. -->
