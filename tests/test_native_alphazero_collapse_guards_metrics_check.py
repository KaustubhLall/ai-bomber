"""v7 Stage-1 IL Bombing-Collapse guards (docs/experiment-memory/14-v7-from-scratch-design.md
Stage 1; Meisheri et al. 2019 arXiv:1911.04947): the two applied-loss metrics keys must appear
on every optimization row, and must reflect the staged value warmup exactly.

The fixture is a single fresh 3-iteration run with BOTH knobs on
(--value-only-iterations 2 -> K=2, --policy-entropy-bonus 0.01). metrics.jsonl rows are emitted
AFTER run()'s ++iteration, so the 0-based value-only window `iteration < K` in optimize() shows
up to a reader as rows iteration 1..K:
  - rows iteration 1, 2 (<= K): value-only warmup -> policy_loss_applied_weight == 0.0, and the
    entropy floor is inactive while the policy term is (so policy_entropy_bonus_applied == 0.0
    even though --policy-entropy-bonus 0.01 is configured).
  - row iteration 3 (> K): policy active -> policy_loss_applied_weight == 1.0 and the entropy
    floor is live -> policy_entropy_bonus_applied == 0.01.

Also confirms config.json and the fresh runtime_config (config-history.jsonl) both persist the
two new semantic fields. Structurally mirrors the forced-playouts metrics check."""

from __future__ import annotations

import json
import sys
from pathlib import Path


def parse_runtime_config(raw: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for pair in raw.split(";"):
        if "=" not in pair:
            continue
        key, _, value = pair.partition("=")
        result[key] = value
    return result


run_dir = Path(sys.argv[1])
K = 2
BETA = 0.01

rows = [json.loads(line) for line in (run_dir / "metrics.jsonl").read_text().splitlines()]
assert len(rows) == 3, (
    f"expected 3 metrics.jsonl rows (one per completed iteration), got {len(rows)}"
)

for row in rows:
    iteration = row["iteration"]
    optimization = row.get("optimization")
    assert optimization is not None, f"iteration {iteration}: missing 'optimization' block"

    assert "policy_loss_applied_weight" in optimization, (
        f"iteration {iteration}: optimization block missing 'policy_loss_applied_weight' - every "
        f"row must carry it so a reader can see what the loss contained"
    )
    assert "policy_entropy_bonus_applied" in optimization, (
        f"iteration {iteration}: optimization block missing 'policy_entropy_bonus_applied'"
    )

    weight = optimization["policy_loss_applied_weight"]
    bonus = optimization["policy_entropy_bonus_applied"]

    if iteration <= K:
        assert abs(weight - 0.0) < 1e-9, (
            f"iteration {iteration} (<= K={K}, value-only warmup): "
            f"policy_loss_applied_weight={weight}, expected 0.0 - the policy CE must not "
            f"contribute to the loss during the staged value warmup"
        )
        assert abs(bonus - 0.0) < 1e-9, (
            f"iteration {iteration} (<= K={K}): policy_entropy_bonus_applied={bonus}, expected "
            f"0.0 - the entropy floor is only applied when the policy term is active"
        )
    else:
        assert abs(weight - 1.0) < 1e-9, (
            f"iteration {iteration} (> K={K}): policy_loss_applied_weight={weight}, expected 1.0 "
            f"- the policy CE must be active once the value-only warmup ends"
        )
        assert abs(bonus - BETA) < 1e-9, (
            f"iteration {iteration} (> K={K}): policy_entropy_bonus_applied={bonus}, expected "
            f"{BETA} (the configured --policy-entropy-bonus) once the policy term is active"
        )

# config.json (written every process) must carry both fields.
config = json.loads((run_dir / "config.json").read_text())
assert config["value_only_iterations"] == K, (
    f"config.json value_only_iterations={config.get('value_only_iterations')!r}, expected {K}"
)
assert abs(config["policy_entropy_bonus"] - BETA) < 1e-9, (
    f"config.json policy_entropy_bonus={config.get('policy_entropy_bonus')!r}, expected {BETA}"
)

# The fresh runtime_config signature must carry both fields too.
history = [
    json.loads(line)
    for line in (run_dir / "config-history.jsonl").read_text().splitlines()
]
fresh_cfg = parse_runtime_config(history[0]["runtime_config"])
assert fresh_cfg.get("value_only_iterations") == str(K), (
    f"fresh runtime_config value_only_iterations={fresh_cfg.get('value_only_iterations')!r}, "
    f"expected {K}"
)
assert abs(float(fresh_cfg["policy_entropy_bonus"]) - BETA) < 1e-9, (
    f"fresh runtime_config policy_entropy_bonus={fresh_cfg.get('policy_entropy_bonus')!r}, "
    f"expected {BETA}"
)

print(
    "Native AlphaZero collapse-guards applied-loss metrics validated across "
    f"{len(rows)} rows (value-only rows 1..{K} weight 0.0/bonus 0.0, row 3 weight 1.0/bonus "
    f"{BETA}); config.json + fresh runtime_config carry both fields"
)
