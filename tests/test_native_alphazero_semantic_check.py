"""KL-101 regression: a checkpoint trained at a non-default arena-crush-win-value must not
silently evaluate at the trainer's struct default when the flag isn't repeated on the
evaluate command line - semantics are inherited from the checkpoint's manifest."""

from __future__ import annotations

import json
import sys
from pathlib import Path

run_dir = Path(sys.argv[1])
result = json.loads((run_dir / "eval-result.json").read_text())

semantics = result["resolved_semantics"]
resolved = semantics["arena_crush_win_value"]
assert abs(resolved - 0.15) < 1e-9, (
    f"evaluate resolved arena_crush_win_value={resolved}, expected the checkpoint's "
    f"trained value 0.15 (inherited); got the trainer's struct default (0.3) instead, "
    f"which is exactly the KL-101 silent-semantics bug"
)

print("Native AlphaZero semantic-manifest inheritance validated "
      f"(arena_crush_win_value={resolved} correctly inherited from checkpoint)")
