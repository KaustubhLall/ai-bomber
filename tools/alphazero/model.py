"""Small dependency-light policy/value network with Adam optimization."""

from __future__ import annotations

import numpy as np

from .env import ACTIONS, CHANNELS


class PolicyValueNet:
    def __init__(self, input_size: int, hidden_size: int, seed: int = 1,
                 conv_channels: int = 16):
        self.input_size = input_size
        self.hidden_size = hidden_size
        self.conv_channels = conv_channels
        cells = input_size // CHANNELS
        self.view_size = int(np.sqrt(cells))
        if self.view_size * self.view_size * CHANNELS != input_size:
            raise ValueError("policy input must be a square channel-first tensor")
        rng = np.random.default_rng(seed)
        conv_input = CHANNELS * 3 * 3
        conv_flat = self.view_size * self.view_size * conv_channels
        self.params = {
            "wc": (rng.standard_normal((conv_input, conv_channels)) /
                   np.sqrt(conv_input)).astype(np.float32),
            "bc": np.zeros(conv_channels, np.float32),
            "w1": (rng.standard_normal((conv_flat, hidden_size)) /
                   np.sqrt(conv_flat)).astype(np.float32),
            "b1": np.zeros(hidden_size, np.float32),
            "wp": (rng.standard_normal((hidden_size, ACTIONS)) /
                   np.sqrt(hidden_size)).astype(np.float32),
            "bp": np.zeros(ACTIONS, np.float32),
            "wv": (rng.standard_normal((hidden_size, 1)) /
                   np.sqrt(hidden_size)).astype(np.float32),
            "bv": np.zeros(1, np.float32),
        }
        self.adam_m = {name: np.zeros_like(value) for name, value in self.params.items()}
        self.adam_v = {name: np.zeros_like(value) for name, value in self.params.items()}
        self.adam_step = 0
        self.scheduler_step = 0

    def _conv_features(self, x: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        batch = len(x)
        board = x.reshape(batch, CHANNELS, self.view_size, self.view_size)
        padded = np.pad(board, ((0, 0), (0, 0), (1, 1), (1, 1)))
        windows = np.lib.stride_tricks.sliding_window_view(
            padded, (3, 3), axis=(2, 3))
        columns = windows.transpose(0, 2, 3, 1, 4, 5).reshape(
            batch * self.view_size * self.view_size, CHANNELS * 9)
        conv_pre = columns @ self.params["wc"] + self.params["bc"]
        features = np.maximum(conv_pre, 0.0).reshape(batch, -1)
        return columns, conv_pre, features

    def _forward(self, x: np.ndarray) -> tuple[np.ndarray, ...]:
        columns, conv_pre, features = self._conv_features(x)
        hidden_pre = features @ self.params["w1"] + self.params["b1"]
        hidden = np.maximum(hidden_pre, 0.0)
        logits = hidden @ self.params["wp"] + self.params["bp"]
        logits -= logits.max(axis=1, keepdims=True)
        probabilities = np.exp(logits)
        probabilities /= probabilities.sum(axis=1, keepdims=True)
        value = np.tanh(hidden @ self.params["wv"] + self.params["bv"])
        return columns, conv_pre, features, hidden_pre, probabilities, value[:, 0]

    def predict(self, x: np.ndarray) -> tuple[np.ndarray, np.ndarray | float]:
        single = x.ndim == 1
        batch = x.reshape(1, -1) if single else x
        *_, policy, value = self._forward(batch.astype(np.float32, copy=False))
        return (policy[0], float(value[0])) if single else (policy, value)

    def train_batch(self, x: np.ndarray, target_policy: np.ndarray, target_value: np.ndarray,
                    learning_rate: float, weight_decay: float = 1e-4) -> dict[str, float]:
        x = x.astype(np.float32, copy=False)
        target_policy = target_policy.astype(np.float32, copy=False)
        target_value = target_value.astype(np.float32, copy=False)
        batch_size = max(len(x), 1)
        columns, conv_pre, features, hidden_pre, policy, value = self._forward(x)
        hidden = np.maximum(hidden_pre, 0.0)

        policy_loss = float(-np.sum(target_policy * np.log(policy + 1e-8)) / batch_size)
        value_loss = float(np.mean((value - target_value) ** 2))
        entropy = float(-np.sum(policy * np.log(policy + 1e-8)) / batch_size)

        dlogits = (policy - target_policy) / batch_size
        dvalue = (2.0 * (value - target_value) * (1.0 - value * value) / batch_size)[:, None]
        grads = {
            "wp": hidden.T @ dlogits + weight_decay * self.params["wp"],
            "bp": dlogits.sum(axis=0),
            "wv": hidden.T @ dvalue + weight_decay * self.params["wv"],
            "bv": dvalue.sum(axis=0),
        }
        dhidden = dlogits @ self.params["wp"].T + dvalue @ self.params["wv"].T
        dhidden[hidden_pre <= 0.0] = 0.0
        grads["w1"] = features.T @ dhidden + weight_decay * self.params["w1"]
        grads["b1"] = dhidden.sum(axis=0)
        dfeatures = dhidden @ self.params["w1"].T
        dconv = dfeatures.reshape(-1, self.conv_channels)
        dconv[conv_pre <= 0.0] = 0.0
        grads["wc"] = columns.T @ dconv + weight_decay * self.params["wc"]
        grads["bc"] = dconv.sum(axis=0)

        self.adam_step += 1
        beta1, beta2, epsilon = 0.9, 0.999, 1e-8
        for name, gradient in grads.items():
            gradient = np.clip(gradient, -5.0, 5.0)
            self.adam_m[name] = beta1 * self.adam_m[name] + (1.0 - beta1) * gradient
            self.adam_v[name] = beta2 * self.adam_v[name] + (1.0 - beta2) * gradient * gradient
            corrected_m = self.adam_m[name] / (1.0 - beta1 ** self.adam_step)
            corrected_v = self.adam_v[name] / (1.0 - beta2 ** self.adam_step)
            self.params[name] -= learning_rate * corrected_m / (np.sqrt(corrected_v) + epsilon)

        return {"policy_loss": policy_loss, "value_loss": value_loss,
                "entropy": entropy, "loss": policy_loss + value_loss}

    def state_arrays(self) -> dict[str, np.ndarray]:
        arrays: dict[str, np.ndarray] = {f"param_{key}": value for key, value in self.params.items()}
        arrays.update({f"adam_m_{key}": value for key, value in self.adam_m.items()})
        arrays.update({f"adam_v_{key}": value for key, value in self.adam_v.items()})
        arrays["adam_step"] = np.asarray(self.adam_step, dtype=np.int64)
        arrays["scheduler_step"] = np.asarray(self.scheduler_step, dtype=np.int64)
        return arrays

    def load_arrays(self, arrays: dict[str, np.ndarray]) -> None:
        for name in self.params:
            self.params[name][...] = arrays[f"param_{name}"]
            self.adam_m[name][...] = arrays[f"adam_m_{name}"]
            self.adam_v[name][...] = arrays[f"adam_v_{name}"]
        self.adam_step = int(arrays["adam_step"])
        self.scheduler_step = int(arrays["scheduler_step"])

    def copy(self) -> "PolicyValueNet":
        result = PolicyValueNet(self.input_size, self.hidden_size,
                                conv_channels=self.conv_channels)
        result.load_arrays(self.state_arrays())
        return result
