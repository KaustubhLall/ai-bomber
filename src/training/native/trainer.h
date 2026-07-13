#ifndef BOMBER_NATIVE_ALPHAZERO_TRAINER_H
#define BOMBER_NATIVE_ALPHAZERO_TRAINER_H

#include "training/native/model.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace bomber::az {

struct TrainConfig {
    std::filesystem::path run_dir{"results/alphazero-native"};
    std::filesystem::path checkpoint{"latest.pt"};
    std::filesystem::path evaluation_output{};
    /* KL-108 Brick 2: if set (evaluate mode with --eval-mcts), write one JSON line per
       completed MCTS-baseline match (seed, seat, outcome, cause, steps, WAIT) to this path.
       Evidence paths fail closed when they already exist unless --overwrite-evidence is
       explicitly supplied; this prevents a later gate from silently replacing an earlier
       result. CLI --per-match-output. */
    std::filesystem::path per_match_output{};
    /* KL-107: if set (evaluate mode with --eval-mcts), write one JSON line per LEARNER STEP
       across all MCTS-baseline matches. policy_prior_after_safety_mask/search_value_estimate
       are the network policy AFTER the engine's safe-action mask and renormalization, and the
       search backup average, respectively - not an unmasked policy-head output or raw value
       head. v3 adds a genuinely raw pre-mask recomputation (policy_head_raw_recomputed,
       value_head_raw_recomputed, via a dedicated single-position forward pass on the same root
       the search evaluated), the safe_action_mask used to derive the masked prior, a
       wait_forced flag (idling was the position's only safe action, not a preference), and
       root Q per action (search_root_q_values, marginalized from existing search backup data).
       v4 adds opponent_modeled_as (what the search's internal lookahead assumed for the
       opposing seat - "self" or a fixed baseline agent type name) and learner_moved (false for
       WAIT/PLACE_BOMB by construction, and false for a movement action blocked by terrain or a
       lost simultaneous-move collision - "effective idle" beyond explicit WAIT). CLI
       --trace-output. */
    std::filesystem::path trace_output{};
    /* If set (evaluate mode), write a v4 replay of one representative checkpoint game that
       bomber_viz --replay can play back. Opponent = heuristic, or MCTS with --eval-mcts;
       set replay_incumbent to instead record checkpoint-vs-checkpoint. */
    std::filesystem::path replay_output{};
    std::filesystem::path replay_incumbent{};
    /* KL-105 Phase 2b: `gates` subcommand only. Empty (default) = the loaded checkpoint's
       network (search mode + raw mode, both run and recorded). Non-empty = substitute a
       scripted Agent (agent_parse_type name: random/scripted/heuristic/greedy/enemy-bot/
       external/alpha-beta/mcts/evasive) for the learner entirely - the achievability
       reference that proves a gate is solvable by *something*, not just an aspirational
       target no policy could ever pass. CLI --gates-agent NAME. Reuses --output (write-once
       evidence, same as evaluate) rather than a dedicated gates output flag. */
    std::string gates_agent{};
    /* KL-110 Phase B: `gates` subcommand only, search mode. "self" (default) = today's
       behavior, unchanged - search's own internal lookahead always models BOTH seats with the
       network, uniformly across all six scenarios, regardless of what the outer loop's actual
       opponent_mode is that step. "aligned" = search's internal opponent model instead matches
       the scenario's REAL opponent this step (NONE -> fixed WAIT, CONSTANT -> the scenario's
       fixed action, AGENT(type) -> that type via the existing baseline_action() path) - see
       gate_search_constraint() in trainer.cpp. Purpose: a raw-pass/search-fail inversion (see
       docs/experiment-memory/12-post-audit-execution.md's "Load-bearing new observation" on the
       trap gate) that disappears under "aligned" implicates opponent-model mismatch
       specifically, not generic value suppression - the two are otherwise confounded because
       "self" always searches against a full-strength mirror even when the real opponent that
       step is a WAIT-only or CONSTANT-action stub. Validated to be exactly "self" or "aligned"
       (validate_config) - any other value fails closed rather than silently falling back to a
       default. Not a semantic field (unlike replay_cause_balance_cap): it changes what a
       read-only diagnostic probe measures, never what a checkpoint is trained under, so it is
       deliberately NOT in the manifest/runtime_config_signature/semantic-fork machinery - same
       treatment as --gates-agent. CLI --gates-opponent-model NAME. */
    std::string gates_opponent_model{"self"};
    /* v7 Stage 0 item 0.4 (docs/experiment-memory/14-v7-from-scratch-design.md; SEARCH-CONTEMPT
       PROTOTYPE): `gates` subcommand only. false (default) = today's behavior, unchanged - every
       gates SearchConstraint has contempt_seat==-1 (off), exactly like before this field
       existed. true = gate_search_constraint() sets contempt_seat=1 (the scenario opponent; the
       gates learner is always seat 0) on every constraint it builds, composing with
       gates_opponent_model: under "self" this is the interesting case (freezes the self-model's
       seat-1 PUCT adaptation past --search-contempt-nscl visits - see SearchConstraint::
       contempt_seat and search_contempt_nscl below); under "aligned" it is a documented no-op
       (aligned already one-hots seat 1's policy to a single fixed action at every node, so seat
       1's marginal visit distribution - snapshotted or not - is already 100% concentrated on
       that one action; freezing changes nothing). Not a semantic field (unlike
       search_contempt_nscl itself): it changes what a read-only diagnostic probe's internal
       lookahead assumes, never what a checkpoint is trained under - same treatment as
       --gates-agent/--gates-opponent-model, deliberately NOT in the manifest/
       runtime_config_signature/semantic-fork machinery. CLI --gates-search-contempt. */
    bool gates_search_contempt{false};
    /* If > 0 and replay_incumbent is set, run a full N-game statistical mirror-match
       (checkpoint vs replay_incumbent, both seats, with the same win-cause/WAIT behavior
       instrumentation as the baseline evals) instead of just the single replay-recording
       game. Diagnostic: the closest available proxy to "what does self-play look like at
       this skill level" without instrumenting the live training loop. */
    int incumbent_eval_games{0};
    int width{13};
    int height{11};
    int max_steps{200};
    int crate_density{50};
    int flame_duration{2}; /* ticks a blast tile stays lethal (persistent flame) */
    int sudden_death_start{120}; /* step the arena starts closing inward (0 disables) */
    int shrink_interval{4};      /* steps between inward wall rings during sudden death */
    int iterations{10'000};
    int self_play_games{128};
    int simulations{96};
    int train_steps{128};
    int batch_size{1024};
    int replay_capacity{200'000};
    int channels{128};
    int residual_blocks{10};
    int teacher_games{32};
    int teacher_iterations{20};
    int evaluation_interval{5};
    int evaluation_games{32};
    int evaluation_simulations{64};
    int promotion_games{128};
    int promotion_simulations{96};
    int mcts_evaluation_interval{25};
    int mcts_evaluation_games{4};
    int baseline_mcts_simulations{512};
    int baseline_mcts_depth{16};
    uint64_t evaluation_seed_base{900'001};
    uint64_t promotion_seed_base{1'100'001};
    uint64_t mcts_evaluation_seed_base{1'300'001};
    int snapshot_interval{10};
    int temperature_steps{30};
    int seed{1};
    double learning_rate{2.0e-4};
    double min_learning_rate{2.0e-5};
    int64_t learning_rate_schedule_start_update{0};
    int64_t learning_rate_schedule_updates{0};
    double weight_decay{1.0e-4};
    double c_puct{1.5};
    double dirichlet_alpha{0.3};
    double dirichlet_fraction{0.25};
    /* v7 Stage 0 item 0.2 (docs/experiment-memory/14-v7-from-scratch-design.md; KataGo forced
       playouts + policy-target pruning, Wu arXiv:1902.10565 S4.1-4.2, independently replicated
       by Trudeau & Bowling 2023): a semantic field riding the FULL manifest machinery exactly
       like temperature_final - persisted/inherited/explicit-override-logged, part of
       runtime_config_signature, config.json, and semantic_field_flags().
         - forced_playouts_k<=0.0 (default 0.0, "off"): EXACTLY today's root selection, bit for
           bit - BatchedMcts::search()'s root-only forcing hook is never invoked (see the
           path.empty()-gated branch in search()), so a legacy checkpoint or any run that never
           passes --forced-playouts-k is unaffected.
         - forced_playouts_k>0.0: during COLLECTION searches only (root_noise==true - mirror
           self-play and league play; eval/gates/promotion always pass root_noise==false and are
           therefore never affected regardless of this value), a root action a for seat s is
           FORCED - selected regardless of its PUCT score - whenever that seat's marginal visit
           count n(a) < sqrt(forced_playouts_k * P(a) * N), P(a) the seat's POST-noise marginal
           root prior and N the root's total simulation count so far; the most-starved qualifying
           action (largest deficit) wins if more than one qualifies. Guarantees every noised
           action a minimum, prior-scaled visit floor that grows with sqrt(N), directly countering
           measured prior starvation (BOMB visits starve below prior ~0.08 - see doc 14 section
           1.3). The POLICY TRAINING TARGET (not action selection, which stays on unpruned
           visits) is then pruned of forced-only visits at collection time - see
           policy_target_pruning.h - so the network is not taught "forced == good" merely from
           the forcing itself. CLI --forced-playouts-k X, must lie in [0, 10]. */
    double forced_playouts_k{0.0};
    /* v7 Stage 0 item 0.4 (docs/experiment-memory/14-v7-from-scratch-design.md; SEARCH-CONTEMPT
       PROTOTYPE, Joshi 2025 arXiv:2504.07757, adapted to decoupled simultaneous PUCT; our H3a):
       a semantic field riding the FULL manifest machinery exactly like forced_playouts_k -
       persisted/inherited/explicit-override-logged, part of runtime_config_signature,
       config.json, and semantic_field_flags() - even though, as of this prototype, only the
       `gates` subcommand ever constructs a SearchConstraint with contempt_seat>=0 (see
       gate_search_constraint(), --gates-search-contempt) and gates never trains or mutates
       weights. It rides the full machinery anyway because it changes what search COMPUTES
       during any run that enables it - the same standard forced_playouts_k is held to, and the
       one that matters if a later, separately-gated evaluation ever adopts this in a training
       call site.
         - search_contempt_nscl<=0 (default 0, "off"): BatchedMcts::select_joint_contempt() is
           never invoked (search()'s descent-loop guard requires BOTH a root's
           constraint.contempt_seat>=0 AND this field >0) - every existing call site, including
           every gates run that never passes --gates-search-contempt, is unaffected, bit for bit.
         - search_contempt_nscl>0 AND a root's constraint.contempt_seat is s>=0 (gates-only for
           now - see SearchConstraint::contempt_seat's own doc comment): at ANY node (root or
           interior) reached during that root's search, once the node's total visits first
           exceed this threshold, seat s's marginal action stops being chosen by PUCT argmax and
           is instead SAMPLED from seat s's marginal visit distribution AS IT STOOD at that
           freeze moment (snapshotted once per node - a small side map in BatchedMcts, keyed by
           Node* and cleared at the start of every search() call - and reused thereafter for the
           rest of THIS search() call). The other seat's selection is never modified. Caps how
           perfectly the modeled opponent is allowed to punish a commitment move (e.g. BOMB) as
           simulation count grows within one simulation budget, adapted from Joshi 2025's
           alternating-turn-chess mechanism (freeze the visit distribution into a fixed
           stochastic policy past N_scl visits) to this engine's decoupled simultaneous PUCT,
           per seat. constraint.contempt_seat>=0 additionally REQUIRES root_noise==false
           (BatchedMcts::search() throws otherwise) - contempt never coexists with
           Dirichlet-noised collection by construction; training call sites (self-play/league)
           never set contempt_seat, so this throw is unreachable from any current CLI path. CLI
           --search-contempt-nscl X, must lie in [0, 10000]. */
    int search_contempt_nscl{0};
    double temperature{1.0};
    /* v7 Stage 0 item 0.1+0.3 (docs/experiment-memory/14-v7-from-scratch-design.md): the sample
       temperature used for step < temperature_steps of self-play/league collection (argmax
       after) has two selectable shapes, both riding the FULL semantic manifest machinery
       exactly like replay_cause_balance_cap - persisted/inherited/explicit-override-logged,
       part of runtime_config_signature, config.json, and semantic_field_flags():
         - temperature_anneal=false (default): EXACTLY today's step-function behavior, bit for
           bit - temperature for step < temperature_steps, 0 (argmax) after. Every checkpoint
           trained before this field existed, and every run that never passes
           --temperature-anneal, is unaffected. See resolve_temperature() in trainer.cpp.
         - temperature_anneal=true: linear anneal from `temperature` at step 0 down to
           `temperature_final` at step temperature_steps, then argmax after (v7 passes
           1.0 -> 0.25 over 60 steps - data-diversity collapse after step 30 was measured in
           idle-streak structure under the old step function). validate_config requires
           temperature_steps > 0 whenever anneal is requested (the linear fraction's
           denominator) and temperature_final in [0, temperature].
       CLI --temperature-final X, --temperature-anneal (flag). Dirichlet alpha (item 0.1) needs
       no struct change - dirichlet_alpha is already a semantic field below; v7 sets it
       explicitly (1.5) in its launcher rather than changing this compiled default, per this
       project's explicit-over-default discipline. */
    double temperature_final{0.0};
    bool temperature_anneal{false};
    double bootstrap_value_weight{0.75};
    int bootstrap_value_iterations{30};
    /* Draw-averse value targets: a both-alive timeout stall is valued at
       timeout_draw_value per seat (negative), a mutual death at mutual_death_value.
       Both are negative so forcing a decisive result beats running out the clock.
       The aggression dial: more negative => the agent demands worse win:loss odds before
       settling (see docs/REWARD_AND_MODEL_DESIGN.md). CLI --draw-value X sets both at once;
       --timeout-draw-value / --mutual-death-value override per seat. Kept at -0.5/-0.2 by
       default (the trained regime); the tactical shaping shrinks as these harden toward -1. */
    double timeout_draw_value{-0.5};
    double mutual_death_value{-0.2};
    /* Full WIN value (+1.0) is reserved for a DEMONSTRATED kill: death_owner of the loser is
       the WINNER's own bomb. Two other decisive-win causes are still a win but show no
       offensive skill and are devalued separately (kept as two knobs, not one, since they may
       need different treatment later - e.g. a trap-induced self-kill arguably reflects some
       skill, a pure arena-crush reflects none):
         - arena_crush_win_value: loser died to the closing sudden-death arena (death_owner
           == -1) - pure attrition, zero interaction with the opponent.
         - selfkill_win_value: loser died to its OWN bomb - the opponent's mistake, not yours.
       Losses are NOT reweighted by cause (a loss stays -1.0 regardless). Both default to the
       same value; see docs/experiment-memory/07-grokking-campaign.md's honest-behavior audit
       for why (95-97% of eval wins were uncontested; self-play mirrors confirm ~99% of
       self-play wins are crush or opponent-self-kill, ~1% a landed bomb kill). CLI
       --arena-crush-win-value / --selfkill-win-value. Must be in (0, 1]. */
    double arena_crush_win_value{0.3};
    double selfkill_win_value{0.3};
    /* Fraction of each iteration's self-play games (in [0,1]) played against the heuristic
       agent instead of a mirror of the current network, one randomly-chosen seat per game
       (balanced across games so both seats get league experience). Training samples are
       collected for the network-controlled seat only. Pure self-play mirrors structurally
       suppress clean bomb kills (both seats share identical dodge skill by construction);
       heuristic doesn't self-destruct, so a league win against it is an earned kill for the
       corrected reward to reinforce. 0 (default) = pure self-play, unchanged behavior. CLI
       --league-heuristic-fraction. */
    double league_heuristic_fraction{0.0};
    /* KL-105 Phase 3 (docs/experiment-memory/13-kl105-experiment-design.md section 3): every
       replay sample is tagged at collection with the finished game's outcome cause and whether
       the sample's seat was the bomb-kill WINNING side (see Sample::outcome_cause/bomb_win_side
       in trainer.cpp). This cap biases training batches toward that winning-side pool, capped
       both by this fraction of the batch and by ReplayBuffer's kBoostMax oversampling bound.
       0 (default) = off = the pre-KL-105 uniform sampler, bit-for-bit unchanged (tagging still
       happens - tags are inert bookkeeping until cap>0 makes the sampler read them). A SEMANTIC
       field (persisted/inherited/logged as a fork exactly like arena_crush_win_value) because
       it changes what the trained weights actually saw reinforced during optimization, not
       just a search/eval budget. Must lie in [0, 0.9] - see kBoostMax in trainer.cpp for why
       the cap itself is bounded well under 1.0. CLI --replay-cause-balance-cap. */
    double replay_cause_balance_cap{0.0};
    double promotion_margin{0.0};
    double promotion_confidence_z{1.6448536269514722};
    double random_score_floor{0.95};
    double heuristic_score_floor{0.55};
    double heuristic_regression_margin{0.03};
    bool fresh{false};
    bool progress{true};
    bool evaluate_mcts{false};
    bool evaluation_only{false};
    /* Evaluation evidence is write-once by default. This explicit opt-in is intended for
       disposable tests or a deliberate rerun after the old evidence has been preserved. */
    bool overwrite_evidence{false};
    /* A checkpoint saved before the semantic-manifest fix (KL-101) has no verified record of
       what reward/mechanics/schedule values it was actually trained under - the flat
       runtime_config signature never included arena_crush_win_value, selfkill_win_value, or
       league_heuristic_fraction at all, so those fields cannot be reconstructed from the
       checkpoint file itself. Loading such a checkpoint for resume/evaluate fails clearly
       unless this is set, in which case the CLI-provided values are used as-is with a loud,
       unmissable "UNVERIFIED SEMANTICS" warning - never silently. CLI
       --legacy-accept-unverified-semantics. */
    bool legacy_accept_unverified_semantics{false};
    /* KL-101 Part C: explicit fork - seed THIS run's lineage (weights, optimizer, replay, RNG,
       semantics, champion/promotion state) from an external checkpoint, rather than the
       ad-hoc "copy latest.pt into a new run-dir, then resume normally" convention used before
       this existed (which is invisible to the trainer and is what caused the iter-110 fake
       "initial promotion" bug - see reconcile_champion_state()/has_incumbent in trainer.cpp).
       Must be combined with --fresh (a fork establishes a new lineage/run-dir, it is not an
       ordinary resume). Requires the SAME model ABI (channels/blocks/observation) as the
       parent; semantic fields are inherited from the parent exactly like a normal resume
       (apply_semantic_manifest), and any explicit CLI override at fork time is captured as
       the fork's own config diff, not silently applied. CLI --fork-from PATH. */
    std::filesystem::path fork_from{};
    /* Only meaningful with --fresh --fork-from (fails closed otherwise): instead of inheriting
       the parent's champion artifact/lineage, the child's champion history starts at its own
       fork point - best_iteration = the fork iteration, best.pt = a freshly-saved checkpoint
       of the just-loaded fork-point weights (self-consistent by construction: artifact
       iteration == best_iteration, so this is NOT the historical relabel-current-weights-as-
       an-older-champion bug), best_score/promotion_count reset to 0. Exists for forking a
       parent whose OWN champion state is historically inconsistent (pre-KL-101 corruption
       that inherit_champion_artifact correctly refuses) without mutating the parent's run
       dir, which is retained evidence. Champion state never feeds collection/optimization in
       this trainer - it only gates promotion telemetry - so a reset is behaviorally inert for
       training and identical across arms that share it. Recorded as "champion_reset": true in
       fork-manifest.json. CLI --fork-reset-champion. */
    bool fork_reset_champion{false};
    /* Optional, opaque to the trainer: a working-tree "dirty diff" digest computed by the
       CALLING script (e.g. `git diff | sha256`) and recorded verbatim in fork-manifest.json.
       The trainer does not shell out to git itself (fragile - needs git on PATH, assumes a
       working directory, etc.); AI_BOMBER_GIT_SHA already captures the committed HEAD at
       compile time, this covers uncommitted changes at fork time. CAVEAT: AI_BOMBER_GIT_SHA is
       resolved at CMake CONFIGURE time (CMakeLists.txt, `git rev-parse` in an execute_process),
       not at every build - a `cmake --build` alone after new commits reuses the stale value from
       the last configure. Re-run `cmake -S . -B <builddir>` before trusting a binary's stamped
       git_commit as current; executable_sha256 is the only field that's always accurate, since
       it hashes the actual bytes regardless of what configure step produced them.
       CLI --dirty-diff-digest. */
    std::string dirty_diff_digest{};
    /* Exact argv captured at parse time and embedded in evaluation evidence. */
    /* Exact process argv retained as separate strings for evidence JSON. A reconstructed shell
       command is not lossless on Windows (backslashes, quotes, and empty arguments), so the
       aggregate bundle serializes this vector directly. */
    std::vector<std::string> invocation_argv{};
    /* Populated by parse_train_config() from which semantic CLI flags were literally present
       in argv (as opposed to left at their struct default). Read by load_checkpoint() to
       decide, per semantic field: inherit the checkpoint's stored value (flag absent - the
       common resume/evaluate case) or keep the CLI's value and log an explicit semantic-fork
       diff (flag present - a deliberate override). Not part of any signature/manifest itself;
       parse-time metadata threaded through because Impl only sees the resolved TrainConfig. */
    std::set<std::string> explicit_semantic_flags{};
};

TrainConfig parse_train_config(int argc, char** argv, int first_argument);
void print_native_help();

class Trainer {
public:
    explicit Trainer(TrainConfig config);
    ~Trainer();
    Trainer(const Trainer&) = delete;
    Trainer& operator=(const Trainer&) = delete;

    void run();
    void evaluate_only();
    void gates();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

void benchmark_model(int argc, char** argv, int first_argument);

}  // namespace bomber::az

#endif
