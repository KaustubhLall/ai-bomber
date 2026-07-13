# Resume the v6-league AlphaZero run (reward fix: arena-crush/self-kill wins devalued to
# 0.3 vs a demonstrated bomb-kill's 1.0 - see docs/experiment-memory/07-grokking-campaign.md
# "Part 2" for the full diagnosis; opponent league: 50% of self-play games vs the heuristic
# agent instead of a network mirror, since mirror self-play structurally can't produce clean
# kills). Runs under the auto-restart watchdog (tools/launch/watchdog-train.ps1) so a crash
# (two of these happened overnight 2026-07-10, no in-app error trace either time - see the
# campaign log for the incident writeups) auto-resumes instead of silently stalling.
#
# Usage:
#   .\tools\launch\v6-league-resume.ps1                  # resume toward iteration 200
#   .\tools\launch\v6-league-resume.ps1 -Iterations 300  # extend the target further
#
# To check progress while it runs: tail metrics.jsonl, e.g. in PowerShell -
#   Get-Content results\alphazero-native-superhuman-v6-league\metrics.jsonl -Tail 1 | ConvertFrom-Json
# or watch the console output directly - it prints a self-play/league-heuristic win-cause
# line (bomb/selfkill/crush breakdown + WAIT%) every iteration.
#
# To stop: close the window, or Ctrl+C (checkpoints after every completed iteration either
# way, so nothing is lost beyond the single in-flight iteration).
#
# IMPORTANT - before running: check Task Manager / `tasklist` for any other
# bomber_alphazero_native.exe already running (including from another Claude Code / terminal
# window against this same repo - this is what caused tonight's "phantom" crashes: an
# unrelated leftover script was silently launching its own trainer instances in the
# background). Running two training processes concurrently on one GPU is the one thing that
# reliably breaks this setup.
param(
    [int]$Iterations = 200
)
. "$PSScriptRoot\_env.ps1"

$trainerArgs = @(
    "--run-dir", "results/alphazero-native-superhuman-v6-league",
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
    "--arena-crush-win-value", "0.3", "--selfkill-win-value", "0.3", "--league-heuristic-fraction", "0.5"
)

& "$PSScriptRoot\watchdog-train.ps1" -TrainerArgs $trainerArgs
