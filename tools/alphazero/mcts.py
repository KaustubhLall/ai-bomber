"""PUCT over simultaneous joint actions, marginalized to each six-action policy."""

from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

from .env import ACTIONS, BomberEnv
from .model import PolicyValueNet


@dataclass
class Node:
    env: BomberEnv
    terminal: bool = False
    priors: np.ndarray | None = None
    visits: np.ndarray = field(default_factory=lambda: np.zeros(ACTIONS * ACTIONS, np.int32))
    value_sum: np.ndarray = field(default_factory=lambda: np.zeros(ACTIONS * ACTIONS, np.float32))
    children: dict[int, "Node"] = field(default_factory=dict)

    def close(self) -> None:
        for child in self.children.values():
            child.close()
        self.env.close()


def masked_policy(model: PolicyValueNet, env: BomberEnv, perspective: int) -> tuple[np.ndarray, float]:
    policy, value = model.predict(env.encode(perspective))
    safe = env.safe_mask(perspective)
    policy = np.where(safe, policy, 0.0)
    total = float(policy.sum())
    policy = policy / total if total > 0 else safe.astype(np.float32) / safe.sum()
    return policy.astype(np.float32), float(value)


class JointMCTS:
    def __init__(self, model: PolicyValueNet, simulations: int, c_puct: float,
                 dirichlet_alpha: float, dirichlet_fraction: float,
                 rng: np.random.Generator, heuristic_weight: float = 0.0,
                 fixed_opponent_type: int | None = None,
                 fixed_opponent_seat: int | None = None,
                 opponent_seed: int = 1):
        self.model = model
        self.simulations = simulations
        self.c_puct = c_puct
        self.dirichlet_alpha = dirichlet_alpha
        self.dirichlet_fraction = dirichlet_fraction
        self.rng = rng
        self.heuristic_weight = heuristic_weight
        self.fixed_opponent_type = fixed_opponent_type
        self.fixed_opponent_seat = fixed_opponent_seat
        self.opponent_seed = opponent_seed

    def _expand(self, node: Node) -> float:
        if node.terminal:
            return float(node.env.outcome(0))
        policy_zero, value_zero = masked_policy(self.model, node.env, 0)
        policy_one, value_one = masked_policy(self.model, node.env, 1)
        if self.fixed_opponent_type is not None and self.fixed_opponent_seat is not None:
            modeled_action = node.env.baseline_action(
                self.fixed_opponent_type, self.fixed_opponent_seat,
                self.opponent_seed ^ node.env.state_hash())
            modeled_policy = np.zeros(ACTIONS, np.float32)
            modeled_policy[modeled_action] = 1.0
            if self.fixed_opponent_seat == 0:
                policy_zero = modeled_policy
            else:
                policy_one = modeled_policy
        node.priors = np.outer(policy_zero, policy_one).reshape(-1)
        node.priors /= node.priors.sum()
        network_value = 0.5 * (value_zero - value_one)
        if self.heuristic_weight <= 0.0:
            return network_value
        tactical_zero = np.tanh(node.env.tactical_value(0) / 250.0)
        tactical_one = np.tanh(node.env.tactical_value(1) / 250.0)
        tactical_value = 0.5 * (tactical_zero - tactical_one)
        return ((1.0 - self.heuristic_weight) * network_value +
                self.heuristic_weight * tactical_value)

    def _simulate(self, node: Node) -> float:
        if node.terminal:
            return float(node.env.outcome(0))
        if node.priors is None:
            return self._expand(node)
        joint_action = select_decoupled_joint(node.visits, node.value_sum,
                                               node.priors, self.c_puct)
        child = node.children.get(joint_action)
        if child is None:
            child_env = node.env.clone()
            terminal = child_env.step_joint(joint_action // ACTIONS, joint_action % ACTIONS)
            child = Node(child_env, terminal=terminal)
            node.children[joint_action] = child
        value = self._simulate(child)
        node.visits[joint_action] += 1
        node.value_sum[joint_action] += value
        return value

    def search(self, env: BomberEnv, add_root_noise: bool = True) -> np.ndarray:
        root = Node(env.clone())
        self._expand(root)
        if add_root_noise and root.priors is not None:
            legal = root.priors > 0
            noise = self.rng.dirichlet(np.full(int(legal.sum()), self.dirichlet_alpha))
            root.priors[legal] = ((1.0 - self.dirichlet_fraction) * root.priors[legal] +
                                  self.dirichlet_fraction * noise)
            root.priors /= root.priors.sum()
        for _ in range(max(self.simulations, 1)):
            self._simulate(root)
        visits = root.visits.astype(np.float64)
        if visits.sum() == 0:
            visits = root.priors.astype(np.float64)
        root.close()
        return visits.reshape(ACTIONS, ACTIONS)


def select_decoupled_joint(visits: np.ndarray, value_sum: np.ndarray,
                           priors: np.ndarray, c_puct: float) -> int:
    """Select simultaneous actions with opposing zero-sum objectives.

    Values are stored from player zero's perspective. Player zero maximizes its
    marginal PUCT score while player one independently maximizes the negated
    marginal value. This avoids the cooperative-opponent bug produced by taking
    a single argmax over joint-action values.
    """
    visit_matrix = visits.reshape(ACTIONS, ACTIONS)
    value_matrix = value_sum.reshape(ACTIONS, ACTIONS)
    prior_matrix = priors.reshape(ACTIONS, ACTIONS)
    total = int(visits.sum())
    scale = np.sqrt(total + 1.0)

    visits_zero = visit_matrix.sum(axis=1)
    values_zero = value_matrix.sum(axis=1)
    priors_zero = prior_matrix.sum(axis=1)
    q_zero = np.divide(values_zero, visits_zero, out=np.zeros(ACTIONS, np.float32),
                       where=visits_zero > 0)
    scores_zero = q_zero + c_puct * priors_zero * scale / (1.0 + visits_zero)
    scores_zero[priors_zero <= 0.0] = -np.inf

    visits_one = visit_matrix.sum(axis=0)
    values_one = value_matrix.sum(axis=0)
    priors_one = prior_matrix.sum(axis=0)
    q_one = np.divide(-values_one, visits_one, out=np.zeros(ACTIONS, np.float32),
                      where=visits_one > 0)
    scores_one = q_one + c_puct * priors_one * scale / (1.0 + visits_one)
    scores_one[priors_one <= 0.0] = -np.inf

    return int(np.argmax(scores_zero)) * ACTIONS + int(np.argmax(scores_one))


def sample_joint(visits: np.ndarray, temperature: float,
                 rng: np.random.Generator) -> tuple[int, int]:
    flat = visits.reshape(-1).astype(np.float64)
    if temperature <= 1e-6:
        choice = int(np.argmax(flat))
    else:
        weights = np.power(flat + 1e-12, 1.0 / temperature)
        weights /= weights.sum()
        choice = int(rng.choice(len(weights), p=weights))
    return choice // ACTIONS, choice % ACTIONS
