# Reproducible experiments

Every published run records the git SHA, configuration, policy settings, seed suite, hardware, throughput, and metrics in JSON or CSV. Training may use `train`; model selection may use `validation`; final claims must use the untouched `holdout` seeds in `assets/config/seed_suites.json`.

Claim levels are deliberately narrow: beats random reliably; beats heuristic reliably; beats a search baseline at the same decision budget; strongest local policy in the complete benchmark matrix. “Superhuman” is prohibited unless a separately documented human benchmark supports it.

```powershell
build-codex-vs/src/Release/bomber_benchmark.exe --matrix --episodes 20 --seed 9001 --suite holdout --output results/matrix.json
python tools/learning.py alphazero-lite --seed 1 --output results/az.json --checkpoint results/az.npz
python tools/learning.py ppo --seed 1 --output results/ppo.json --checkpoint results/ppo.npz
python tools/plot_results.py results/ppo.json results/ppo.svg
```

The NumPy learning arena is a compact six-action reference task. Its distance-shaped reward is easier than the full C arena, so results demonstrate reproducibility and learning-pipeline behavior, not transfer or superhuman Bomberman play.
