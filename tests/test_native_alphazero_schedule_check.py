"""KL-101 regression: extending --iterations on a resume must not silently re-derive a new
LR schedule horizon against the same accumulated global_updates. The fresh run (iterations=2,
train_steps=4) resolves and pins learning_rate_schedule_updates=8; the extend run
(iterations=5, train_steps=4, no explicit --lr-schedule-updates) must inherit that same 8,
not re-derive 5*4=20."""

from __future__ import annotations

import json
import sys
from pathlib import Path

run_dir = Path(sys.argv[1])
config = json.loads((run_dir / "config.json").read_text())

resolved = config["learning_rate_schedule_updates"]
assert resolved == 8, (
    f"learning_rate_schedule_updates={resolved} after extending --iterations 2 -> 5 "
    f"without an explicit --lr-schedule-updates override; expected 8 (pinned from the "
    f"original fresh run, 2*4), not 20 (5*4 - a silent re-derive from the new target, "
    f"exactly the bug that produced the observed ~1e-5 -> 1.596e-4 learning-rate jump)"
)

print("Native AlphaZero LR schedule horizon pinning validated "
      f"(learning_rate_schedule_updates={resolved} correctly preserved across --iterations extension)")
