"""Replay storage and atomic, complete AlphaZero checkpoints."""

from __future__ import annotations

import json
import os
import shutil
from pathlib import Path

import numpy as np

from .model import PolicyValueNet


FORMAT_VERSION = 1


class ReplayBuffer:
    def __init__(self, capacity: int):
        self.capacity = capacity
        self.states: list[np.ndarray] = []
        self.policies: list[np.ndarray] = []
        self.values: list[float] = []

    def __len__(self) -> int:
        return len(self.values)

    def extend(self, samples: list[tuple[np.ndarray, np.ndarray, float]]) -> None:
        for state, policy, value in samples:
            self.states.append(state.astype(np.float16))
            self.policies.append(policy.astype(np.float32))
            self.values.append(float(value))
        overflow = len(self) - self.capacity
        if overflow > 0:
            del self.states[:overflow]
            del self.policies[:overflow]
            del self.values[:overflow]

    def sample(self, batch_size: int, rng: np.random.Generator) -> tuple[np.ndarray, ...]:
        indices = rng.choice(len(self), size=min(batch_size, len(self)), replace=False)
        return (np.stack([self.states[int(i)] for i in indices]).astype(np.float32),
                np.stack([self.policies[int(i)] for i in indices]),
                np.asarray([self.values[int(i)] for i in indices], dtype=np.float32))

    def arrays(self, input_size: int) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        states = (np.stack(self.states) if self.states else
                  np.empty((0, input_size), dtype=np.float16))
        policies = (np.stack(self.policies) if self.policies else
                    np.empty((0, 6), dtype=np.float32))
        return states, policies, np.asarray(self.values, dtype=np.float32)

    def load_arrays(self, states: np.ndarray, policies: np.ndarray, values: np.ndarray) -> None:
        self.states = [row.copy() for row in states[-self.capacity:]]
        self.policies = [row.copy() for row in policies[-self.capacity:]]
        self.values = [float(value) for value in values[-self.capacity:]]


def _json_array(value: object) -> np.ndarray:
    return np.asarray(json.dumps(value, separators=(",", ":")))


def atomic_save(path: Path, arrays: dict[str, np.ndarray]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("wb") as handle:
        np.savez(handle, **arrays)
        handle.flush()
        os.fsync(handle.fileno())
    os.replace(temporary, path)


def save_checkpoint(path: Path, model: PolicyValueNet, replay: ReplayBuffer,
                    config: dict, iteration: int, global_steps: int,
                    rng: np.random.Generator, best_score: float,
                    last_evaluation: dict | None) -> None:
    states, policies, values = replay.arrays(model.input_size)
    arrays = model.state_arrays()
    arrays.update({
        "format_version": np.asarray(FORMAT_VERSION, dtype=np.int64),
        "iteration": np.asarray(iteration, dtype=np.int64),
        "global_steps": np.asarray(global_steps, dtype=np.int64),
        "best_score": np.asarray(best_score, dtype=np.float64),
        "config_json": _json_array(config),
        "rng_json": _json_array(rng.bit_generator.state),
        "evaluation_json": _json_array(last_evaluation or {}),
        "replay_states": states,
        "replay_policies": policies,
        "replay_values": values,
    })
    atomic_save(path, arrays)


def load_checkpoint(path: Path, model: PolicyValueNet, replay: ReplayBuffer,
                    expected_config: dict, rng: np.random.Generator) -> dict:
    with np.load(path, allow_pickle=False) as data:
        if int(data["format_version"]) != FORMAT_VERSION:
            raise RuntimeError("unsupported checkpoint format")
        stored_config = json.loads(str(data["config_json"]))
        signature_keys = ("width", "height", "channels", "abi_version", "hidden_size",
                          "conv_channels")
        mismatch = [key for key in signature_keys if stored_config.get(key) != expected_config.get(key)]
        if mismatch:
            raise RuntimeError(f"checkpoint configuration mismatch: {', '.join(mismatch)}")
        model.load_arrays({name: data[name] for name in data.files if
                           name.startswith(("param_", "adam_", "scheduler_"))})
        replay.load_arrays(data["replay_states"], data["replay_policies"], data["replay_values"])
        rng.bit_generator.state = json.loads(str(data["rng_json"]))
        return {
            "iteration": int(data["iteration"]),
            "global_steps": int(data["global_steps"]),
            "best_score": float(data["best_score"]),
            "evaluation": json.loads(str(data["evaluation_json"])),
            "config": stored_config,
        }


def copy_checkpoint(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + ".tmp")
    shutil.copyfile(source, temporary)
    os.replace(temporary, destination)
