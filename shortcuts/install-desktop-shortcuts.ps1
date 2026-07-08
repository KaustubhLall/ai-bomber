$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$exe = Join-Path $root 'build-codex-vs\src\Release\bomber_viz.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "Build not found: $exe" }
$desktop = [Environment]::GetFolderPath('Desktop')
$shell = New-Object -ComObject WScript.Shell
$items = @(
    @{ Name='AI Bomber - MCTS'; Args='--agent mcts --enemy heuristic --agents 2 --fps 1 --render-fps 60 --start-paused' },
    @{ Name='AI Bomber - Alpha Beta'; Args='--agent alpha-beta --enemy heuristic --agents 2 --fps 1 --render-fps 60 --start-paused' },
    @{ Name='AI Bomber - Policy Comparison'; Args='--view compare --matchup mcts:heuristic --matchup heuristic:mcts --matchup mcts:greedy --matchup greedy:mcts --matchup mcts:alpha-beta --matchup alpha-beta:mcts --epochs 20 --fps 3 --render-fps 60' },
    @{ Name='AI Bomber - Live Policy Arena'; Args='--matchup --fps 1 --render-fps 60 --start-paused' },
    @{ Name='AI Bomber - Match History'; Args='--history --fps 1 --render-fps 60 --start-paused' }
    @{ Name='AI Bomber - MCTS Causal Win'; Args='--replay "results\mcts-safe-greedy-win.bin" --fps 3 --render-fps 60' }
)
foreach ($item in $items) {
    $shortcut = $shell.CreateShortcut((Join-Path $desktop ($item.Name + '.lnk')))
    $shortcut.TargetPath = $exe
    $shortcut.Arguments = $item.Args
    $shortcut.WorkingDirectory = $root
    $shortcut.Description = $item.Name
    $shortcut.Save()
}
Write-Output "Created $($items.Count) desktop shortcuts in $desktop"
