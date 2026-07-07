# Adversarial evaluation audit

## What counts as evidence

A headline win rate is not enough. Every benchmark row now separates owned
eliminations, self-eliminations, opponent self-eliminations, and eliminations
caused by the opponent. Matrix summaries combine both spawn/role orientations.
This prevents a passive policy from receiving combat credit when its opponent
simply blows itself up.

Battle reward also no longer pays a per-step survival bonus. Timeouts carry a
-1 penalty, prolonged waiting carries a larger penalty, elimination reward is
only granted to the bomb owner, and a battle ends after 200 steps.

## July 7, 2026 audit run

Command:

```powershell
build-codex-vs/src/Release/bomber_benchmark.exe --matrix --episodes 20 --seed 9001 --suite post-audit --output results/post-audit-20.json
python tools/summarize_matrix.py results/post-audit-20.json
```

This is a 20-episode-per-orientation diagnostic run, not a final statistical
claim. In the role-balanced results MCTS went 30-8-2 against heuristic and
39-1-0 against greedy. However, MCTS produced only 2 owned eliminations in the
40 heuristic games and 3 in the 40 greedy games. MCTS-vs-MCTS and
MCTS-vs-alpha-beta timed out in all 40 role-balanced games. Alpha-beta also
produced no owned eliminations in the inspected matrix.

The meaningful conclusion is negative: current MCTS is a safer root-UCB rollout
planner, but it is not yet a strong offensive Bomberman policy. Its high win
rate against the older baselines is still dominated by opponent self-destruction.
No “trained”, “strong”, or superhuman claim is justified.

## Best next experiment

Do not train longer in the compact NumPy reference arena and assume transfer.
The next useful model work is a full-C-environment policy/value adapter trained
against a sampled opponent pool (heuristic, greedy, MCTS, frozen checkpoints,
then self-play). Promotion should require all of the following on untouched
role-balanced seeds:

1. higher owned-elimination rate, not just win rate;
2. lower self-elimination and timeout rates;
3. improvement against at least heuristic and greedy without regression against
   frozen prior checkpoints;
4. deterministic replay validation for sampled evaluation games.
