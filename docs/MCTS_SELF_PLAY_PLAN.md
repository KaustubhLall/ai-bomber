# MCTS and Self-Play Plan

## Why MCTS fits this simulator

AI Bomber has a small discrete action space, a fast deterministic C environment,
copyable state, and short tactical horizons dominated by bomb placement, escape,
and trapping. Those properties make repeated forward simulation practical without
coupling search to the visualizer.

## Required environment API

- Add a cheap, explicit environment clone/copy operation.
- Copy deterministic RNG state with every clone.
- Add `env_step_joint(Action agent_action, Action opponent_action)` so search can
  control both sides without hidden fallback actions.
- Add a rollout/evaluation function that depends only on the core environment.
- Keep all search APIs independent of raylib and visualizer state.
- Preserve the current `agent0_policy` plus shared `opponent_policy` model; a
  per-agent policy array can follow when multi-policy matches are needed.

## MCTS implementation

Start with single-threaded UCT search. Generate only legal actions, use random or
heuristic rollout policies, and cap both iterations/time per move and rollout
depth. Evaluate survival, enemy damage/elimination, immediate danger, crates,
powerups, and mobility. Add deterministic known-board tests for the selected
action before exposing an `AGENT_MCTS` type. Do not add a placeholder that claims
to search.

## Self-play progression

1. Establish fixed random and heuristic baselines, including heuristic versus
   heuristic and heuristic versus random.
2. Add MCTS as a teacher and evaluation baseline.
3. Train the lightweight policy/value model from generated full-simulator experience via `tools/train_alphazero.py`.
4. Maintain a checkpoint league rather than evaluating only the newest policy.
5. Evaluate every checkpoint against fixed baselines and held-out seeds.

The project now contains both a compact AlphaZero-lite reference and a
full-simulator AlphaZero-style trainer. A run is only self-play when both sides
are explicitly controlled by policies; the built-in random enemy fallback is
not self-play.
