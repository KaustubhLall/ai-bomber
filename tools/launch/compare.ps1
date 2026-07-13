# Policy comparison grid: MCTS vs heuristic / greedy / alpha-beta, both seats. GUI.
. "$PSScriptRoot\_env.ps1"
& (Get-Viz) --view compare `
    --matchup mcts:heuristic --matchup heuristic:mcts `
    --matchup mcts:greedy --matchup greedy:mcts `
    --matchup mcts:alpha-beta --matchup alpha-beta:mcts `
    --epochs 20 --fps 3 --render-fps 60
