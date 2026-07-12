# Evidence: KL-105 tactical-gates baseline, 2026-07-12

Pre-intervention baseline for the six deterministic tactical gates (doc 13 section 2),
run at commit `58f688b` (clean stamp, verified in each file's `git_commit`) with
`--eval-simulations 96` (the diagnostic search budget). These are the "before" numbers the
Phase 3 arms are compared against. Each JSON is itself write-once KL-101-style evidence with
embedded argv/hashes/resolved semantics.

## Results (search mode = the deployed decision procedure, authoritative)

| scenario | control03-130 | crush01-130 | lever2-160 | heuristic ref* | MCTS-256 ref |
|---|---|---|---|---|---|
| bomb-and-escape | FAIL | FAIL | FAIL | PASS | PASS (4) |
| corridor-clear | PASS (12) | PASS (14) | PASS (12) | PASS | PASS (10) |
| trap | FAIL — **raw PASSes (5)** | FAIL | FAIL — **raw PASSes (5)** | PASS | PASS (6) |
| chase | FAIL | FAIL | FAIL | PASS | FAIL |
| flame-timing | FAIL | FAIL | FAIL | FAIL | FAIL |
| stall-break | FAIL | FAIL | FAIL | PASS | PASS (4) |
| **total (search)** | **1/6** | **1/6** | **1/6** | 5/6 | 4/6 |

*Heuristic reference results are the CI-pinned run (`tests/test_native_alphazero_gates_check.py`),
not re-run here. Every gate is achievable by at least one reference agent: flame-timing was
passed by an exploratory MCTS-32/depth-6 run in 12 steps (Unit A executor's summary; not
archived), chase by the heuristic. The MCTS-256 reference file in this directory loaded the
tiny CI fixture checkpoint purely to satisfy the loader — in `--gates-agent` mode the network
is unused and the checkpoint identity is irrelevant to the results.*

## The load-bearing observation

On two of the three checkpoints (control03-130 and lever2-160), **raw-argmax mode passes the
trap gate in 5 steps while 96-simulation search FAILS it**: the bare policy head walks up and
takes the demonstrated kill; adding search *removes* it. This inverts the usual direction of
the aggregate finding (search was measured as net slightly *anti-passive* on the diagnostic
distribution) and shows that in at least this constructed kill position, the search/value side
actively suppresses aggression the head would have taken. It sharpens the intervention's
mechanism: capped cause-balanced replay retrains BOTH heads (bomb-win-side samples carry +1
value targets, so the value estimates that steer search away from kills are being retrained
too, not just the prior).

All three checkpoints pass exactly one gate deployed (corridor-clear — crate clearing, the one
skill the training distribution rewards constantly). Zero of three pass any gate that requires
engaging the opponent.

## Files

- `control03-iter130-gates.json`, `crush01-iter130-gates.json` (legacy semantics, crush 0.1),
  `lever2-iter160-gates.json` (native SD160) — network runs, search+raw modes.
- `mcts256-reference-gates.json` — `--gates-agent mcts` (256 sims / depth 16) achievability run.
- `SHA256SUMS` — hashes of the four evidence files.

Reproduce: each file's `invocation_argv`. Gates are deterministic (fixed seeds, noise-off,
greedy; CI asserts byte-identical reruns).
