# KL-98 reserve lever 1: arena-crush-win-value 0.3 -> 0.1. Branched from v6-league's
# iteration-102 checkpoint (results/alphazero-native-superhuman-v6-league/latest.pt,
# copied into this run's own directory) after the KL-97 iter-100 eval-time gate FAILED
# (vs MCTS-256: 24W-6D-34L score=0.4, bomb-kill only 2/64=3.1% of games, WAIT 75.0% -
# worse than the iteration-50 read, not better). See KL-97's "RESULT" section for the
# full numbers and KL-98 for why this lever is first (cheapest, CLI-only, no rebuild).
#
# Every flag below is IDENTICAL to v6-league-resume.ps1 except --run-dir and
# --arena-crush-win-value - this is a single-variable change so a re-run of the KL-97
# gate against this run's checkpoints can be attributed to this lever alone. Caveat
# (recorded honestly, not hidden): the copied starting checkpoint's replay buffer still
# contains samples labeled under the OLD 0.3 value; it dilutes as new self-play/league
# games get appended, not instantaneous. Not laboratory-clean, but the pragmatic
# tradeoff given overnight throughput - a from-scratch retrain per lever is ~12-14h each.
#
# Usage:
#   .\tools\launch\v6-league-crush01-resume.ps1                  # resume toward iteration 200
#   .\tools\launch\v6-league-crush01-resume.ps1 -Iterations 150  # re-check KL-97 gate sooner
#
# IMPORTANT - one bomber_alphazero_native.exe at a time. The original v6-league
# training was stopped (not killed-and-forgotten - its checkpoint is intact and
# untouched at results/alphazero-native-superhuman-v6-league/latest.pt, iteration 102)
# specifically to free the GPU for this run. Do not run both simultaneously.
param(
    [int]$Iterations = 200
)
. "$PSScriptRoot\_env.ps1"

$trainerArgs = @(
    "--run-dir", "results/alphazero-native-superhuman-v6-league-crush01",
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
    "--arena-crush-win-value", "0.1", "--selfkill-win-value", "0.3", "--league-heuristic-fraction", "0.5"
)

& "$PSScriptRoot\watchdog-train.ps1" -TrainerArgs $trainerArgs
