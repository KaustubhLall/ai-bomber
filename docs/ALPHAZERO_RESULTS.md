# AlphaZero full-simulator results

## Native selected checkpoint

- Run: `results/alphazero-native-grokking-v1`
- Checkpoint: `best.pt` / selected at iteration 70
- SHA-256: `05176007799935d4001dd11653bd540535b26122f87dc23c2f30c8b91b26f46a`
- Model: 128-channel, 10-block residual policy/value tower, 3,250,839
  parameters, centered 17-channel 11x11 input
- Search: simultaneous-action decoupled zero-sum PUCT, 64 simulations in
  training and 96 simulations in the final holdout
- Compute: RTX 5080 BF16 batched inference, FP32 optimization, OpenMP C++ tree
  traversal

The fixed training protocol completed 100 iterations, 12,800 optimizer updates,
and 3,774,976 new samples in 4.342 summed phase-hours. The replay ring held
200,000 samples. `latest.pt` completed iteration 100; it is not substituted for
the pre-selected iteration-70 `best.pt` in the holdout.

## Untouched holdout

The final holdout used seed base 1,500,001, which was not used for self-play or
checkpoint selection. Every seed was played in both seat orientations. Results
were atomically written to `holdout-best.json`.

| Opponent | Seeds | Games | Wins | Draws | Losses | Score |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Random | 1500001-1500064 | 128 | 128 | 0 | 0 | 100.00% |
| Native heuristic | 1500001-1500064 | 128 | 41 | 87 | 0 | 66.02% |
| Native MCTS | 1500001-1500008 | 16 | 2 | 14 | 0 | 56.25% |

The selected model lost none of 272 holdout games. It converted 41 heuristic
games and two native-MCTS games into wins. The MCTS result is the most important
boundary improvement: the earlier reference checkpoint scored 37.50% with two
losses and no wins; the native model scored 56.25% with two wins and no losses.

Post-run review found two selection-protocol defects in the trainer used for
this run. Evaluation search inherited the bootstrap tactical-value blend
through iteration 20, and native promotion compared heuristic scores without a
challenger-versus-incumbent arena. Iterations 5, 10, and 20 are therefore
heuristic-aided measurements, and the promotion lineage is not an
AlphaZero-grade champion gate. The selected iteration-70 model is after the
bootstrap window and is a clean network measurement. The untouched holdout is
also clean and remains valid because it measures the frozen model directly.
Treat `best.pt` as the best measured v1 candidate, not as a statistically
proven champion.

Holdout artifact SHA-256:
`9b3f533bd574cdf2db2510ba0fcb56bf2970c4a5dda432ac926388ac6c2e852c`.

## Validation trajectory and grokking conclusion

Checkpoint selection used a separate fixed block beginning at seed 900,001.

| Iteration | Heuristic W-D-L | Score | MCTS score | Promoted |
| ---: | ---: | ---: | ---: | --- |
| 5 | 6-26-0 | 59.38% | — | yes |
| 10 | 6-26-0 | 59.38% | — | no |
| 20 | 9-23-0 | 64.06% | 50.00% | yes |
| 30 | 2-30-0 | 53.12% | — | no |
| 40 | 7-25-0 | 60.94% | 56.25% | no |
| 50 | 8-24-0 | 62.50% | — | no |
| 60 | 6-26-0 | 59.38% | 50.00% | no |
| 70 | 10-22-0 | 65.62% | — | yes |
| 80 | 10-22-0 | 65.62% | 50.00% | no |
| 90 | 9-23-0 | 64.06% | — | no |
| 100 | 9-23-0 | 64.06% | 56.25% | no |

This is **not evidence of grokking** under the documented standard. Validation
improved early, regressed, and later recovered to a modestly higher plateau; it
did not show a sharp late transition from poor generalization to a new regime.
The supported conclusion is noisy ordinary learning with a persistent late
improvement. The negative grokking result is retained because training loss
alone would have suggested a cleaner story than opponent play actually showed.

## Comparison with the dependency-light reference

The earlier NumPy checkpoint remains reproducible rather than being overwritten:

- Run: `results/alphazero-conv-main`
- Checkpoint: `best.npz` / `iteration_000045.npz`
- SHA-256: `88cc9fd61892b335d2fd76ee55449e8a726fb1b4bd2f2d1b7e895058b1fdb5f4`
- Holdout: random 31-0-1 (96.88%), heuristic 9-19-4 (57.81%), native
  MCTS 0-6-2 (37.50%).

Seed ranges and search budgets differ between the reference and native audits,
so the percentages are not a paired statistical test. They do establish the
stronger operational boundary: the native selected model was perfect against
random, lossless against both stronger opponents, and won games against native
MCTS on its predeclared holdout.

## Reproduce the native holdout

```powershell
.\tools\run_native_alphazero.ps1 evaluate `
  --run-dir results\alphazero-native-grokking-v1 `
  --checkpoint best.pt `
  --channels 128 --blocks 10 --replay-capacity 200000 --max-steps 200 `
  --eval-seed-base 1500001 --eval-games 64 --eval-simulations 96 `
  --eval-mcts --mcts-eval-games 8 `
  --baseline-mcts-simulations 96 --baseline-mcts-depth 12 `
  --output results\alphazero-native-grokking-v1\holdout-best.json
```

The evaluator rejects incompatible model/map/replay signatures, loads the
selected checkpoint directly, reports W-D-L separately, and does not rewrite
the training manifest.
