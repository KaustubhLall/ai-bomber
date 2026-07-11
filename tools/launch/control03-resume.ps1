# Resume control03-from102 (the matched control for the crush01 KL-98 lever-1 test - see
# control03-bootstrap.ps1 for what this run is and why) under the auto-restart watchdog.
# Ordinary resume only - no --fresh, no --fork-from, which would re-fork from the parent and
# discard all progress on every watchdog restart. Run control03-bootstrap.ps1 first, once.
#
# Usage:
#   .\tools\launch\control03-resume.ps1                  # resume toward iteration 130
#   .\tools\launch\control03-resume.ps1 -Iterations 200  # extend further after the gate
param(
    [int]$Iterations = 130
)
. "$PSScriptRoot\_env.ps1"

$trainerArgs = @(
    "--run-dir", "results/alphazero-native-superhuman-control03-from102",
    "--iterations", "$Iterations",
    "--width", "13", "--height", "11", "--max-steps", "200", "--crate-density", "50",
    "--flame-duration", "2", "--sudden-death-start", "120", "--shrink-interval", "4",
    "--channels", "128", "--blocks", "10", "--games", "128", "--simulations", "96",
    "--train-steps", "128", "--batch-size", "512", "--replay-capacity", "200000",
    "--learning-rate", "0.0002", "--min-learning-rate", "0.00001",
    "--weight-decay", "0.0001", "--c-puct", "1.5", "--dirichlet-alpha", "0.3", "--dirichlet-fraction", "0.25",
    "--temperature", "1", "--temperature-steps", "30",
    "--teacher-games", "32", "--teacher-iterations", "20",
    "--eval-interval", "10", "--eval-games", "32", "--eval-simulations", "96", "--eval-seed-base", "900001",
    "--promotion-games", "64", "--promotion-simulations", "96", "--promotion-seed-base", "1100001",
    "--promotion-margin", "0", "--random-score-floor", "0.95", "--heuristic-score-floor", "0.55", "--heuristic-regression-margin", "0.03",
    "--mcts-eval-interval", "50", "--mcts-eval-games", "4", "--baseline-mcts-simulations", "256", "--baseline-mcts-depth", "16",
    "--mcts-eval-seed-base", "1300001",
    "--snapshot-interval", "10", "--seed", "1",
    "--timeout-draw-value", "-0.5", "--mutual-death-value", "-0.2",
    "--selfkill-win-value", "0.3", "--league-heuristic-fraction", "0.5"
    # Deliberately NOT --arena-crush-win-value - inherited from the checkpoint on every resume.
)

& "$PSScriptRoot\watchdog-train.ps1" -TrainerArgs $trainerArgs
