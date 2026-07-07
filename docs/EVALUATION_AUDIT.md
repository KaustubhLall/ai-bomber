# Adversarial evaluation audit

## Evidence standard

A win counts as combat evidence only when the opponent's `death_owner` is the
evaluated policy. Reports separately retain owned eliminations, self-kills,
opponent self-kills, and opponent kills. Every promotion run uses both spawn
orientations. Battle mode has no survival reward, penalizes timeouts, and ends
after 200 steps.

Heuristic and greedy now use the same adversarial bomb-survival proof as MCTS.
They may place a bomb only when they can survive every legal immediate opponent
reply, and they follow a robust escape action while their bomb is active. In a
40-game development check (`seed 28001`) they recorded zero self-kills;
heuristic earned 18 real eliminations against greedy. They are therefore useful
opponents rather than sources of donated wins. The bomb-free evasive and
alpha-beta policies remain draw/safety stress tests.

## Candidate policy

MCTS is an open-loop UCT search with 96 simulations and rollout depth 12. It
uses adversarial opponent replies, causal progress features, upgrade-aware
tactical rollouts, repetition avoidance, and a separate robust execution-time
bomb/escape proof. Exact simultaneous player swaps are resolved atomically, so
head-on movement cannot create a permanent engine body-lock.

## Untouched holdout: seeds 40001-40010

Commands used each policy pair in both role orientations:

```powershell
build-codex-vs/src/Release/bomber_headless.exe --mode battle --agent mcts --enemy greedy --episodes 10 --seed 40001
build-codex-vs/src/Release/bomber_headless.exe --mode battle --agent greedy --enemy mcts --episodes 10 --seed 40001
```

Equivalent paired commands were run for `alpha-beta` and `heuristic`. Results:

| Opponent | MCTS owned eliminations | Opponent eliminations | Timeouts | MCTS self-kills | Opponent self-kills |
|---|---:|---:|---:|---:|---:|
| safe greedy | 11/20 (55%) | 0/20 | 9/20 | 0 | 0 |
| bomb-free alpha-beta | 1/20 (5%) | 0/20 | 19/20 | 0 | 0 |
| safe heuristic | 0/20 | 0/20 | 20/20 | 0 | 0 |

MCTS is now a competent, active combat policy against safe greedy: its 55%
holdout elimination rate contains no self-kills or donated wins. It is not a
dominant policy. Alpha-beta usually forces a draw, and safe heuristic remains
the top curriculum rung: MCTS did not beat it on this holdout. Across the
controlled MCTS orientation it waited only 0.2-0.4% of actions, so the draw
result is not an idle-policy artifact.

Seed 40004 is the deterministic visual proof: MCTS beats safe greedy in 41
steps with one owned elimination, zero waits, zero self-kills, and zero opponent
self-kills. The replay, CSV trace, JSON metrics, and one-click launcher are
`results/mcts-safe-greedy-win.*` and `shortcuts/Watch MCTS Causal Win.cmd`.

## Next promotion target

Do not claim mastery or train longer against random. The next policy must keep
zero self-kills and at least the safe-greedy result while earning causal wins
against safe heuristic on a validation seed set, then repeat on a new untouched
holdout. A sampled curriculum should use safe greedy, safe heuristic, defensive
alpha-beta, frozen MCTS checkpoints, and only then self-play.
