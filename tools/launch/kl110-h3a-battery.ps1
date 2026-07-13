# KL-110 Phase B battery: the aligned-opponent gates ablation - 4 checkpoints x 2 opponent
# models = 8 sequential gates runs on ONE binary (commit cleanly + reconfigure + rebuild BEFORE
# running; each evidence file embeds the stamp/hash for verification). Labels match
# tools/analyze_h3a_verdict.py's REQUIRED_CHECKPOINTS exactly; lever2-160 is descriptive only.
# All read-only checkpoint access; gates use their own dedicated 5000001+ seed block internally.
#
# Usage: .\tools\launch\kl110-h3a-battery.ps1
param(
    [string]$OutDir = "results/kl110-h3a-ablation-2026-07-12"
)
. "$PSScriptRoot\_env.ps1"
Use-Torch
New-Item -ItemType Directory -Force $OutDir | Out-Null

$common = @(
    "--channels", "128", "--blocks", "10", "--width", "13", "--height", "11",
    "--max-steps", "200", "--crate-density", "50", "--flame-duration", "2",
    "--shrink-interval", "4", "--eval-simulations", "96", "--no-progress"
)

# label, run-dir, checkpoint, sudden-death-start (matches each manifest; irrelevant to the
# scenarios themselves at K<=30 but kept fork-line-silent), extra flags
$checkpoints = @(
    @("control03-130", "results/alphazero-native-superhuman-control03-from102", "iteration_000130.pt", "120", @()),
    @("control-154",   "results/kl105-arm-control-from-control03-130",          "latest.pt",           "120", @()),
    @("treatment-154", "results/kl105-arm-treatment-from-control03-130",        "latest.pt",           "120", @()),
    @("lever2-160",    "results/alphazero-native-superhuman-lever2-sdstart160", "iteration_000160.pt", "160", @())
)

foreach ($entry in $checkpoints) {
    $label, $runDir, $ckpt, $sd, $extra = $entry
    foreach ($mode in @("self", "aligned")) {
        $out = "$OutDir/$label-$mode-gates.json"
        Write-Host "=== [$([DateTime]::Now.ToString('HH:mm:ss'))] $label / $mode ==="
        $gateArgs = @("gates", "--run-dir", $runDir, "--checkpoint", $ckpt,
                      "--sudden-death-start", $sd) + $common + $extra + @(
                      "--gates-opponent-model", $mode, "--output", $out)
        & $NativeExe @gateArgs
        if ($LASTEXITCODE -ne 0) { throw "$label/$mode FAILED with exit code $LASTEXITCODE" }
        if (-not (Test-Path $out)) { throw "$label/$mode exited 0 but wrote no $out" }
    }
}
Write-Host "=== ALL 8 ABLATION RUNS COMPLETE -> $OutDir ==="
