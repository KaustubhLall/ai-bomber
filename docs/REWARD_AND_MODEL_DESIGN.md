# Reward design and model architecture — analysis & recommendations

_2026-07-09. Written for the AI-Bomber AlphaZero track (KL-96). Covers two design
questions: (1) scoring draws negative (e.g. −0.9), and (2) ResNet vs Transformer._

---

## 1. Should a draw be worth −0.9 instead of 0?

### Short answer
It is **mathematically valid as reward shaping**, and it is **already configurable** in the
native trainer (`--timeout-draw-value`, `--mutual-death-value`; default −0.5). It is *not*
the game's true zero-sum payoff, so it optimizes a different objective — deliberately trading
"maximize win-rate / never lose" for "force wins, refuse to settle." Whether that helps
depends on the opponent. **Keep it a knob, not a hardcoded assumption** (it is).

### The math (why the number matters)
Model a decision as: attack now with win-probability `p`, loss-probability `q` (the rest is a
draw). Let the draw be worth `d` (0 ≥ d ≥ −1; win = +1, loss = −1). Then

- Value of **attacking** = `p·(+1) + q·(−1) + (1−p−q)·d`
- Value of **playing for the draw** = `d`
- Attacking is preferred iff `p·(1−d) > q·(1+d)`, i.e. **`p/q > (1+d)/(1−d)`**.

That ratio `(1+d)/(1−d)` is the *aggression threshold* — the win:loss odds the agent demands
before it will attack:

| draw value `d` | attack iff win:loss odds exceed | behavior |
| ---: | :---: | --- |
| `0` (true zero-sum) | `1 : 1` | attack only when more likely to win than lose (rational) |
| `−0.5` (current default) | `1 : 3` | attack when win-prob > ⅓ of loss-prob (assertive) |
| `−0.8` | `1 : 9` | attack when win-prob > ~11% of loss-prob (aggressive) |
| `−0.9` | `1 : 19` | attack when win-prob > ~5% of loss-prob (hyper-aggressive) |
| `−1.0` (draw = loss) | `1 : ∞` | attack on *any* win chance (maximally reckless) |

So `d` is a clean, monotonic **aggression dial**. −0.9 makes the agent treat a draw as almost
a loss and gamble hard for wins — the "grandmaster who refuses to draw" you described.

### The tradeoffs (be honest about the costs)
1. **It no longer optimizes win-rate.** The true payoff (win 1 / draw 0 / loss 0 or ½) is what
   "how often do I beat this opponent" means. With `d = −0.9`, the agent will throw *drawable*
   games trying to force a win. Against an **equal or stronger** opponent — where a draw is a
   *good* result — this can **lower** real performance: it gambles and loses games it could
   have held. This directly matters for our current goal (beating **MCTS**): drawing MCTS is a
   fine outcome, so an over-negative draw value could *hurt* the MCTS-ladder score.
2. **Value-head resolution collapses.** The value head is `tanh ∈ (−1, 1)`. Targets become
   win = +1, loss = −1, draw = −0.9 — so **draw and loss are nearly indistinguishable** to the
   network (both ≈ −1), and every non-win state looks equally bad. The value signal that guides
   search loses its gradient between "losing" and "merely not winning." `d = 0` gives maximal
   spread (+1 / 0 / −1); `d = −0.5` keeps draws distinguishable while still penalizing them.
3. **Your concern is real but already largely handled.** "A draw-neutral agent gets whittled
   down / accepts non-winning outcomes" is a genuine risk **when draws are frequent and
   draw-neutral**. But this project already (a) forces draws to be rare via **sudden-death**
   (the arena closes), and (b) penalizes them at **−0.5**, and (c) uses **per-seat value MCTS**
   so the penalty actually reaches move selection. The agent is already strongly draw-averse.
   `−0.9` pushes further along the same axis; it is not turning on a missing behavior.

### Search compatibility (important detail)
Because the search now carries **per-seat value sums** (decoupled general-sum PUCT), a draw
that is negative for *both* seats is representable and *does* steer move selection. (Under the
old single shared `0.5·(v0−v1)` aggregation a symmetric draw penalty cancelled — that was the
v3 bug.) So any `d ∈ [−1, 0]` is consistent end-to-end today. The tanh-saturation caveat (#2)
is the only remaining numeric wrinkle.

### Recommendation
- **Keep `−0.5` as the default** — it is the sweet spot for the superhuman-vs-MCTS goal
  (draw-averse but still values a draw over a loss, preserves value resolution).
- **For a "win-forcing grandmaster" experiment vs weaker/human opponents, try `d ≈ −0.7 to
  −0.8`**, not −0.9/−1.0 (those risk recklessness and value-signal collapse). Run it as a
  separate ablation and compare **win-rate AND loss-rate** vs each opponent — the right `d`
  is opponent-dependent.
- Convenience: a single `--draw-value X` that sets both `timeout_draw_value` and
  `mutual_death_value` (see implementation note below). Also note the terminal-value **tactical
  shaping** (`+0.15·tanh(Δtactical)`) nudges the dominant seat's *timeout* draw toward 0; for a
  *hard* draw value, scale that shaping down so `−0.9` stays close to −0.9.

### Implementation status
Configurable per-seat (`terminal_training_value(env, seat, timeout_draw_value,
mutual_death_value)`; CLI `--timeout-draw-value`, `--mutual-death-value`, validated to
`[−1, 0]`). **`--draw-value X` convenience alias is now implemented** (sets both per-seat values;
`--timeout-draw-value`/`--mutual-death-value` still override per seat). The tanh tactical shaping
in `terminal_training_value` is now scaled by `shape_scale = clamp((1 + d) / 0.5, 0, 1)`, which is
**1 for every draw value at or above the −0.5 default** (so the trained regime is byte-identical)
and ramps to 0 at −1, so a deliberately harsh `−0.9` is not lifted back toward 0 by the +0.15
advantage term (it stays within ≈ ±0.03 of −0.9). **The −0.5/−0.2 default is unchanged** — this is
an opt-in dial, not a new assumption. Built and smoke-tested (`evaluate … --draw-value -0.8`).

---

## 2. ResNet vs Transformer — practical evaluation

### First, the framing correction
The current model is **not an RNN**. It is a **convolutional residual tower (ResNet)**:
`src/training/native/model.cpp` — a 3×3 conv stem → N residual blocks (default 128 channels,
10 blocks, 3.25M params) → a policy head (conv1×1 → FC → 6 logits) and a value head (conv1×1 →
FC → tanh). Input is a **17-channel 11×11** board tensor. There is **no recurrence and no
attention**; each position is scored independently (the game is treated as Markov — the full
board state is the input, with bomb timers and a game-progress channel supplying the little
temporal context that matters).

### Would a Transformer help? When yes, when overkill.

**The receptive-field argument (the key point):** a 10-block ResNet of 3×3 convs has a
receptive field of ±~20 tiles — it **already sees the entire 11×11 board** at every position.
So the usual reason to reach for attention — *long-range dependencies the conv can't reach* —
**does not apply here**. A bomb-chain or an opponent on the far side of the board is already
within the conv's receptive field.

**When a Transformer / attention genuinely helps:**
- **Much larger boards** (e.g. 31×31+) where conv receptive field or parameter efficiency
  becomes limiting. Not our case (11×11 = 121 cells).
- **Variable-size / set-structured inputs** (arbitrary entity lists, graph structure). Not our
  case (fixed dense grid).
- **History / partial observability** — if the optimal policy needed the *sequence* of past
  frames (it mostly doesn't here; state is ~fully observable and Markov). A small temporal
  attention over the last K frames would be the tool *if* we found history-dependent failures.
- **Content-based global routing** — reasoning like "these two specific tiles interact" that
  attention expresses more naturally than convolution. Possible marginal gains; unproven here.

**Why it's likely overkill / risky for this game:**
- **AlphaZero strength ≈ inference throughput.** Search quality scales with simulations/second
  (we run ~64k positions/s at batch 512, which buys the sims that make the agent strong). At an
  11×11 spatial size, a Transformer of comparable quality is typically **slower per position**
  than a cudnn-optimized ResNet (self-attention is O(tokens²) = 121² per layer). Fewer sims/sec
  → **weaker search**, which can net-*hurt* even if per-eval quality is equal.
- **Loss of translation equivariance.** Conv is naturally translation-equivariant, which suits
  a board where local bomb/escape patterns recur everywhere. A ViT needs positional encodings
  and must *learn* that equivariance from data — more data, more finicky training.
- ResNets are the **proven** AlphaZero choice for grid games (Go/Chess/Shogi up to 19×19). For
  a 11×11 board this is squarely in ResNet territory.

**Pipeline changes a Transformer would require (bounded, if you want to try):**
- **Model only** (`model.cpp`) — the observation/replay/training pipeline is **unchanged** (same
  17×11×11 tensor; tokenization happens inside the model). Concretely:
  - Tokenize: treat each cell as a token (121 tokens, each a 17-dim vector → linear to
    `d_model`), or use 2×2/patchified tokens; add 2D positional encoding.
  - Encoder: L pre-norm Transformer blocks (`d_model`, `h` heads, MLP ratio 4), then pool
    (mean or a CLS token) → value head; pooled → policy head (6 logits).
  - Training: same policy-CE + value-MSE losses, but add LR warmup, weight decay, and likely
    dropout/stochastic-depth (Transformers are more regularization-sensitive and data-hungry).
  - **Must benchmark BF16 batched inference throughput** before committing — that number, not
    parameter count, decides whether it helps.
- A **hybrid** (conv stem for local features → 2–4 attention blocks for global mixing → heads)
  is the most promising middle ground if specific long-range failures show up: it keeps conv
  speed/equivariance and adds cheap global routing. This is a smaller, safer change than a full
  ViT.

### Recommendation
**Keep the ResNet.** For strength on this board, spend effort on (a) more MCTS sims per move,
(b) a wider/deeper ResNet (already a CLI knob: `--channels/--blocks`), and (c) better training
(LR schedule, curriculum, opponent league) — all of which we know convert to strength — before
a Transformer rewrite. Reach for attention only if we (1) enlarge the board, (2) add
history/partial-observability, or (3) diagnose concrete long-range reasoning failures; and even
then, prefer a **conv+attention hybrid** benchmarked for inference throughput. I can scaffold a
hybrid behind a `--model-type` flag as a bounded experiment if you want to measure it directly.
