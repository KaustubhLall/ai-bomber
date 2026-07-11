#ifndef BOMBER_NATIVE_ALPHAZERO_TRAINER_H
#define BOMBER_NATIVE_ALPHAZERO_TRAINER_H

#include "training/native/model.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <set>
#include <string>

namespace bomber::az {

struct TrainConfig {
    std::filesystem::path run_dir{"results/alphazero-native"};
    std::filesystem::path checkpoint{"latest.pt"};
    std::filesystem::path evaluation_output{};
    /* If set (evaluate mode), write a v4 replay of one representative checkpoint game that
       bomber_viz --replay can play back. Opponent = heuristic, or MCTS with --eval-mcts;
       set replay_incumbent to instead record checkpoint-vs-checkpoint. */
    std::filesystem::path replay_output{};
    std::filesystem::path replay_incumbent{};
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
    double temperature{1.0};
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
    double promotion_margin{0.0};
    double promotion_confidence_z{1.6448536269514722};
    double random_score_floor{0.95};
    double heuristic_score_floor{0.55};
    double heuristic_regression_margin{0.03};
    bool fresh{false};
    bool progress{true};
    bool evaluate_mcts{false};
    bool evaluation_only{false};
    /* A checkpoint saved before the semantic-manifest fix (KL-101) has no verified record of
       what reward/mechanics/schedule values it was actually trained under - the flat
       runtime_config signature never included arena_crush_win_value, selfkill_win_value, or
       league_heuristic_fraction at all, so those fields cannot be reconstructed from the
       checkpoint file itself. Loading such a checkpoint for resume/evaluate fails clearly
       unless this is set, in which case the CLI-provided values are used as-is with a loud,
       unmissable "UNVERIFIED SEMANTICS" warning - never silently. CLI
       --legacy-accept-unverified-semantics. */
    bool legacy_accept_unverified_semantics{false};
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

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

void benchmark_model(int argc, char** argv, int first_argument);

}  // namespace bomber::az

#endif
