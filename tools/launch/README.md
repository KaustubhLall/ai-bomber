# AI Bomber launch scripts

Reusable, version-controlled launchers for the visualizer, headless sim, and the trained
champion. The desktop shortcuts call these (via `powershell.exe -File …`), so after a rebuild
or a path change you edit **one file** — [`_env.ps1`](_env.ps1) — instead of every `.lnk`.

All scripts resolve binaries through `_env.ps1`, which prefers a **Release** build in
`build/src/Release/` and falls back to `Debug/`. Build first if needed:

```powershell
cmake --build build --config Release --target bomber_viz bomber_headless
```

## Scripts

| Script | What it launches |
| --- | --- |
| [`arena.ps1`](arena.ps1) | Live single-match policy arena (matchup picker). |
| [`compare.ps1`](compare.ps1) | Policy-comparison grid: MCTS vs heuristic/greedy/alpha-beta, both seats. |
| [`history.ps1`](history.ps1) | Match History / replay viewer (latest recorded match). |
| [`mcts.ps1`](mcts.ps1) | MCTS vs heuristic demo match. |
| [`alpha-beta.ps1`](alpha-beta.ps1) | Alpha-beta vs heuristic demo match. |
| [`mcts-selfplay.ps1`](mcts-selfplay.ps1) | Generate (if missing) + watch an MCTS self-play game with **sudden-death + persistent flame**. `-Regen` forces a fresh game; `-Seed N`. |
| [`champion-replay.ps1`](champion-replay.ps1) | Generate (if missing) + watch the **trained champion (v5 iter-210)** play. `-Opponent heuristic\|mcts`, `-Seed N`, `-Regen`. |
| [`tournament.ps1`](tournament.ps1) | Headless round-robin standings among agents. `-Agents "mcts,heuristic,greedy,alpha-beta"`, `-Episodes N`. |
| [`champion-eval.ps1`](champion-eval.ps1) | Native evaluate ladder on the champion (W-D-L + Wilson LCB vs random/heuristic/MCTS). `-Mcts`, `-Games N`. |
| [`v6-league-resume.ps1`](v6-league-resume.ps1) | Resume the **v6-league** retrain (reward fix + opponent league; see `docs/experiment-memory/07-grokking-campaign.md` Part 2) under the auto-restart watchdog. `-Iterations N` to extend the target. **Stopped 2026-07-10 at iteration 102** — its KL-97 iter-100 gate check failed (vs MCTS: 24-6-34, bomb-kill 3.1% of games); GPU handed to `v6-league-crush01-resume.ps1` for the KL-98 lever-1 test. Checkpoint is intact and untouched if you want to inspect it or branch a different lever from the same point. |
| [`v6-league-crush01-resume.ps1`](v6-league-crush01-resume.ps1) | KL-98 reserve lever 1: identical to v6-league except `--arena-crush-win-value 0.1` (was 0.3), branched from v6-league's iteration-102 checkpoint. `-Iterations N`. |
| [`v6-league-gate-eval.ps1`](v6-league-gate-eval.ps1) | The eval-time (noise-off, greedy) win-cause + WAIT% check vs MCTS that actually decides whether a run is working — see KL-97/KL-98/KL-108. `-RunDir path`, `-Checkpoint filename`, `-MctsGames N`, `-SuddenDeathOff` (KL-108 discriminating control), `-PerMatchOutput path` (immutable per-match JSONL rows). |
| [`control03-bootstrap.ps1`](control03-bootstrap.ps1) | **Run once.** KL-108's matched control for the crush01 lever-1 test: forks from the same v6-league iteration-102 checkpoint via `--fork-from`, changing nothing (`arena-crush-win-value` stays 0.3). Establishes `results/alphazero-native-superhuman-control03-from102`. |
| [`control03-resume.ps1`](control03-resume.ps1) | Watchdog-wrapped ordinary resume of control03 toward iteration 130+ (run after `control03-bootstrap.ps1`, never before — resuming before the bootstrap has nothing to resume, and re-running the bootstrap under the watchdog would re-fork and discard progress on every restart). `-Iterations N`. |
| [`watchdog-train.ps1`](watchdog-train.ps1) | Generic auto-restarting wrapper for `train` mode — relaunches on any crash (always resuming, never `--fresh`) until the trainer exits 0 or `-MaxRestarts` is hit. Takes the full trainer arg list as one `-TrainerArgs` array. Durably logs every launch/crash/restart to `<run-dir>/watchdog-attempts.jsonl` (KL-101 Part D). |

## Notes

- The champion launchers need the **native LibTorch trainer** (`build-native-gpu`, Release) and
  the LibTorch runtime (`.venv-gpu/…/torch/lib`); `_env.ps1` puts it on `PATH`. The scripted/viz
  launchers do **not** need torch.
- Generated replays are cached under `results/replays/` (git-ignored). Delete a file or pass
  `-Regen` to rebuild it.
- **Seed hygiene:** `champion-eval.ps1` defaults to the *selection* seed block (900001). Never
  point `-SeedBase` at a final holdout block (A3 = 2400001/2410001, B3 = 2500001/2510001) —
  starting an evaluation there retires it and it can no longer serve as untouched proof.
- **One `bomber_alphazero_native.exe` at a time.** Two full training loops (self-play +
  optimize) concurrently on one GPU has caused abrupt, silent kills (no in-app error trace)
  more than once. This is now an OS-level enforced lock (KL-101 Part D, `ProcessLock` in
  `trainer.cpp`) — a second `train` process fails immediately with a clear error instead of
  silently contending, whether it targets the same run-dir or a different one. `train` +
  `evaluate` together is still fine (evaluate doesn't take the lock).
- **Paired gate-eval comparison:** after running `v6-league-gate-eval.ps1 -PerMatchOutput ...`
  against two checkpoints on the same seed block (e.g. a treatment and its KL-108 matched
  control), compare them with `python tools/analyze_paired_gate_eval.py treatment.jsonl
  control.jsonl` — paired bomb-win deltas on identical seeds, not two independent point
  estimates.
