# Phase 1b (KL-107 v4): clean-binary diagnostic re-run across four configs, sequential
# (single native-GPU-process rule - each call must fully exit before the next starts).
# Supersedes the dirty-binary evidence at results/kl107-wait-diagnostic-2026-07-11/ (v3,
# stamped git_commit d6eeb4ea30d3-dirty) with a clean-HEAD v4 binary that adds
# opponent_modeled_as/learner_moved. Output dir is intentionally distinct so neither set of
# evidence silently overwrites the other.

$ErrorActionPreference = "Stop"
$env:PATH = "$PWD\.venv-gpu\Lib\site-packages\torch\lib;$env:PATH"
$exe = "build-native-gpu\src\Release\bomber_alphazero_native.exe"
$outDir = "results/kl107-wait-diagnostic-v4-2026-07-11"

$common = @(
  "--channels","128","--blocks","10","--width","13","--height","11","--max-steps","200",
  "--crate-density","50","--flame-duration","2","--shrink-interval","4",
  "--eval-games","2","--eval-simulations","96","--eval-seed-base","900001",
  "--eval-mcts","--mcts-eval-games","16","--mcts-eval-seed-base","1300001",
  "--baseline-mcts-simulations","256","--baseline-mcts-depth","16",
  "--no-progress","--overwrite-evidence"
)

function Run-Config($label, $runDir, $checkpoint, $sdStart, $extra) {
    Write-Output "=== [$([DateTime]::Now.ToString('HH:mm:ss'))] starting $label ==="
    $args = @("evaluate","--run-dir",$runDir,"--checkpoint",$checkpoint,
              "--sudden-death-start",$sdStart) + $common + $extra + @(
              "--trace-output","$outDir/$label-trace.jsonl",
              "--output","$outDir/$label-agg.json")
    $t0 = Get-Date
    & $exe @args
    if ($LASTEXITCODE -ne 0) { throw "$label FAILED with exit code $LASTEXITCODE" }
    $t1 = Get-Date
    Write-Output "=== [$([DateTime]::Now.ToString('HH:mm:ss'))] finished $label in $(($t1-$t0).TotalMinutes.ToString('F1')) min ==="
}

Run-Config "crush01-iter130" "results/alphazero-native-superhuman-v6-league-crush01" "iteration_000130.pt" "120" `
    @("--legacy-accept-unverified-semantics","--arena-crush-win-value","0.1")

Run-Config "control03-iter130" "results/alphazero-native-superhuman-control03-from102" "iteration_000130.pt" "120" @()

Run-Config "lever2-iter160-SD160native" "results/alphazero-native-superhuman-lever2-sdstart160" "iteration_000160.pt" "160" @()

Run-Config "lever2-iter160-SD120uniform" "results/alphazero-native-superhuman-lever2-sdstart160" "iteration_000160.pt" "120" @()

Write-Output "=== ALL FOUR CONFIGS COMPLETE ==="
