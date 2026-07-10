# MCTS vs heuristic demo match. GUI.
. "$PSScriptRoot\_env.ps1"
& (Get-Viz) --agent mcts --enemy heuristic --agents 2 --fps 1 --render-fps 60 --start-paused
