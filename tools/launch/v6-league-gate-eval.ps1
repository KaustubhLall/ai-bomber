# The ONE measurement that gets to decide whether v6-league is working: eval-time
# (dirichlet noise OFF, greedy argmax action selection - see evaluate_baseline()/
# marginal_action() in src/training/native/trainer.cpp) win-cause + WAIT% against the
# native MCTS baseline. Everything else printed during training (vs-heuristic score,
# training-time win-cause under exploration noise) is a training-health signal only -
# it is exactly the class of measurement that produced the retracted v5 "superhuman"
# claim (see docs/experiment-memory/07-grokking-campaign.md Part 2, and the KL-96
# Linear issue "Update 2026-07-10" section). Do not let a vs-heuristic or
# training-time number substitute for this.
#
# Uses the standing MCTS-eval seed block (1300001) at N=32 (vs the in-loop N=4) -
# this is a mid-course gate check, NOT the final claim, so it deliberately does not
# touch the reserved final-holdout blocks (A3 = 2400001/2410001, B3 = 2500001/2510001).
# See tools/launch/README.md "Seed hygiene" for why those must stay untouched until a
# checkpoint has actually cleared this gate.
#
# Usage:
#   .\tools\launch\v6-league-gate-eval.ps1                    # checkpoint=latest.pt, N=32
#   .\tools\launch\v6-league-gate-eval.ps1 -Checkpoint iteration_000100.pt
#   .\tools\launch\v6-league-gate-eval.ps1 -MctsGames 64      # tighter Wilson LCB
#
# -Checkpoint is resolved by the native trainer relative to --run-dir (same
# convention as Resolve-Champion in _env.ps1, which returns a bare filename,
# not a joined path) - passing a path that already includes the run-dir
# prefix doubles it and fails with "checkpoint not found". This script
# strips any leading path so either form works.
param(
    [string]$Checkpoint = "latest.pt",
    [int]$MctsGames = 32,
    [int]$MctsSims = 256,
    [string]$Output = "results/alphazero-native-superhuman-v6-league/gate-eval.json"
)
. "$PSScriptRoot\_env.ps1"
Use-Torch
$Checkpoint = Split-Path -Leaf $Checkpoint

Write-Host "Gate eval: $Checkpoint vs MCTS-$MctsSims, N=$MctsGames games, noise OFF, greedy action selection."
$evalArgs = @(
    "evaluate", "--run-dir", "results/alphazero-native-superhuman-v6-league", "--checkpoint", $Checkpoint,
    "--channels", "128", "--blocks", "10",
    "--width", "13", "--height", "11", "--max-steps", "200", "--crate-density", "50",
    "--flame-duration", "2", "--sudden-death-start", "120", "--shrink-interval", "4",
    "--eval-games", "32", "--eval-simulations", "96", "--eval-seed-base", "900001",
    "--eval-mcts", "--mcts-eval-games", "$MctsGames",
    "--baseline-mcts-simulations", "$MctsSims", "--baseline-mcts-depth", "16",
    "--mcts-eval-seed-base", "1300001",
    "--output", $Output
)
& $NativeExe @evalArgs
Write-Host "`nRead the 'mcts' win-cause + WAIT% lines above (or $Output). Compare against the KL-96 child issue decision tree before drawing any conclusion."
