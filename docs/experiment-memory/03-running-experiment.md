# 03 — The Running Experiment

_Last updated: 2026-07-08 05:34 (completed)._

> 🏁 **STATUS: COMPLETED CLEANLY at ~05:33** — all 100/100 iterations, natural finish (no
> `emergency.pt`, empty stderr, 100 metrics rows). Process 14104 is gone. Final model =
> `best.pt` (iteration 70, clean eval 0.656 vs heuristic). Outcome: plateau vs heuristic
> (~0.64–0.66, 0 losses), draws vs MCTS — ordinary learning, **no grokking**. To restart/
> extend, re-run the command below with a higher `--iterations` and **no `--fresh`**.

## Identity

| | |
| --- | --- |
| Binary | `build-native-gpu\src\Release\bomber_alphazero_native.exe` |
| PID | **14104** (verify — a resume gets a new PID) |
| Started | 2026-07-08 **01:58:46** local (this is a **resumed** process; see below) |
| Run dir | `results\alphazero-native-grokking-v1` |
| Linear | KL-94 (AI-BOMBER-16) |
| git sha | `760c51e6adc9` (branch `polish/viz-and-policy-clarity`, matches config.json) |
| torch | 2.12.1, CUDA, BF16 inference / FP32 optimization, RTX 5080 |

### Exact command

```
bomber_alphazero_native.exe train --run-dir results\alphazero-native-grokking-v1 \
  --iterations 100 --games 128 --simulations 64 --train-steps 128 --batch-size 512 \
  --replay-capacity 200000 --channels 128 --blocks 10 \
  --teacher-games 32 --teacher-iterations 20 \
  --eval-interval 10 --eval-games 16 --eval-simulations 64 \
  --mcts-eval-interval 20 --mcts-eval-games 4 \
  --snapshot-interval 10 --max-steps 200 --seed 1 --no-progress
```

`--no-progress` ⇒ `native.stdout.log`/`native.stderr.log` stay empty; **`metrics.jsonl`
is the source of truth.**

## This is a resumed run (recovery worked)

`iteration_000010.pt` was written **00:08**, before the current process started at
**01:58** → an earlier run reached iteration 10, then this process **auto-resumed** from
`latest.pt` and continued. Evidence in `metrics.jsonl`: eval cadence changes at iteration
10 — iters **5 & 10** carry evals (earlier run used `--eval-interval 5`), iters **20, 30,
40** carry evals (current run uses `--eval-interval 10`), and MCTS evals appear at 20 & 40
(`--mcts-eval-interval 20`). The config **signature** only pins map/model/replay-capacity,
so resuming with a different eval cadence / teacher / LR / iteration target is allowed —
which is why metrics semantics shift mid-file. Not a bug; just read the cadence carefully.

## Config snapshot (`results/alphazero-native-grokking-v1/config.json`)

Map 13×11, max_steps 200, crate_density 50. Obs 17ch×11×11, 6 actions. Model 128ch /
10 blocks / **3,250,839 params**. iterations 100, self_play_games 128, simulations 64,
train_steps 128, batch_size 512, replay_capacity 200000. LR 2e-4 → min 2e-5 (cosine),
weight_decay 1e-4, c_puct 1.5, dirichlet α0.3 frac0.25, temperature 1 for 30 steps,
teacher_games 32 / teacher_iterations 20, bootstrap_value_weight 0.75 over 30 iters,
eval_interval 10 / eval_games 16 / eval_sims 64 / **eval_seed_base 900001**,
mcts_eval_interval 20 / mcts_eval_games 4, snapshot_interval 10, seed 1.

## Seed partitions actually used by this run

| Purpose | Formula / range | Overlap? |
| --- | --- | --- |
| Self-play | `seed·100000007 + (iter+1)·100003 + game` (~1e8–1e9) | distinct |
| Teacher | `seed·1000003 + (iter+1)·10007 + game` (~1e6) | distinct |
| Selection eval (random/heuristic/MCTS) | `900001 + seed_index` → **900001–900016** | distinct |
| Modeled opponent during eval-search | `700001 + match_index` | distinct |

The **in-training eval reuses 900001–900016 every eval iteration** — that is the fixed
selection/validation block. A **final holdout must use a different, pre-registered range**
(the prior NumPy run used 150001–150016 and 160001–160004; for this run pick e.g. 1500001)
run *after* selection freezes. Do not reuse 900001–900016 as a holdout.

## Metrics trajectory (through iteration 49)

| iter | loss | value_loss | entropy | vs random | vs heuristic (W-D-L, score) | vs MCTS | promoted |
| ---: | ---: | ---: | ---: | --- | --- | --- | --- |
| 1 | 1.409 | 0.200 | 1.246 | – | – | – | – |
| 5 | 1.235 | 0.083 | 1.152 | 32-0-0 (1.00) | 6-26-0 (0.594) | – | ✔ best 0.594 |
| 10 | 1.222 | 0.055 | 1.165 | 32-0-0 (1.00) | 6-26-0 (0.594) | – | – |
| 20 | 1.251 | 0.040 | 1.209 | 32-0-0 (1.00) | 9-23-0 (**0.641**) | 0-8-0 (0.500) | ✔ best 0.641 |
| 30 | 1.236 | 0.033 | 1.202 | 32-0-0 (1.00) | 2-30-0 (0.531) | – | – |
| 40 | 1.234 | 0.038 | 1.195 | 32-0-0 (1.00) | 7-25-0 (0.609) | 1-7-0 (0.563) | – |
| 50 | 1.203 | 0.034 | 1.169 | 32-0-0 (1.00) | 8-24-0 (0.625) | – | – |
| 60 | 1.183 | 0.030 | 1.153 | 32-0-0 (1.00) | 6-26-0 (0.594) | 0-8-0 (0.500) | – |
| 70 | 1.199 | 0.036 | 1.163 | 32-0-0 (1.00) | 10-22-0 (**0.656**) | – | ✔ best 0.656 (clean) |
| 80 | 1.198 | 0.035 | 1.162 | 32-0-0 (1.00) | 10-22-0 (0.656) | 0-8-0 (0.500) | – |
| 90 | 1.217 | 0.038 | 1.179 | 32-0-0 (1.00) | 9-23-0 (0.641) | – | – |
| 100 | 1.205 | 0.036 | 1.170 | 32-0-0 (1.00) | 9-23-0 (0.641) | 1-7-0 (0.563) | – (final; best=iter70) |

Clean heuristic evals (iter ≥ 30, bootstrap weight 0): 0.531 → 0.609 → 0.625 → 0.594 →
**0.656** — slow genuine upward drift, still no grokking *jump*. MCTS stays all-draws.

**Reading it:**
- **Random**: perfect 32-0-0 every eval — solidly clears the 90% floor.
- **Heuristic**: never *loses* (0 losses each eval) but wins only a handful; score drifts
  up slowly with noise. Bootstrap-aided 0.641 at iter 20; **clean best 0.65625 at iter 70**
  (10-22-0). Plateau-ish, heavy draws, no grokking jump.
- **MCTS**: all/mostly draws (0-8-0 then 1-7-0). Survives MCTS, rarely beats it — matches
  the documented "MCTS is the upper boundary."
- **Losses** decline cleanly (value_loss 0.20→0.03); policy entropy drifts down slowly.
- **Self-play** is draw-heavy and increasingly so (draws 48→97 across iters); W≈L by the
  symmetry of two-seat self-play — **not** a strength signal.

**Interpretation:** so far this is *ordinary learning that has plateaued against the
heuristic*, **not** a grokking transition. Per the playbook, a credible late-generalization
claim needs a flat-then-jump on a fixed validation block confirmed across later
checkpoints and an untouched holdout — not yet observed.

**⚠️ Audit caveat (finding #1, see [04-code-audit.md](04-code-audit.md)):** the tactical
heuristic used to warm up search **leaks into the evaluation search** while `iteration < 30`
(weight `0.75·(1 − iter/30)`). So the eval-iter 5/10/20 heuristic scores are
network+heuristic hybrids: weight 0.625 / 0.50 / **0.25**. The iter-20 promotion
(best_score 0.640625) was thus heuristic-aided. **Update (iter 70):** a **clean** eval
(bootstrap weight 0 for iter≥30) scored **0.656** and re-promoted, so the current `best.pt`
is the iter-70 model and is no longer confounded. Still trust only iter ≥ 30 evals; the
iter-20 `best.pt` and iters ≤ 25 scores were heuristic-aided. Re-evaluate any early
checkpoint through a bootstrap-disabled path for an honest number.

## Timing / ETA

Non-eval iterations ≈ 110–145 s; eval iterations (every 10th) ≈ 380–460 s; MCTS-eval
iterations (20, 40, …) longest. From iteration 49 at ~03:16, the remaining ~51 iterations
should finish around **~05:15–05:30 local**. Snapshots exist at 10/20/30/40; expect
50/60/70/80/90/100. `best.pt` currently = the iteration-20 model.

## How to check health (safe, read-only)

```powershell
# 1. Process alive + actively computing (CPU should climb between two samples)
Get-Process -Id 14104 | Select Id,CPU,@{N='WS_MB';E={[math]::Round($_.WorkingSet64/1MB,1)}},StartTime

# 2. Latest iteration + freshness (mtime should be < ~10 min old while running)
$d='C:\Users\kaust\IdeaProjects\ai-bomber\results\alphazero-native-grokking-v1'
(Get-Item "$d\metrics.jsonl").LastWriteTime
Get-Content "$d\metrics.jsonl" -Tail 1

# 3. Checkpoints advancing, no orphan .tmp
Get-ChildItem $d\*.pt,$d\*.tmp | Sort LastWriteTime | Select LastWriteTime,Name
```

**Healthy** = PID alive, CPU rising, `metrics.jsonl` mtime fresh, last iteration
increasing, no lingering `*.tmp`, losses finite & not diverging.
**Completed** = PID gone **and** last metrics iteration == 100 (normal exit, not a crash).
**Trouble** = PID gone with last iteration < 100 (check `native.stderr.log`, then resume
by re-running the exact command **without `--fresh`**); or metrics mtime stale (>~15 min)
while PID alive (possible hang — inspect threads/GPU).
