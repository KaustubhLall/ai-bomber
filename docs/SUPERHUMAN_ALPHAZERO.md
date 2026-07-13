# Superhuman AlphaZero ladder

## ⚠️ RETRACTED 2026-07-10 — the v5/iter-210 claim below does not hold

The "Frozen candidate" and "Final proof status" sections below report W-D-L
and Wilson lower bounds only — **no win-cause classification**. An overnight
audit (Linear KL-96, "Update 2026-07-10") found that 95-97% of this
checkpoint's wins, including the A3/B3 holdout wins reported here, were
**arena-crush deaths** (`death_owner == -1`, the closing-arena wall killing
the loser), not bomb-kills. A sudden-death-OFF control against the same
checkpoint collapsed to near-all-draws, meaning there was almost no
demonstrated combat skill independent of the crush mechanic. Retroactive
win-cause classification across earlier checkpoints showed bomb-kill%
collapsing specifically at iteration 210, the promoted checkpoint — not a
gradual drift.

Root cause: `terminal_training_value` paid the same full +1.0 reward for an
arena-crush win as for a real bomb-kill, and self-play is a mirror match
(both seats = the same network), which structurally can't produce many clean
kills between two agents of identical dodge skill. The network learned the
only reliably decisive line available to it — survive to the crush window —
and the W-D-L-only gate in this document had no way to see the difference.

This is retained as a historical record of what was measured and exactly how
the gap slipped through a statistically careful-looking protocol. It is
**not** a valid superhuman claim. The fix (reward reshaping so only a
demonstrated bomb-kill pays full value, plus an opponent league) and the new
claim bar (win-cause classified, eval-time/noise-off only, SD-off control
required) are tracked in Linear KL-96 → KL-100 ("honest claim ledger"). Any
future superhuman claim for this project must clear KL-100's checklist before
being written up as fact.

## Claim boundary

This continuation targets **agent-ladder superhuman performance**: a frozen
neural checkpoint must be statistically stronger than every predeclared
non-learning agent shipped with AI Bomber, including a 512-simulation,
depth-16 native MCTS opponent. There is no human match corpus yet, so passing
this gate does not by itself prove superiority to all human Bomberman players.

## Fixed seed partitions

| Split | Seeds | Use |
| --- | --- | --- |
| random/heuristic selection | 900001 onward | frequent validation |
| neural champion arena | 1100001 onward | candidate vs `best.pt` |
| strong-MCTS selection | 1300001 onward | sparse milestone checks |
| diagnostic probe | 1310001 onward | v1 baseline diagnosis only |
| retired final holdout A | 2000001-2000064 | leaked by premature eval output; never use for final proof |
| retired final holdout A2 | 2200001-2200064 | premature eval process started; never use for final proof |
| retired final holdout B | 2100001-2100064 | premature eval process started; never use for final proof |
| final holdout A3 validation | 2400001-2400064 | active replacement random/heuristic block |
| final holdout A3 MCTS | 2410001-2410064 | active replacement MCTS-512 block |
| final holdout B3 validation | 2500001-2500064 | active independent random/heuristic block |
| final holdout B3 MCTS | 2510001-2510064 | active independent MCTS-512 replication |

The active final proof blocks are A3 and B3, each split into disjoint
random/heuristic and MCTS sub-blocks because the evaluator rejects overlapping
selection ranges. They are never evaluated until checkpoint selection is frozen.
On 2026-07-09, external premature native
evaluation processes touched the previous final blocks: seed base 2000001
produced `results/v5-holdout/test.json`, seed base 2200001 produced
`results/v5-holdout/test2.json`, and additional processes for 2200001 and
2100001 were observed and stopped. A lingering 2200001 process was stopped
again during the A3 proof run at about 12:25 PDT. To keep the claim boundary
conservative, all touched final blocks are retired.

## Starting boundary

The clean v1 iteration-70 checkpoint was probed on diagnostic seeds
1310001-1310008 with 128 neural simulations against MCTS-512/depth-16:

- random: 16-0-0, score 1.000;
- heuristic: 5-11-0, score 0.65625;
- MCTS-512: 0-16-0, score 0.500, one-sided 95% lower bound 0.309843.

The checkpoint is robust but not superior to the strengthened search baseline.
The diagnostic took 919.2 seconds, confirming that MCTS-512 belongs in sparse
milestones and final proof rather than frequent promotion. Artifact SHA-256:
`c5717d1541ca664bdd6769870b16148ab742c97506884bb20f067c9280fdf33f`.

A one-seed calibration smoke subsequently gave the native heuristic one win
and one draw against MCTS-512. This sample is far too small for a strength
claim, but it proves that MCTS-512 is not merely an invulnerable timeout bot and
that baseline budgets must be cross-calibrated on larger role-balanced blocks.
The full calibration tool and reserved diagnostic range begin at seed 1320001.

## Champion promotion gate

- clean neural search with no bootstrap heuristic;
- both seat orientations;
- random score at least 0.95;
- heuristic score no more than 0.03 below the historical champion maximum;
- 64 seed pairs (128 games) against the incumbent by default;
- challenger draw-adjusted-score one-sided 95% Wilson lower bound greater
  than 0.50;
- disjoint random/heuristic, incumbent, and MCTS seed ranges.

The gate is intentionally about monotonic champion lineage. MCTS-256/512 is too
CPU-expensive to run on every promotion and is instead a sparse outer ladder.

## Final agent-ladder superhuman gate

Freeze one checkpoint before touching either active final block. It passes only if:

1. random has no losses on holdout A3;
2. heuristic score is at least 0.65 with no catastrophic seat-specific
   regression on holdout A3;
3. against MCTS-512/depth-16, the one-sided 95% lower bound on
   `(wins + 0.5*draws)/games` is above 0.50 on holdout A3;
4. the same MCTS condition independently passes on holdout B3;
5. full native and dependency-free test suites pass, and checkpoint/config/
   result SHA-256 hashes plus exact commands are recorded.

Final JSON must include W-D-L for each learner seat, not only an aggregate.
This makes the seat-specific non-regression condition directly inspectable.

If a block fails, it becomes diagnostic data and is never reused as holdout.
Training may continue, but the next final attempt must use a newly predeclared,
untouched block.

## Frozen candidate

On 2026-07-09, v5 `best.pt` from iteration 210 passed the non-final
strong-search selection gate against MCTS-512/depth-16 on seed base 1300001:

- random: 128-0-0, score 1.000, lower bound 0.979300;
- heuristic: 127-1-0, score 0.996094, lower bound 0.972187;
- MCTS-512: 23-1-8, score 0.734375, lower bound 0.591441;
- learner seat 0 vs MCTS: 10-1-5; learner seat 1 vs MCTS: 13-0-3.

Frozen checkpoint:
`results/alphazero-native-superhuman-v5/frozen-best210-selection-mcts512.pt`
with SHA-256
`e205daccf3023e367afe6899433b744a04a8185a6ded6346cbee9db4d58149fb`.
Selection JSON SHA-256:
`f368c9db518cb57f9a6b4cd6a21a0525b4cdfc26f8dede06bff996a374bd3345`.

## Final proof status

On 2026-07-09, the frozen candidate passed final holdout A3 using schema 2
with validation seed base 2400001 and MCTS seed base 2410001:

- random: 128-0-0, score 1.000, lower bound 0.979300;
- heuristic: 121-1-6, score 0.949219, lower bound 0.906992;
- MCTS-512/depth-16: 91-5-32, score 0.730469, lower bound 0.661681;
- MCTS learner seat 0: 43-2-19; learner seat 1: 48-3-13.

A3 satisfies the predeclared random, heuristic, and MCTS lower-bound gates.
A3 JSON SHA-256:
`356314ca0d080d94573bd4ebb681160fdbb74f48870781873c697975aa420da2`.

B3 was launched only after A3 passed, using the same frozen checkpoint,
validation seed base 2500001, MCTS seed base 2510001, 64 seeds per opponent,
and MCTS-512/depth-16. The output target is
`results/alphazero-native-superhuman-v5/holdout-b3-frozen-best210-mcts512.json`.
The first B3 process stopped before writing JSON and did not leave stdout/stderr
artifacts, so B3 was relaunched through `tools/run_native_alphazero.ps1` at
about 14:15 PDT with explicit stdout/stderr logs using the same frozen
checkpoint and seed blocks. The relaunched B3 proof passed:

- random: 128-0-0, score 1.000, lower bound 0.979300;
- heuristic: 123-1-4, score 0.964844, lower bound 0.927031;
- MCTS-512/depth-16: 102-5-21, score 0.816406, lower bound 0.753772;
- MCTS learner seat 0: 54-2-8; learner seat 1: 48-3-13.

B3 JSON SHA-256:
`5347606520ebdeb0bedb93e5c45a2e4861371eb5c64607b04152d6669e35a150`.

Both final holdouts independently clear the MCTS-512 lower-bound gate. This is
an **agent-ladder superhuman** result versus the shipped random, heuristic, and
native MCTS-512/depth-16 agents under the predeclared protocol. It is not a
claim of superiority to all human Bomberman players.

Final artifact hashes:

| Artifact | SHA-256 |
| --- | --- |
| `frozen-best210-selection-mcts512.pt` | `e205daccf3023e367afe6899433b744a04a8185a6ded6346cbee9db4d58149fb` |
| `selection-best210-mcts512.json` | `f368c9db518cb57f9a6b4cd6a21a0525b4cdfc26f8dede06bff996a374bd3345` |
| `holdout-a3-frozen-best210-mcts512.json` | `356314ca0d080d94573bd4ebb681160fdbb74f48870781873c697975aa420da2` |
| `holdout-b3-frozen-best210-mcts512.json` | `5347606520ebdeb0bedb93e5c45a2e4861371eb5c64607b04152d6669e35a150` |
| `config-history.jsonl` | `be0d60ec039eabc873312938672edd239f280e18504714006c932c16f3670010` |
| `metrics.jsonl` | `95e396e02ef497144b9b25d32ada9d4f7aaf5e2819667acce5e64358de6ce391` |

Final proof commands:

```powershell
.\tools\run_native_alphazero.ps1 evaluate `
  --run-dir results\alphazero-native-superhuman-v5 `
  --checkpoint frozen-best210-selection-mcts512.pt `
  --eval-seed-base 2400001 --eval-games 64 --eval-simulations 96 `
  --eval-mcts --mcts-eval-seed-base 2410001 --mcts-eval-games 64 `
  --baseline-mcts-simulations 512 --baseline-mcts-depth 16 `
  --output results\alphazero-native-superhuman-v5\holdout-a3-frozen-best210-mcts512.json

.\tools\run_native_alphazero.ps1 evaluate `
  --run-dir results\alphazero-native-superhuman-v5 `
  --checkpoint frozen-best210-selection-mcts512.pt `
  --channels 128 --blocks 10 `
  --eval-seed-base 2500001 --mcts-eval-seed-base 2510001 `
  --eval-games 64 --eval-simulations 96 `
  --eval-mcts --mcts-eval-games 64 `
  --baseline-mcts-simulations 512 --baseline-mcts-depth 16 `
  --output results\alphazero-native-superhuman-v5\holdout-b3-frozen-best210-mcts512.json `
  --no-progress
```

Final verification:

```powershell
cmake --build build-native-gpu --config Release -j 12
ctest --test-dir build-native-gpu -C Release --output-on-failure

cmake --build build-native-c-only-audit --config Release -j 12
ctest --test-dir build-native-c-only-audit -C Release --output-on-failure
```

Results: native GPU suite 25/25 passed; dependency-free C-only suite 22/22
passed.

Completion audit:

- checkpoint frozen before final holdouts: proven by
  `frozen-best210-selection-mcts512.pt` and selection JSON hashes;
- final holdout seed hygiene: A3 and B3 used the active replacement blocks, while
  contaminated 2000001/2200001/2100001 blocks remain quarantined;
- schema and seed partition: both final JSON files are schema 2 and record
  disjoint validation and MCTS seed bases;
- A3 gate: random no losses, heuristic above floor without catastrophic seat
  regression, and MCTS-512 lower bound above 0.50;
- B3 replication gate: independently passed the same random/heuristic/MCTS
  boundary on the second active final block;
- verification: native GPU and dependency-free C-only builds/tests passed after
  B3;
- claim wording: limited to the shipped agent ladder and explicitly not a
  human-superiority claim.

## Continuation preset

Seed a new run directory by copying `latest.pt` and `best.pt` from v1. Continue
from iteration 100 with more accurate self-play search and an explicit learning
rate restart:

```powershell
.\tools\run_native_alphazero.ps1 train `
  --run-dir results\alphazero-native-superhuman-v2 --iterations 500 `
  --games 128 --simulations 96 `
  --channels 128 --blocks 10 --replay-capacity 200000 `
  --train-steps 128 --batch-size 512 `
  --learning-rate 0.0001 --min-learning-rate 0.00001 `
  --lr-schedule-start-update 12800 --lr-schedule-updates 51200 `
  --teacher-games 0 --teacher-iterations 20 `
  --eval-interval 10 --eval-games 32 --eval-simulations 96 `
  --promotion-games 64 --promotion-simulations 96 `
  --promotion-seed-base 1100001 --promotion-confidence-z 1.6448536269514722 `
  --random-score-floor 0.95 --heuristic-score-floor 0.55 `
  --heuristic-regression-margin 0.03 `
  --mcts-eval-interval 50 --mcts-eval-games 4 `
  --mcts-eval-seed-base 1300001 `
  --baseline-mcts-simulations 256 --baseline-mcts-depth 16 `
  --snapshot-interval 10 --seed 1
```

This is an adaptive campaign, not a promise that iteration 500 is sufficient.
At each 100-iteration boundary, compare validation per wall-clock hour and the
MCTS ladder. Extend iterations, search, or data diversity only when the measured
bottleneck justifies it. Resume without `--fresh`; progress bars, per-phase ETA,
atomic checkpoints, emergency checkpoints, and `config-history.jsonl` remain
the operational record.

## Live v2 campaign

The preset above started on 2026-07-08 from v1 `latest.pt` at iteration 100 and
v1 `best.pt` at iteration 70. Iteration 101 completed in 201.36 seconds with
42,158 new samples, 128 optimizer updates, a full 200,000-sample replay, and
loss 1.20397. The measured ETA to iteration 500 was about 22.5 hours. A separate
process loaded the new iteration-101 checkpoint and produced schema-2 JSON while
training continued, proving backward migration and concurrent checkpoint reads.
The exact running binary and DLL hashes are recorded beside the run in
`RUN_PROVENANCE.md`.
