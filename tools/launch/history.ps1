# Match History / replay viewer — opens the latest recorded match. GUI.
# (Use champion-replay.ps1 / mcts-selfplay.ps1 to load a specific replay file.)
. "$PSScriptRoot\_env.ps1"
& (Get-Viz) --history --fps 1 --render-fps 60 --start-paused
