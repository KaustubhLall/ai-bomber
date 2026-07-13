import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.alphazero.checkpoint import ReplayBuffer, load_checkpoint, save_checkpoint
from tools.alphazero.env import TrainingLibrary
from tools.alphazero.model import PolicyValueNet
from tools.alphazero.mcts import select_decoupled_joint
from tools.alphazero.progress import ProgressBar
from tools.alphazero.trainer import Trainer


library = TrainingLibrary(os.environ.get("AI_BOMBER_TRAINING_LIB"))
env = library.create(7, 7, 8, 20, 123)
clone = env.clone()
assert env.state_hash() == clone.state_hash()
clone.step_joint(5, 5)
assert env.state_hash() != clone.state_hash()
clone.reset(123)
assert env.state_hash() == clone.state_hash()
assert env.encode(0).shape == (17 * 11 * 11,)
assert env.legal_mask(0)[5]
assert np.all(~env.safe_mask(0) | env.legal_mask(0))
mcts = library.create_agent(7, 99)
mcts.configure_mcts(8, 4)
assert 0 <= mcts.action(env, 0) < 6
mcts.close()
env.close()
clone.close()

# Player zero should select its high-value action while player one selects the
# action that is worst for player zero, rather than cooperating on a joint max.
priors = np.full(36, 1 / 36, np.float32)
visits = np.ones(36, np.int32)
values = np.zeros(36, np.float32).reshape(6, 6)
values[3, :] += 5.0
values[:, 4] -= 10.0
joint = select_decoupled_joint(visits, values.reshape(-1), priors, 0.01)
assert joint // 6 == 3 and joint % 6 == 4

bar = ProgressBar("test", 10, "items", enabled=False)
bar.started -= 5.0
line = bar.line(5)
assert "50.0%" in line and "ETA" in line and "items/s" in line

# The absence of an incumbent must not bypass the quality floor.
assert not Trainer._should_promote(
    {"random": {"score": 0.89}, "heuristic": {"score": 1.0}},
    best_exists=False, best_score=float("-inf"), promotion_margin=0.02)
assert Trainer._should_promote(
    {"random": {"score": 0.9}, "heuristic": {"score": 0.5}},
    best_exists=False, best_score=float("-inf"), promotion_margin=0.02)

with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    model = PolicyValueNet(17 * 11 * 11, 8, seed=7)
    replay = ReplayBuffer(16)
    state = np.zeros(model.input_size, np.float32)
    replay.extend([(state, np.full(6, 1 / 6, np.float32), 1.0)])
    rng = np.random.default_rng(9)
    checkpoint = directory / "roundtrip.npz"
    config = {"width": 7, "height": 7, "channels": 17, "abi_version": 6,
              "hidden_size": 8, "conv_channels": 16}
    save_checkpoint(checkpoint, model, replay, config, 3, 44, rng, 0.5, {"ok": True})
    expected_next_random = int(rng.integers(0, 1_000_000))
    expected_policy, expected_value = model.predict(state)
    restored = PolicyValueNet(model.input_size, 8, seed=99)
    restored_replay = ReplayBuffer(16)
    restored_rng = np.random.default_rng(99)
    metadata = load_checkpoint(checkpoint, restored, restored_replay, config, restored_rng)
    actual_policy, actual_value = restored.predict(state)
    np.testing.assert_allclose(actual_policy, expected_policy)
    assert actual_value == expected_value
    assert metadata["iteration"] == 3 and metadata["global_steps"] == 44
    assert len(restored_replay) == 1
    assert int(restored_rng.integers(0, 1_000_000)) == expected_next_random

    run_dir = directory / "resume"
    command = [
        sys.executable, str(ROOT / "tools" / "train_alphazero.py"),
        "--library", str(library.path), "--run-dir", str(run_dir),
        "--iterations", "1", "--self-play-games", "1", "--simulations", "1",
        "--teacher-games", "0",
        "--adversary-games", "0",
        "--train-steps", "1", "--batch-size", "2", "--buffer-capacity", "32",
        "--hidden-size", "8", "--width", "7", "--height", "7", "--max-steps", "4",
        "--crate-density", "10", "--evaluation-games", "1", "--snapshot-interval", "1",
        "--seed", "5", "--fresh", "--no-progress",
    ]
    subprocess.run(command, cwd=ROOT, check=True, capture_output=True, text=True)
    command[command.index("1", command.index("--iterations") + 1)] = "2"
    command.remove("--fresh")
    resumed = subprocess.run(command, cwd=ROOT, check=True, capture_output=True, text=True)
    assert "Resumed" in resumed.stdout
    metrics = [json.loads(line) for line in (run_dir / "metrics.jsonl").read_text().splitlines()]
    assert [row["iteration"] for row in metrics] == [1, 2]
    with np.load(run_dir / "latest.npz", allow_pickle=False) as data:
        assert int(data["iteration"]) == 2

print("AlphaZero bridge, checkpoint, progress, smoke, and resume tests passed")
