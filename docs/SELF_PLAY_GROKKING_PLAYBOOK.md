# Reusable self-play and long-horizon generalization playbook

## Scope

This is the portability contract for applying the native AlphaZero pipeline to
another deterministic, finite-action game. It separates three questions that
are easy to blur together:

1. Does the implementation execute and recover correctly?
2. Does training improve against known opponents?
3. Does a late, durable generalization transition occur after extended
   optimization, sometimes called grokking?

Passing one question does not answer the next. In particular, grokking is a
hypothesis to test, not a promised property of a large network or long run.

## Game adapter contract

Every new game must supply one authoritative implementation of these operations:

| Operation | Contract |
| --- | --- |
| `reset(seed)` | Produce the same initial state for the same config and seed. |
| `clone(state)` | Make an isolated, exact copy with no shared mutable policy state. |
| `encode(state, perspective)` | Return a fixed-shape, perspective-aware tensor. |
| `legal/safe_mask(state, perspective)` | Mask impossible actions; distinguish tactical safety only if the game defines it. |
| `step_joint(actions)` | Apply simultaneous actions under one documented collision/order rule. Turn games may use a one-player joint tuple. |
| `outcome(state, perspective)` | Return `-1`, `0`, or `+1` only at terminal states. |
| `training_value(state, perspective)` | Prefer terminal outcome; any draw shaping must be bounded, symmetric, and documented. |
| `state_hash(state)` | Be stable across clones and useful for deterministic search/baseline seeding. |
| baseline action | Expose at least random and one competent deterministic opponent. |

The encoder belongs next to the game, not inside a framework frontend. Test
that every frontend receives byte-equivalent features and masks.

## Search contract

For alternating zero-sum games, standard PUCT selects one action and negates
value at each ply. For simultaneous two-player games, do not take a cooperative
argmax over joint actions. Store value from one fixed perspective, then:

- player zero maximizes its marginal PUCT score;
- player one maximizes the negated marginal score;
- the selected pair forms the joint action;
- root joint visits are marginalized into one policy target per seat.

Batch one unevaluated leaf from many independent roots before each network
forward. Keep tree traversal and state cloning native. GPU utilization comes
from the number of active roots, not from crossing a language boundary for each
node.

## Model sizing

Begin with a residual model large enough to represent the relevant spatial and
temporal motifs, then benchmark it on the target hardware. Record:

- input shape, channels, blocks, heads, parameter count, and precision;
- inference batch size, milliseconds/batch, and positions/second;
- optimizer batch size, updates/second, peak VRAM, and host replay memory;
- self-play games/hour at the intended search budget.

Choose the model that maximizes validation improvement per wall-clock hour.
Parameter count alone is not a capacity proof. If a larger model learns more
slowly but later overtakes, retain both curves; that crossover is scientifically
useful.

## Seed partitions

Define immutable, non-overlapping ranges before the long run:

| Split | Purpose | May select checkpoints? |
| --- | --- | --- |
| self-play | Generate replay and curriculum games. | Yes, indirectly through training. |
| validation | Frequent random/heuristic/league checks. | Yes. |
| diagnostic | Infrequent stronger-search checks and failure analysis. | Prefer no. |
| final holdout | One role-balanced audit after selection freezes. | No. |

Record exact seeds, both seat orientations, game config, search simulations,
temperature, and checkpoint hash. Never move a disappointing holdout block into
training and continue calling it holdout.

## Long-horizon or grokking-oriented protocol

Use a fixed validation distribution and train long after replay fit begins. Plot
or retain the following per iteration:

- policy loss, value loss, total loss, policy entropy, and learning rate;
- replay size/age and unique state hashes if available;
- self-play win/draw length and action distribution;
- random, heuristic, incumbent, and stronger-search W-D-L by seat;
- value calibration grouped by predicted-value bucket;
- wall-clock time, positions/second, games/hour, and checkpoint hash.

A credible late-generalization observation requires all of these:

1. validation performance remains flat or poor while training/replay loss
   improves for an extended interval;
2. validation then improves substantially without changing the seed split,
   evaluator, or rules;
3. the improvement persists across multiple later checkpoints;
4. an untouched final holdout and both seats confirm it;
5. reruns or ablations make the transition at least partly reproducible.

If performance improves steadily, call it ordinary learning. If only the
selection block improves, call it validation overfitting. If the stronger
opponent remains unbeaten, retain that negative result as the next boundary.

## Checkpoint and recovery contract

One recoverable checkpoint must contain every state capable of changing future
training:

- model parameters and non-parameter buffers;
- optimizer and scheduler/scaler state;
- replay contents, capacity position, priorities, and ordering;
- iteration, environment-step, sample, and optimizer-update counters;
- all host and device RNG states actually used after initialization;
- opponent league and curriculum state;
- configuration plus game/encoder/checkpoint schema versions.

Write to a sibling temporary path, flush/close it, then atomically replace the
latest pointer. Periodic immutable snapshots protect against logically valid but
bad recent checkpoints. Ctrl+C should stop at a state boundary and write an
emergency archive. Test recovery by comparing the next deterministic sample or
metric from uninterrupted and interrupted/resumed smoke runs.

## Promotion and claim gates

A practical promotion sequence is:

1. no regression in simulator determinism or native smoke tests;
2. at least 90% score against random on both seats;
3. improvement over the incumbent on a fixed, role-balanced selection block;
4. no material heuristic regression;
5. periodic stronger-search diagnostic;
6. one untouched, role-balanced final holdout after selection freezes.

Always report W-D-L separately. A 50% score composed entirely of draws means
something different from balanced wins and losses. Publish the final checkpoint
hash, config, seeds, hardware, runtime, and evaluator command.

Do not use a point estimate alone for champion promotion. For draw-adjusted
score `(wins + 0.5*draws)/games`, record a predeclared one-sided uncertainty
bound and require it to clear the incumbent threshold. The native trainer uses
a Wilson lower bound with fractional draw credit. This is a conservative
operational rule, not a claim that game outcomes are literally Bernoulli.
Final strength claims should replicate on a second untouched block.

“Superhuman” needs an explicit reference population. If no human match corpus
exists, the defensible claim is narrower: stronger than every predeclared
non-learning/search agent in the environment at the stated compute budgets.
Do not silently shorten that to universal human superiority.

## Reproducibility packet for another game

Before calling the port complete, check in:

- adapter and encoder source plus unit tests;
- exact configure/build commands and dependency versions;
- a one-command CUDA benchmark;
- a one-command end-to-end smoke and resume drill;
- a named long-run preset with resource estimates;
- checkpoint schema and compatibility rules;
- seed partition table and evaluation commands;
- append-only metrics schema;
- the best positive result and the strongest retained negative result.

This packet should let another machine reproduce mechanics first, resume a run
second, and assess strategy third. That order prevents a fast but subtly
different simulator from producing a beautifully graphed non-result.
