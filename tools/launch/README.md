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
| [`v6-league-resume.ps1`](v6-league-resume.ps1) | Resume the **v6-league** retrain (reward fix + opponent league; see `docs/experiment-memory/07-grokking-campaign.md` Part 2) under the auto-restart watchdog. `-Iterations N` to extend the target. |
| [`watchdog-train.ps1`](watchdog-train.ps1) | Generic auto-restarting wrapper for `train` mode — relaunches on any crash (always resuming, never `--fresh`) until the trainer exits 0 or `-MaxRestarts` is hit. Takes the full trainer arg list as one `-TrainerArgs` array. |

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
  more than once. Before launching a training run, check `tasklist` for existing instances —
  including ones from another Claude Code window/terminal against the same repo, which is
  what actually happened overnight 2026-07-10 (an orphaned script from an earlier session was
  silently generating replays in the background, launching its own trainer instances the whole
  time). One `train` + one `evaluate` process together has run fine for extended periods; two
  `train` processes together has not.
