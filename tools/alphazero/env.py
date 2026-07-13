"""ctypes wrapper for the opaque AI Bomber training ABI."""

from __future__ import annotations

import ctypes
import os
from pathlib import Path

import numpy as np


ABI_VERSION = 6
ACTIONS = 6
CHANNELS = 17


def _candidate_libraries() -> list[Path]:
    root = Path(__file__).resolve().parents[2]
    names = ("bomber_training.dll", "libbomber_training.so", "libbomber_training.dylib")
    candidates: list[Path] = []
    configured = os.environ.get("AI_BOMBER_TRAINING_LIB")
    if configured:
        candidates.append(Path(configured))
    for build in ("build-codex-vs", "build", "build-alpha-zero", "build_ninja"):
        base = root / build
        for name in names:
            candidates.extend((base / name, base / "src" / name, base / "src" / "Release" / name))
    return candidates


class TrainingLibrary:
    def __init__(self, path: str | os.PathLike[str] | None = None):
        candidates = [Path(path)] if path else _candidate_libraries()
        found = next((candidate.resolve() for candidate in candidates if candidate.is_file()), None)
        if found is None:
            looked = "\n  ".join(str(item) for item in candidates)
            raise FileNotFoundError(
                "bomber_training shared library not found. Build target bomber_training or pass "
                f"--library. Looked in:\n  {looked}"
            )
        self.path = found
        self.lib = ctypes.CDLL(str(found))
        self._bind()
        if self.lib.bomber_training_abi_version() != ABI_VERSION:
            raise RuntimeError("incompatible bomber_training ABI")
        if self.lib.bomber_training_channels() != CHANNELS or self.lib.bomber_training_actions() != ACTIONS:
            raise RuntimeError("training tensor/action schema mismatch")

    def _bind(self) -> None:
        lib = self.lib
        lib.bomber_training_abi_version.restype = ctypes.c_int
        lib.bomber_training_channels.restype = ctypes.c_int
        lib.bomber_training_actions.restype = ctypes.c_int
        lib.bomber_training_create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                                ctypes.c_int, ctypes.c_uint64]
        lib.bomber_training_create.restype = ctypes.c_void_p
        lib.bomber_training_clone.argtypes = [ctypes.c_void_p]
        lib.bomber_training_clone.restype = ctypes.c_void_p
        lib.bomber_training_destroy.argtypes = [ctypes.c_void_p]
        lib.bomber_training_reset.argtypes = [ctypes.c_void_p, ctypes.c_uint64]
        for name in ("width", "height", "step_count", "observation_size"):
            fn = getattr(lib, f"bomber_training_{name}")
            fn.argtypes = [ctypes.c_void_p]
            fn.restype = ctypes.c_int
        lib.bomber_training_encode.argtypes = [ctypes.c_void_p, ctypes.c_int,
                                                ctypes.POINTER(ctypes.c_float), ctypes.c_int]
        lib.bomber_training_encode.restype = ctypes.c_int
        lib.bomber_training_legal_actions.argtypes = [ctypes.c_void_p, ctypes.c_int,
                                                       ctypes.POINTER(ctypes.c_int), ctypes.c_int]
        lib.bomber_training_legal_actions.restype = ctypes.c_int
        lib.bomber_training_safe_actions.argtypes = [ctypes.c_void_p, ctypes.c_int,
                                                      ctypes.POINTER(ctypes.c_int), ctypes.c_int]
        lib.bomber_training_safe_actions.restype = ctypes.c_int
        lib.bomber_training_step_joint.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
        lib.bomber_training_step_joint.restype = ctypes.c_int
        lib.bomber_training_outcome.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.bomber_training_outcome.restype = ctypes.c_int
        lib.bomber_training_state_hash.argtypes = [ctypes.c_void_p]
        lib.bomber_training_state_hash.restype = ctypes.c_uint64
        lib.bomber_training_tactical_value.argtypes = [ctypes.c_void_p, ctypes.c_int]
        lib.bomber_training_tactical_value.restype = ctypes.c_float
        lib.bomber_training_agent_create.argtypes = [ctypes.c_int, ctypes.c_uint64]
        lib.bomber_training_agent_create.restype = ctypes.c_void_p
        lib.bomber_training_agent_destroy.argtypes = [ctypes.c_void_p]
        lib.bomber_training_agent_action.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                                      ctypes.c_int]
        lib.bomber_training_agent_action.restype = ctypes.c_int
        lib.bomber_training_agent_configure_mcts.argtypes = [
            ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
        lib.bomber_training_agent_configure_mcts.restype = ctypes.c_int
        lib.bomber_training_baseline_action.argtypes = [ctypes.c_int, ctypes.c_void_p,
                                                        ctypes.c_int, ctypes.c_uint64]
        lib.bomber_training_baseline_action.restype = ctypes.c_int

    def create(self, width: int, height: int, max_steps: int, crate_density: int,
               seed: int) -> "BomberEnv":
        pointer = self.lib.bomber_training_create(width, height, max_steps, crate_density, seed)
        if not pointer:
            raise MemoryError("bomber_training_create failed")
        return BomberEnv(self, pointer)

    def create_agent(self, agent_type: int, seed: int) -> "BaselineAgent":
        pointer = self.lib.bomber_training_agent_create(agent_type, seed)
        if not pointer:
            raise ValueError(f"unsupported native agent type: {agent_type}")
        return BaselineAgent(self, pointer)


class BomberEnv:
    def __init__(self, library: TrainingLibrary, pointer: int):
        self.library = library
        self._pointer = ctypes.c_void_p(pointer)

    def close(self) -> None:
        if self._pointer:
            self.library.lib.bomber_training_destroy(self._pointer)
            self._pointer = ctypes.c_void_p()

    def __del__(self) -> None:
        self.close()

    def clone(self) -> "BomberEnv":
        pointer = self.library.lib.bomber_training_clone(self._pointer)
        if not pointer:
            raise MemoryError("bomber_training_clone failed")
        return BomberEnv(self.library, pointer)

    def reset(self, seed: int) -> None:
        self.library.lib.bomber_training_reset(self._pointer, seed)

    @property
    def width(self) -> int:
        return self.library.lib.bomber_training_width(self._pointer)

    @property
    def height(self) -> int:
        return self.library.lib.bomber_training_height(self._pointer)

    @property
    def step_count(self) -> int:
        return self.library.lib.bomber_training_step_count(self._pointer)

    @property
    def observation_size(self) -> int:
        return self.library.lib.bomber_training_observation_size(self._pointer)

    def encode(self, perspective: int) -> np.ndarray:
        result = np.empty(self.observation_size, dtype=np.float32)
        written = self.library.lib.bomber_training_encode(
            self._pointer, perspective,
            result.ctypes.data_as(ctypes.POINTER(ctypes.c_float)), result.size)
        if written != result.size:
            raise RuntimeError("bomber_training_encode failed")
        return result

    def legal_mask(self, perspective: int) -> np.ndarray:
        result = np.zeros(ACTIONS, dtype=np.int32)
        count = self.library.lib.bomber_training_legal_actions(
            self._pointer, perspective,
            result.ctypes.data_as(ctypes.POINTER(ctypes.c_int)), result.size)
        if count <= 0:
            result[-1] = 1
        return result.astype(bool)

    def safe_mask(self, perspective: int) -> np.ndarray:
        result = np.zeros(ACTIONS, dtype=np.int32)
        count = self.library.lib.bomber_training_safe_actions(
            self._pointer, perspective,
            result.ctypes.data_as(ctypes.POINTER(ctypes.c_int)), result.size)
        return result.astype(bool) if count > 0 else self.legal_mask(perspective)

    def step_joint(self, action_zero: int, action_one: int) -> bool:
        result = self.library.lib.bomber_training_step_joint(
            self._pointer, action_zero, action_one)
        if result < 0:
            raise ValueError("invalid joint action")
        return bool(result)

    def outcome(self, perspective: int) -> int:
        return self.library.lib.bomber_training_outcome(self._pointer, perspective)

    def state_hash(self) -> int:
        return self.library.lib.bomber_training_state_hash(self._pointer)

    def tactical_value(self, perspective: int) -> float:
        return float(self.library.lib.bomber_training_tactical_value(
            self._pointer, perspective))

    def baseline_action(self, agent_type: int, perspective: int, seed: int) -> int:
        action = self.library.lib.bomber_training_baseline_action(
            agent_type, self._pointer, perspective, seed)
        if not 0 <= action < ACTIONS:
            raise RuntimeError("one-shot native baseline returned an invalid action")
        return action


class BaselineAgent:
    def __init__(self, library: TrainingLibrary, pointer: int):
        self.library = library
        self._pointer = ctypes.c_void_p(pointer)

    def close(self) -> None:
        if self._pointer:
            self.library.lib.bomber_training_agent_destroy(self._pointer)
            self._pointer = ctypes.c_void_p()

    def __del__(self) -> None:
        self.close()

    def action(self, env: BomberEnv, perspective: int) -> int:
        action = self.library.lib.bomber_training_agent_action(
            self._pointer, env._pointer, perspective)
        if not 0 <= action < ACTIONS:
            raise RuntimeError("native baseline returned an invalid action")
        return action

    def configure_mcts(self, simulations: int, rollout_depth: int) -> None:
        if not self.library.lib.bomber_training_agent_configure_mcts(
                self._pointer, simulations, rollout_depth):
            raise ValueError("agent is not MCTS or its search configuration is invalid")
