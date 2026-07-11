# Shared environment resolver for the AI Bomber launch scripts.
# Dot-source from a sibling script:  . "$PSScriptRoot\_env.ps1"
# Resolves the repo root and the best available built binaries so the desktop
# shortcuts keep working after a rebuild without editing every .lnk — edit here instead.

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

# Prefer an optimized Release build; fall back to Debug so a shortcut still works
# if only Debug is built. Throws a build hint if neither exists.
function Get-BomberExe {
    param([Parameter(Mandatory = $true)][string]$Name)  # bomber_viz | bomber_headless
    $candidates = @(
        (Join-Path $Root "build\src\Release\$Name.exe"),
        (Join-Path $Root "build\src\Debug\$Name.exe")
    )
    foreach ($c in $candidates) { if (Test-Path -LiteralPath $c) { return $c } }
    throw "$Name is not built. Run: cmake --build `"$Root\build`" --config Release --target $Name"
}

function Get-Viz      { Get-BomberExe -Name "bomber_viz" }
function Get-Headless { Get-BomberExe -Name "bomber_headless" }

# Native LibTorch trainer (owns the neural net) — used by the champion launchers.
$NativeExe       = Join-Path $Root "build-native-gpu\src\Release\bomber_alphazero_native.exe"
$TorchLib        = Join-Path $Root ".venv-gpu\Lib\site-packages\torch\lib"
# RETRACTED 2026-07-11 - kept for historical inspection only. v5's "agent-ladder
# superhuman" claim was retracted: 95-97% of its wins were arena-crush deaths, not
# bomb-kills (docs/SUPERHUMAN_ALPHAZERO.md, Linear KL-100). Not renamed (would break
# any existing desktop shortcuts pointing at champion-eval.ps1/champion-replay.ps1,
# which dot-source this file) - the retraction is instead surfaced loudly at the point
# those two scripts actually run. Active work uses v6-league-gate-eval.ps1 and its own
# -RunDir against v6-league/crush01/control03-from102, not this variable.
$ChampionRunDir  = Join-Path $Root "results\alphazero-native-superhuman-v5"
# Immutable frozen champion (iter-210); falls back to best.pt if the frozen copy is absent.
$ChampionCkpt    = "frozen-best210-selection-mcts512.pt"
$ReplayDir       = Join-Path $Root "results\replays"

function Resolve-Champion {
    $frozen = Join-Path $ChampionRunDir $ChampionCkpt
    if (Test-Path -LiteralPath $frozen) { return $ChampionCkpt }
    return "best.pt"
}

function Use-Torch {
    if (-not (Test-Path -LiteralPath $NativeExe)) {
        throw "Native trainer not built: $NativeExe (build-native-gpu, Release)."
    }
    if (-not (Test-Path -LiteralPath $TorchLib)) {
        throw "LibTorch runtime not found: $TorchLib"
    }
    $env:PATH = "$TorchLib;$env:PATH"
}

function Ensure-ReplayDir {
    if (-not (Test-Path -LiteralPath $ReplayDir)) {
        New-Item -ItemType Directory -Path $ReplayDir -Force | Out-Null
    }
}
