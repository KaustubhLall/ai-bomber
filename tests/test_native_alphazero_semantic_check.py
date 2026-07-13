"""KL-101 regression: a checkpoint trained at a non-default arena-crush-win-value must not
silently evaluate at the trainer's struct default when the flag isn't repeated on the
evaluate command line - semantics are inherited from the checkpoint's manifest."""

from __future__ import annotations

import json
import hashlib
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

checkpoint = run_dir / "latest.pt"
assert result["checkpoint_sha256"] == hashlib.sha256(checkpoint.read_bytes()).hexdigest()
assert result["executable_sha256"], "evaluation evidence must identify the executable"
assert result["generated_at_utc"].endswith("Z")
assert "evaluate" in result["invocation_argv"], (
    "evaluation evidence must preserve exact argv as a JSON array"
)
assert result["checkpoint_semantics_verified"] is True
assert result["checkpoint_semantics_source"] == "checkpoint_manifest"
assert Path(result["checkpoint_path"]).is_absolute()
assert result["working_directory"]
assert result["runtime_config_signature"]

draw_alias = json.loads((run_dir / "eval-draw-alias.json").read_text())
assert abs(draw_alias["resolved_semantics"]["timeout_draw_value"] - (-0.7)) < 1e-9
assert abs(draw_alias["resolved_semantics"]["mutual_death_value"] - (-0.7)) < 1e-9, (
    "explicit --draw-value must override both inherited draw fields"
)

print("Native AlphaZero semantic inheritance + evidence provenance validated "
      f"(arena_crush_win_value={resolved}, --draw-value alias honored, "
      "checkpoint/executable/exact argv recorded)")
