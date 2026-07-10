# Live single-match policy arena (opens the matchup picker). GUI.
. "$PSScriptRoot\_env.ps1"
& (Get-Viz) --matchup --fps 1 --render-fps 60 --start-paused
