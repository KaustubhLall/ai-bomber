#include "training/native/trainer.h"

extern "C" {
#include "training/encoding.h"
#include "agents/agent.h"
#include "agents/search_agent.h"
#include "core/config.h"
#include "core/replay.h"
#include "env/env.h"
#include "sim/evaluator.h"
}

#include <ATen/autocast_mode.h>
#include <torch/serialize.h>
#include <torch/version.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <numbers>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace bomber::az {
namespace {

constexpr int kActions = BOMBER_TRAINING_ACTIONS;
constexpr int kJointActions = kActions * kActions;
constexpr int kObservationSize = BOMBER_TRAINING_OBSERVATION_SIZE;
constexpr int kFormatVersion = 1;

std::atomic<bool> stop_requested{false};

void signal_handler(int) { stop_requested.store(true); }

/* Self-contained streaming SHA-256 (FIPS 180-4) - no existing hash utility anywhere in this
   codebase, and this is the only place one is needed (KL-101 fork/checkpoint provenance:
   parent checkpoint hash, own executable hash). Deliberately not shelling out to an external
   tool (certutil/sha256sum) from inside the trainer - keeps provenance capture portable and
   dependency-free, matching how the rest of this file avoids extra libraries. Verified against
   the standard test vectors (SHA-256("") and SHA-256("abc")) and cross-checked against
   PowerShell's Get-FileHash on a real file before being trusted for provenance records. */
std::string sha256_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open for hashing: " + path.string());

    static constexpr uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                      0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    const auto rotr = [](uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
    const auto process_block = [&](const unsigned char* data) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = (static_cast<uint32_t>(data[i * 4]) << 24) |
                   (static_cast<uint32_t>(data[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(data[i * 4 + 2]) << 8) |
                   static_cast<uint32_t>(data[i * 4 + 3]);
        for (int i = 16; i < 64; ++i) {
            const uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const uint32_t ch = (e & f) ^ (~e & g);
            const uint32_t temp1 = hh + s1 + ch + k[i] + w[i];
            const uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const uint32_t temp2 = s0 + maj;
            hh = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    };

    std::vector<char> read_buffer(1 << 20);
    unsigned char block[64];
    size_t block_used = 0;
    uint64_t total_length = 0;
    while (file) {
        file.read(read_buffer.data(), static_cast<std::streamsize>(read_buffer.size()));
        const std::streamsize got = file.gcount();
        if (got <= 0) break;
        total_length += static_cast<uint64_t>(got);
        size_t offset = 0;
        while (offset < static_cast<size_t>(got)) {
            const size_t take = std::min(static_cast<size_t>(got) - offset, size_t{64} - block_used);
            std::memcpy(block + block_used, read_buffer.data() + offset, take);
            block_used += take;
            offset += take;
            if (block_used == 64) {
                process_block(block);
                block_used = 0;
            }
        }
    }
    const uint64_t bit_length = total_length * 8;
    block[block_used++] = 0x80;
    if (block_used > 56) {
        while (block_used < 64) block[block_used++] = 0;
        process_block(block);
        block_used = 0;
    }
    while (block_used < 56) block[block_used++] = 0;
    for (int i = 7; i >= 0; --i)
        block[block_used++] = static_cast<unsigned char>((bit_length >> (i * 8)) & 0xff);
    process_block(block);

    std::ostringstream hex;
    hex << std::hex << std::setfill('0');
    for (const uint32_t word : h) hex << std::setw(8) << word;
    return hex.str();
}

/* KL-101 Part D: durable stdout capture. Redirects std::cout's underlying streambuf to write
   to both the real console AND an append-only log file, for the process's lifetime (RAII,
   scoped to run()). One centralized change here captures every existing std::cout print
   statement throughout this file (progress bars, promotion-gate lines, semantic forks,
   behavior reports...) without touching each call site individually - and any future one
   added later, without needing to remember to also log it. This is what an unattended
   watchdog-wrapped overnight run needs to be inspectable after the fact: the live console
   window is not the only place iteration-by-iteration output exists. */
class TeeStreambuf : public std::streambuf {
public:
    TeeStreambuf(std::streambuf* console, std::ostream& file) : console_(console), file_(file) {}

protected:
    int overflow(int character) override {
        if (character != EOF) {
            console_->sputc(static_cast<char>(character));
            file_.put(static_cast<char>(character));
        }
        return character;
    }
    std::streamsize xsputn(const char* data, std::streamsize count) override {
        console_->sputn(data, count);
        file_.write(data, count);
        file_.flush();
        return count;
    }

private:
    std::streambuf* console_;
    std::ostream& file_;
};

class ConsoleTee {
public:
    explicit ConsoleTee(const std::filesystem::path& log_path)
        : log_(log_path, std::ios::app), buf_(std::cout.rdbuf(), log_),
          original_(std::cout.rdbuf(&buf_)) {
        log_ << "\n--- console tee started, pid=" <<
#ifdef _WIN32
            GetCurrentProcessId()
#else
            ::getpid()
#endif
            << " ---\n";
    }
    ~ConsoleTee() { std::cout.rdbuf(original_); }
    ConsoleTee(const ConsoleTee&) = delete;
    ConsoleTee& operator=(const ConsoleTee&) = delete;

private:
    std::ofstream log_;
    TeeStreambuf buf_;
    std::streambuf* original_;
};

/* KL-101 Part E: adds elapsed wall-clock time to accumulator on destruction (RAII, so it's
   recorded even if the timed block throws). Scoped-per-call-site rather than a global
   profiler - this is deliberately the simplest thing that gives Brick 6/KL-102 the
   "phase timings + same-machine baseline" its own ordered plan lists as its first step,
   without building general-purpose profiling infrastructure this project doesn't need yet. */
class ScopedTimer {
public:
    explicit ScopedTimer(double& accumulator)
        : accumulator_(accumulator), started_(std::chrono::steady_clock::now()) {}
    ~ScopedTimer() {
        accumulator_ += std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started_).count();
    }
    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    double& accumulator_;
    std::chrono::steady_clock::time_point started_;
};

/* KL-101 Part E: one iteration's worth of phase timings, all 7 phases KL-101/KL-102 ask for.
   Reset at the top of each run() loop iteration; checkpoint serialization/durable flush
   accumulate (+=) rather than overwrite since save_checkpoint() can be called more than once
   per iteration (latest.pt always, best.pt on promotion). */
struct PhaseTimings {
    double mirror_collection_seconds{};
    double league_collection_seconds{};
    double optimization_seconds{};
    double evaluation_random_seconds{};
    double evaluation_heuristic_seconds{};
    double evaluation_incumbent_seconds{};
    double evaluation_mcts_seconds{};
    double replay_serialization_seconds{};
    double checkpoint_serialization_seconds{};
    double durable_flush_seconds{};
};

std::string format_duration(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0) return "--:--:--";
    auto total = static_cast<long long>(seconds + 0.5);
    const auto hours = total / 3600;
    const auto minutes = (total % 3600) / 60;
    const auto remaining = total % 60;
    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << hours << ':'
           << std::setw(2) << minutes << ':' << std::setw(2) << remaining;
    return output.str();
}

class PhaseProgress {
public:
    PhaseProgress(std::string name, int total, bool enabled)
        : name_(std::move(name)), total_(std::max(total, 1)), enabled_(enabled),
          started_(std::chrono::steady_clock::now()) {}

    void update(int completed, std::string_view suffix = {}) {
        if (!enabled_) return;
        const auto now = std::chrono::steady_clock::now();
        if (completed < total_ && now - last_print_ < std::chrono::milliseconds(250)) return;
        last_print_ = now;
        const double elapsed = std::chrono::duration<double>(now - started_).count();
        const double rate = completed > 0 ? completed / std::max(elapsed, 1e-9) : 0.0;
        const double eta = rate > 0.0 ? (total_ - completed) / rate :
                                       std::numeric_limits<double>::infinity();
        const int width = 28;
        const int filled = std::clamp(completed * width / total_, 0, width);
        std::cout << '\r' << name_ << " [" << std::string(filled, '=')
                  << std::string(width - filled, ' ') << "] "
                  << std::setw(6) << completed << '/' << total_
                  << " ETA " << format_duration(eta);
        if (!suffix.empty()) std::cout << ' ' << suffix;
        if (completed >= total_) std::cout << '\n';
        std::cout << std::flush;
    }

private:
    std::string name_;
    int total_;
    bool enabled_;
    std::chrono::steady_clock::time_point started_;
    std::chrono::steady_clock::time_point last_print_{};
};

class AutocastGuard {
public:
    AutocastGuard() {
        previous_ = at::autocast::is_autocast_enabled(at::kCUDA);
        at::autocast::set_autocast_dtype(at::kCUDA, at::kBFloat16);
        at::autocast::set_autocast_enabled(at::kCUDA, true);
    }
    ~AutocastGuard() { at::autocast::set_autocast_enabled(at::kCUDA, previous_); }
private:
    bool previous_{};
};

struct Sample {
    std::array<at::Half, kObservationSize> state{};
    std::array<float, kActions> policy{};
    float value{};
};

class ReplayBuffer {
public:
    explicit ReplayBuffer(size_t capacity) : capacity_(capacity) {
        samples_.reserve(capacity);
    }

    size_t size() const { return samples_.size(); }
    size_t capacity() const { return capacity_; }

    void add(Sample sample) {
        if (samples_.size() < capacity_) {
            samples_.push_back(std::move(sample));
        } else {
            samples_[next_] = std::move(sample);
            next_ = (next_ + 1) % capacity_;
        }
    }

    void add(std::vector<Sample>& samples) {
        for (auto& sample : samples) add(std::move(sample));
        samples.clear();
    }

    std::tuple<torch::Tensor, torch::Tensor, torch::Tensor> batch(
            int requested, std::mt19937_64& rng) const {
        const int count = std::min<int>(requested, static_cast<int>(samples_.size()));
        std::vector<float> states(static_cast<size_t>(count) * kObservationSize);
        std::vector<float> policies(static_cast<size_t>(count) * kActions);
        std::vector<float> values(count);
        std::uniform_int_distribution<size_t> choose(0, samples_.size() - 1);
        for (int row = 0; row < count; ++row) {
            const auto& sample = samples_[choose(rng)];
            for (int cell = 0; cell < kObservationSize; ++cell)
                states[static_cast<size_t>(row) * kObservationSize + cell] =
                    static_cast<float>(sample.state[cell]);
            std::copy(sample.policy.begin(), sample.policy.end(),
                      policies.begin() + static_cast<size_t>(row) * kActions);
            values[row] = sample.value;
        }
        auto state_tensor = torch::from_blob(states.data(),
            {count, BOMBER_TRAINING_CHANNELS, BOMBER_TRAINING_VIEW_SIZE,
             BOMBER_TRAINING_VIEW_SIZE}, torch::kFloat32).clone();
        auto policy_tensor = torch::from_blob(policies.data(), {count, kActions},
                                               torch::kFloat32).clone();
        auto value_tensor = torch::from_blob(values.data(), {count}, torch::kFloat32).clone();
        return {state_tensor, policy_tensor, value_tensor};
    }

    std::tuple<torch::Tensor, torch::Tensor, torch::Tensor> tensors() const {
        const auto count = static_cast<int64_t>(samples_.size());
        auto states = torch::empty({count, BOMBER_TRAINING_CHANNELS,
                                    BOMBER_TRAINING_VIEW_SIZE,
                                    BOMBER_TRAINING_VIEW_SIZE}, torch::kFloat16);
        auto policies = torch::empty({count, kActions}, torch::kFloat32);
        auto values = torch::empty({count}, torch::kFloat32);
        auto* state_data = states.data_ptr<at::Half>();
        auto* policy_data = policies.data_ptr<float>();
        auto* value_data = values.data_ptr<float>();
        for (int64_t row = 0; row < count; ++row) {
            std::memcpy(state_data + row * kObservationSize, samples_[row].state.data(),
                        sizeof(at::Half) * kObservationSize);
            std::memcpy(policy_data + row * kActions, samples_[row].policy.data(),
                        sizeof(float) * kActions);
            value_data[row] = samples_[row].value;
        }
        return {states, policies, values};
    }

    void load(const torch::Tensor& states, const torch::Tensor& policies,
              const torch::Tensor& values, size_t next) {
        auto state_cpu = states.to(torch::kCPU, torch::kFloat16).contiguous();
        auto policy_cpu = policies.to(torch::kCPU, torch::kFloat32).contiguous();
        auto value_cpu = values.to(torch::kCPU, torch::kFloat32).contiguous();
        const size_t count = std::min<size_t>(state_cpu.size(0), capacity_);
        samples_.clear();
        samples_.resize(count);
        const auto* state_data = state_cpu.data_ptr<at::Half>();
        const auto* policy_data = policy_cpu.data_ptr<float>();
        const auto* value_data = value_cpu.data_ptr<float>();
        for (size_t row = 0; row < count; ++row) {
            std::memcpy(samples_[row].state.data(), state_data + row * kObservationSize,
                        sizeof(at::Half) * kObservationSize);
            std::memcpy(samples_[row].policy.data(), policy_data + row * kActions,
                        sizeof(float) * kActions);
            samples_[row].value = value_data[row];
        }
        next_ = count == capacity_ ? next % capacity_ : 0;
    }

    size_t next() const { return next_; }

private:
    size_t capacity_;
    size_t next_{};
    std::vector<Sample> samples_;
};

BomberConfig game_config(const TrainConfig& config) {
    BomberConfig result;
    config_battle(&result);
    result.width = config.width;
    result.height = config.height;
    result.max_steps = config.max_steps;
    result.crate_density = config.crate_density;
    result.flame_duration = config.flame_duration;
    result.sudden_death_start = config.sudden_death_start;
    result.shrink_interval = config.shrink_interval;
    config_normalize(&result);
    return result;
}

int outcome(const BomberEnv& env, int perspective) {
    const int opponent = 1 - perspective;
    const bool self_alive = env.state.agents[perspective].alive != 0;
    const bool other_alive = env.state.agents[opponent].alive != 0;
    if (self_alive && !other_alive) return 1;
    if (!self_alive && other_alive) return -1;
    /* A decisive elimination on the exact horizon is still a win in the
       authoritative engine. Only both-alive timeouts and mutual deaths draw. */
    return 0;
}

float tactical_value(const BomberEnv& env, int perspective) {
    BomberEnv copy;
    env_copy(&copy, &env);
    copy.opponent = nullptr;
    if (copy.state.step >= copy.config.max_steps) copy.config.max_steps = copy.state.step + 1;
    return evaluator_score_state(&copy, perspective);
}

float terminal_training_value(const BomberEnv& env, int perspective,
                              double timeout_draw_value, double mutual_death_value,
                              double arena_crush_win_value, double selfkill_win_value) {
    const int result = outcome(env, perspective);
    if (result > 0) {
        /* Full value only for a DEMONSTRATED kill: the loser's death_owner is the WINNER's
           own bomb. A win where the loser blew itself up (its own mistake) or was crushed by
           the closing arena (attrition, zero interaction) is decisive but shows no offensive
           skill — devalue both so a search comparing "wait for their mistake / the arena"
           against "force a kill" prefers the latter whenever reachable. See trainer.h's doc
           comment. Losses are NOT reweighted by cause: a loss is a loss. */
        const int loser = 1 - perspective;
        const int died_owner = env.state.death_owner[loser];
        if (died_owner == perspective) return 1.0f;
        if (died_owner == -1)
            return static_cast<float>(std::clamp(arena_crush_win_value, 0.0, 1.0));
        return static_cast<float>(std::clamp(selfkill_win_value, 0.0, 1.0));
    }
    if (result < 0) return -1.0f;
    const bool self_alive = env.state.agents[perspective].alive != 0;
    const bool other_alive = env.state.agents[1 - perspective].alive != 0;
    if (self_alive && other_alive) {
        /* Both-alive timeout is a stall. Value it negatively (per seat, so BOTH sides
           see running out the clock as worse than any win) — this removes the "safe ~0
           draw" attractor that collapses self-play into mutual avoidance. A small tactical
           term keeps a gradient so the dominant side is less penalized and pressing an
           advantage still pays. */
        const double advantage = tactical_value(env, perspective) - tactical_value(env, 1 - perspective);
        /* Scale the tactical nudge down as the draw value hardens toward -1, so a
           deliberately harsh draw (e.g. --draw-value -0.9, "a draw is almost a loss")
           is not lifted back toward 0 by the +0.15 advantage term. The factor is 1 for
           every draw value at or above the -0.5 default (identical to the trained regime)
           and ramps linearly to 0 at -1, where draw==loss leaves no room to shape. */
        const double shape_scale = std::clamp((1.0 + timeout_draw_value) / 0.5, 0.0, 1.0);
        const double shaped = timeout_draw_value + 0.15 * shape_scale * std::tanh(advantage / 125.0);
        return static_cast<float>(std::clamp(shaped, -1.0, 0.0));
    }
    /* Mutual death: engaged but no winner. Mildly negative — worse than a win, but better
       than a passive stall, so trading blows is not discouraged relative to hiding. */
    return static_cast<float>(std::clamp(mutual_death_value, -1.0, 0.0));
}

void encode_state(const BomberEnv& env, int perspective,
                  std::array<at::Half, kObservationSize>& output) {
    std::array<float, kObservationSize> temporary{};
    if (bomber_training_encode_env(&env, perspective, temporary.data(),
                                   kObservationSize) != kObservationSize)
        throw std::runtime_error("C observation encoder failed");
    for (int index = 0; index < kObservationSize; ++index)
        output[index] = at::Half(temporary[index]);
}

int baseline_action(AgentType type, const BomberEnv& env, int perspective, uint64_t seed) {
    Agent agent;
    agent_init(&agent, type);
    agent_reset(&agent, seed);
    Observation observation;
    DebugSnapshot snapshot;
    env_observe(&env, perspective, &observation);
    env_get_debug_snapshot(&env, &snapshot);
    return static_cast<int>(agent_act(&agent, &observation, &snapshot));
}

/* Shared by expand_and_backup's policy masking and KL-107 trace capture, so a trace's
   safe_action_mask always matches what the search itself actually treated as safe for that
   seat - duplicating this logic at both call sites would risk silent drift between what the
   trace reports and what the search enforced. Returns a kActions-sized 0/1 mask indexed by
   action; count_out receives the number of safe actions (falls back to plain legality when
   the tactical safe-action check reports none, matching the search's own fallback exactly). */
std::array<int, kActions> safe_action_mask_for(const BomberEnv& env, int seat, int& count_out) {
    std::array<int, kActions> safe{};
    count_out = bomber_training_safe_actions_env(&env, seat, safe.data(), kActions);
    if (count_out <= 0) {
        Action legal[kActions];
        env_legal_actions(&env, seat, legal, &count_out);
        safe.fill(0);
        for (int legal_index = 0; legal_index < count_out; ++legal_index)
            safe[static_cast<int>(legal[legal_index])] = 1;
    }
    return safe;
}

struct Node {
    explicit Node(const BomberEnv& source, bool is_terminal = false) : terminal(is_terminal) {
        env_copy(&env, &source);
        env.opponent = nullptr;
    }
    BomberEnv env{};
    bool terminal{};
    bool expanded{};
    std::array<float, kJointActions> priors{};
    std::array<int, kJointActions> visits{};
    /* Per-seat value sums (decoupled general-sum PUCT). Each seat backs up and maximizes
       its OWN value, so a draw that is negative for BOTH seats is avoided by both. A single
       shared value_sum would cancel a common-mode draw penalty in the seat-0-minus-seat-1
       aggregation, hiding draw-aversion from the search entirely. */
    std::array<float, kJointActions> value_sum0{};
    std::array<float, kJointActions> value_sum1{};
    std::array<std::unique_ptr<Node>, kJointActions> children{};
};

struct SearchConstraint {
    int fixed_opponent_seat{-1};
    AgentType fixed_opponent_type{AGENT_RANDOM};
    uint64_t opponent_seed{};
};

struct SearchResult {
    std::array<int, kJointActions> visits{};
    /* KL-107: root priors after safe-action masking/renormalization, plus per-seat search
       backup sums. These are useful but are NOT unmasked policy-head probabilities or raw
       value-head outputs; callers must not label them that way. */
    std::array<float, kJointActions> priors{};
    std::array<float, kJointActions> value_sum0{};
    std::array<float, kJointActions> value_sum1{};
};

struct LeafJob {
    Node* leaf{};
    std::vector<std::pair<Node*, int>> path;
    SearchConstraint constraint;
};

class BatchedMcts {
public:
    BatchedMcts(PolicyValueNet model, torch::Device device, const TrainConfig& config,
                std::mt19937_64& rng, int iteration, bool bootstrap_enabled = true)
        : model_(std::move(model)), device_(device), config_(config), rng_(rng),
          iteration_(iteration), bootstrap_enabled_(bootstrap_enabled) {}

    std::vector<SearchResult> search(const std::vector<BomberEnv*>& environments,
                                     const std::vector<SearchConstraint>& constraints,
                                     bool root_noise, int simulations = -1) {
        if (environments.empty()) return {};
        if (constraints.size() != environments.size())
            throw std::runtime_error("MCTS constraint count mismatch");
        const int simulation_count = simulations > 0 ? simulations : config_.simulations;
        std::vector<std::unique_ptr<Node>> roots;
        roots.reserve(environments.size());
        for (auto* env : environments) roots.push_back(std::make_unique<Node>(*env));

        std::vector<LeafJob> initial;
        initial.reserve(roots.size());
        for (size_t index = 0; index < roots.size(); ++index)
            initial.push_back({roots[index].get(), {}, constraints[index]});
        expand_and_backup(initial);
        if (root_noise) {
            for (auto& root : roots) add_root_noise(*root);
        }

        for (int simulation = 0; simulation < std::max(simulation_count, 1); ++simulation) {
            std::vector<LeafJob> leaves;
            leaves.reserve(roots.size());
            std::vector<std::optional<LeafJob>> leaf_slots(roots.size());
#ifdef AI_BOMBER_NATIVE_OPENMP
#pragma omp parallel for schedule(static)
#endif
            for (int64_t root_index = 0;
                 root_index < static_cast<int64_t>(roots.size()); ++root_index) {
                Node* node = roots[root_index].get();
                std::vector<std::pair<Node*, int>> path;
                while (node->expanded && !node->terminal) {
                    const int action = select_joint(*node);
                    path.emplace_back(node, action);
                    if (!node->children[action]) {
                        BomberEnv child_env;
                        env_copy(&child_env, &node->env);
                        child_env.opponent = nullptr;
                        const Action actions[2] = {
                            static_cast<Action>(action / kActions),
                            static_cast<Action>(action % kActions)};
                        const StepResult step = env_step_joint(&child_env, actions, 2);
                        node->children[action] = std::make_unique<Node>(child_env, step.done != 0);
                    }
                    node = node->children[action].get();
                }
                if (node->terminal) {
                    /* Back up each seat's own terminal value so a draw (negative for both)
                       actually steers the search away, not just the value head. */
                    backup(path,
                           terminal_training_value(node->env, 0, config_.timeout_draw_value,
                                                   config_.mutual_death_value,
                                                   config_.arena_crush_win_value,
                                                   config_.selfkill_win_value),
                           terminal_training_value(node->env, 1, config_.timeout_draw_value,
                                                   config_.mutual_death_value,
                                                   config_.arena_crush_win_value,
                                                   config_.selfkill_win_value));
                } else {
                    leaf_slots[root_index] = LeafJob{
                        node, std::move(path), constraints[root_index]};
                }
            }
            for (auto& slot : leaf_slots)
                if (slot) leaves.push_back(std::move(*slot));
            expand_and_backup(leaves);
        }

        std::vector<SearchResult> results(roots.size());
        for (size_t index = 0; index < roots.size(); ++index) {
            results[index].visits = roots[index]->visits;
            results[index].priors = roots[index]->priors;
            results[index].value_sum0 = roots[index]->value_sum0;
            results[index].value_sum1 = roots[index]->value_sum1;
        }
        return results;
    }

private:
    int select_joint(const Node& node) const {
        int total = std::accumulate(node.visits.begin(), node.visits.end(), 0);
        const float scale = std::sqrt(static_cast<float>(total) + 1.0f);
        int selected_zero = 0;
        int selected_one = 0;
        float best_zero = -std::numeric_limits<float>::infinity();
        float best_one = -std::numeric_limits<float>::infinity();
        for (int action = 0; action < kActions; ++action) {
            int visits_zero = 0;
            float values_zero = 0.0f;
            float priors_zero = 0.0f;
            int visits_one = 0;
            float values_one = 0.0f;
            float priors_one = 0.0f;
            for (int opponent = 0; opponent < kActions; ++opponent) {
                const int index_zero = action * kActions + opponent;
                visits_zero += node.visits[index_zero];
                values_zero += node.value_sum0[index_zero];
                priors_zero += node.priors[index_zero];
                const int index_one = opponent * kActions + action;
                visits_one += node.visits[index_one];
                values_one += node.value_sum1[index_one];
                priors_one += node.priors[index_one];
            }
            if (priors_zero > 0.0f) {
                const float q = visits_zero ? values_zero / visits_zero : 0.0f;
                const float score = q + static_cast<float>(config_.c_puct) * priors_zero *
                                          scale / (1.0f + visits_zero);
                if (score > best_zero) { best_zero = score; selected_zero = action; }
            }
            if (priors_one > 0.0f) {
                /* Seat 1 maximizes its OWN accumulated value (no negation): value_sum1
                   already stores seat 1's perspective. */
                const float q = visits_one ? values_one / visits_one : 0.0f;
                const float score = q + static_cast<float>(config_.c_puct) * priors_one *
                                          scale / (1.0f + visits_one);
                if (score > best_one) { best_one = score; selected_one = action; }
            }
        }
        return selected_zero * kActions + selected_one;
    }

    static void backup(const std::vector<std::pair<Node*, int>>& path,
                       float value0, float value1) {
        for (auto it = path.rbegin(); it != path.rend(); ++it) {
            ++it->first->visits[it->second];
            it->first->value_sum0[it->second] += value0;
            it->first->value_sum1[it->second] += value1;
        }
    }

    void add_root_noise(Node& root) {
        std::gamma_distribution<float> gamma(static_cast<float>(config_.dirichlet_alpha), 1.0f);
        std::array<float, kJointActions> noise{};
        float total = 0.0f;
        for (int index = 0; index < kJointActions; ++index) {
            if (root.priors[index] > 0.0f) {
                noise[index] = gamma(rng_);
                total += noise[index];
            }
        }
        if (total <= 0.0f) return;
        const float fraction = static_cast<float>(config_.dirichlet_fraction);
        float normalized = 0.0f;
        for (int index = 0; index < kJointActions; ++index) {
            if (root.priors[index] > 0.0f) {
                root.priors[index] = (1.0f - fraction) * root.priors[index] +
                                     fraction * noise[index] / total;
                normalized += root.priors[index];
            }
        }
        for (auto& prior : root.priors) prior /= normalized;
    }

    void expand_and_backup(std::vector<LeafJob>& jobs) {
        if (jobs.empty()) return;
        std::vector<float> encoded(jobs.size() * 2 * kObservationSize);
        for (size_t index = 0; index < jobs.size(); ++index) {
            for (int seat = 0; seat < 2; ++seat) {
                float* destination = encoded.data() + (index * 2 + seat) * kObservationSize;
                if (bomber_training_encode_env(&jobs[index].leaf->env, seat, destination,
                                               kObservationSize) != kObservationSize)
                    throw std::runtime_error("C observation encoder failed during MCTS");
            }
        }
        auto input = torch::from_blob(encoded.data(),
            {static_cast<int64_t>(jobs.size() * 2), BOMBER_TRAINING_CHANNELS,
             BOMBER_TRAINING_VIEW_SIZE, BOMBER_TRAINING_VIEW_SIZE}, torch::kFloat32)
            .to(device_);
        torch::Tensor logits;
        torch::Tensor values;
        model_->eval();
        {
            torch::InferenceMode inference;
            AutocastGuard autocast;
            std::tie(logits, values) = model_->forward(input);
        }
        auto probabilities = torch::softmax(logits.to(torch::kFloat32), 1).to(torch::kCPU);
        auto values_cpu = values.to(torch::kFloat32).to(torch::kCPU);
        const auto policy = probabilities.accessor<float, 2>();
        const auto value = values_cpu.accessor<float, 1>();

        const double bootstrap_progress = std::min(
            static_cast<double>(iteration_) /
                std::max(config_.bootstrap_value_iterations, 1), 1.0);
        const float heuristic_weight = bootstrap_enabled_ ? static_cast<float>(
            config_.bootstrap_value_weight * (1.0 - bootstrap_progress)) : 0.0f;
        for (size_t index = 0; index < jobs.size(); ++index) {
            Node& node = *jobs[index].leaf;
            std::array<std::array<float, kActions>, 2> seat_policy{};
            for (int seat = 0; seat < 2; ++seat) {
                int count = 0;
                const auto safe = safe_action_mask_for(node.env, seat, count);
                float total = 0.0f;
                for (int action = 0; action < kActions; ++action) {
                    seat_policy[seat][action] = safe[action] ?
                        policy[static_cast<int64_t>(index * 2 + seat)][action] : 0.0f;
                    total += seat_policy[seat][action];
                }
                if (jobs[index].constraint.fixed_opponent_seat == seat) {
                    seat_policy[seat].fill(0.0f);
                    const int action = baseline_action(
                        jobs[index].constraint.fixed_opponent_type, node.env, seat,
                        jobs[index].constraint.opponent_seed ^ env_state_hash(&node.env));
                    seat_policy[seat][action] = 1.0f;
                } else if (total > 0.0f) {
                    for (auto& probability : seat_policy[seat]) probability /= total;
                } else {
                    const float uniform = 1.0f / std::max(count, 1);
                    for (int action = 0; action < kActions; ++action)
                        seat_policy[seat][action] = safe[action] ? uniform : 0.0f;
                }
            }
            float prior_total = 0.0f;
            for (int zero = 0; zero < kActions; ++zero) {
                for (int one = 0; one < kActions; ++one) {
                    const int joint = zero * kActions + one;
                    node.priors[joint] = seat_policy[0][zero] * seat_policy[1][one];
                    prior_total += node.priors[joint];
                }
            }
            if (prior_total > 0.0f)
                for (auto& prior : node.priors) prior /= prior_total;
            node.expanded = true;
            float value0 = value[index * 2];
            float value1 = value[index * 2 + 1];
            if (heuristic_weight > 0.0f) {
                const float tactical_zero = std::tanh(tactical_value(node.env, 0) / 250.0f);
                const float tactical_one = std::tanh(tactical_value(node.env, 1) / 250.0f);
                value0 = (1.0f - heuristic_weight) * value0 + heuristic_weight * tactical_zero;
                value1 = (1.0f - heuristic_weight) * value1 + heuristic_weight * tactical_one;
            }
            backup(jobs[index].path, value0, value1);
        }
    }

    PolicyValueNet model_;
    torch::Device device_;
    const TrainConfig& config_;
    std::mt19937_64& rng_;
    int iteration_;
    bool bootstrap_enabled_;
};

int sample_joint_action(const std::array<int, kJointActions>& visits, float temperature,
                        std::mt19937_64& rng) {
    if (temperature <= 1e-6f) {
        return static_cast<int>(std::distance(visits.begin(),
            std::max_element(visits.begin(), visits.end())));
    }
    std::array<double, kJointActions> weights{};
    for (int index = 0; index < kJointActions; ++index)
        weights[index] = std::pow(static_cast<double>(visits[index]) + 1e-12,
                                  1.0 / temperature);
    std::discrete_distribution<int> choose(weights.begin(), weights.end());
    return choose(rng);
}

double wilson_lower_bound(double score, int games, double z) {
    if (games <= 0) return 0.0;
    const double probability = std::clamp(score, 0.0, 1.0);
    const double z_squared = z * z;
    const double denominator = 1.0 + z_squared / games;
    const double center = probability + z_squared / (2.0 * games);
    const double spread = z * std::sqrt(
        (probability * (1.0 - probability) + z_squared / (4.0 * games)) / games);
    return std::clamp((center - spread) / denominator, 0.0, 1.0);
}

int marginal_action(const std::array<int, kJointActions>& visits, int seat) {
    std::array<int, kActions> marginal{};
    for (int action = 0; action < kActions; ++action) {
        for (int opponent_action = 0; opponent_action < kActions; ++opponent_action) {
            marginal[action] += seat == 0 ? visits[action * kActions + opponent_action] :
                                            visits[opponent_action * kActions + action];
        }
    }
    return static_cast<int>(std::distance(
        marginal.begin(), std::max_element(marginal.begin(), marginal.end())));
}

/* KL-107: marginalize a joint-action array (visits or priors, whichever numeric type) down
   to one seat's per-action distribution - same accumulation pattern as marginal_action()
   above, generalized to return the full array (for tracing) instead of just the argmax. */
template <typename JointArray>
std::array<double, kActions> marginal_distribution(const JointArray& joint, int seat) {
    std::array<double, kActions> marginal{};
    for (int action = 0; action < kActions; ++action) {
        for (int opponent_action = 0; opponent_action < kActions; ++opponent_action) {
            marginal[action] += seat == 0 ? joint[action * kActions + opponent_action] :
                                            joint[opponent_action * kActions + action];
        }
    }
    return marginal;
}

double distribution_entropy(const std::array<double, kActions>& distribution) {
    const double total = std::accumulate(distribution.begin(), distribution.end(), 0.0);
    if (total <= 0.0) return 0.0;
    double entropy = 0.0;
    for (const double value : distribution) {
        if (value <= 0.0) continue;
        const double probability = value / total;
        entropy -= probability * std::log(probability);
    }
    return entropy;
}

void atomic_replace(const std::filesystem::path& temporary,
                    const std::filesystem::path& destination) {
#ifdef _WIN32
    DWORD error = ERROR_SUCCESS;
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (MoveFileExW(temporary.c_str(), destination.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return;
        error = GetLastError();
        if (error != ERROR_SHARING_VIOLATION && error != ERROR_ACCESS_DENIED)
            break;
        Sleep(static_cast<DWORD>(25 * (attempt + 1)));
    }
    throw std::runtime_error("atomic checkpoint replace failed: Windows error " +
                             std::to_string(error));
#else
    std::filesystem::rename(temporary, destination);
#endif
}

void durable_flush(const std::filesystem::path& path) {
#ifdef _WIN32
    HANDLE handle = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        throw std::runtime_error("checkpoint flush open failed: Windows error " +
                                 std::to_string(GetLastError()));
    const BOOL flushed = FlushFileBuffers(handle);
    const DWORD error = flushed ? ERROR_SUCCESS : GetLastError();
    CloseHandle(handle);
    if (!flushed)
        throw std::runtime_error("checkpoint flush failed: Windows error " +
                                 std::to_string(error));
#else
    const int descriptor = ::open(path.c_str(), O_RDONLY);
    if (descriptor < 0 || ::fsync(descriptor) != 0) {
        if (descriptor >= 0) ::close(descriptor);
        throw std::runtime_error("checkpoint flush failed");
    }
    ::close(descriptor);
#endif
}

void atomic_copy_file(const std::filesystem::path& source,
                      const std::filesystem::path& destination) {
    const auto temporary = destination.string() + ".tmp";
    std::filesystem::copy_file(source, temporary,
                               std::filesystem::copy_options::overwrite_existing);
    durable_flush(temporary);
    atomic_replace(temporary, destination);
}

/* KL-101 Part C: this process's own executable path, for self-hashing into fork-manifest.json
   provenance (which exact binary produced this checkpoint). argv[0] is not reliable (may be a
   relative path, or just "bomber_alphazero_native" if found via PATH) - query the OS directly. */
std::filesystem::path current_executable_path() {
#ifdef _WIN32
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                                 static_cast<DWORD>(buffer.size()));
        if (length == 0)
            throw std::runtime_error("GetModuleFileNameW failed: Windows error " +
                                     std::to_string(GetLastError()));
        if (length < buffer.size()) return std::filesystem::path(buffer.data());
        buffer.resize(buffer.size() * 2);
    }
#else
    return std::filesystem::read_symlink("/proc/self/exe");
#endif
}

/* KL-101 Part D: OS-level exclusive lock, held for the process's lifetime, auto-released on
   any exit (normal, crash, or kill) because it's a raw OS handle, not an advisory file the
   process has to remember to delete. Opened with zero share mode - a second process trying to
   open the same path fails immediately with a clear "already running" error instead of the
   two processes silently contending for the GPU, which crashed training twice earlier in this
   session. Every train/evaluate process takes the global lock; training also takes a run-dir
   lock so the ownership rules remain explicit. */
class ProcessLock {
public:
    explicit ProcessLock(const std::filesystem::path& lock_path) : path_(lock_path) {
#ifdef _WIN32
        handle_ = CreateFileW(lock_path.c_str(), GENERIC_READ | GENERIC_WRITE,
                              0 /* no sharing - exclusive */, nullptr, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) {
            const DWORD error = GetLastError();
            throw std::runtime_error(
                "cannot acquire exclusive lock " + lock_path.string() +
                " (Windows error " + std::to_string(error) + ", commonly "
                "ERROR_SHARING_VIOLATION=32 - another bomber_alphazero_native.exe train or "
                "evaluate process is already running; native processes must not contend for "
                "the same GPU)");
        }
        const std::string pid_line = "pid=" + std::to_string(GetCurrentProcessId()) + "\n";
        DWORD written = 0;
        WriteFile(handle_, pid_line.data(), static_cast<DWORD>(pid_line.size()),
                  &written, nullptr);
        FlushFileBuffers(handle_);
#else
        descriptor_ = ::open(lock_path.c_str(), O_CREAT | O_RDWR, 0644);
        if (descriptor_ < 0 || ::flock(descriptor_, LOCK_EX | LOCK_NB) != 0) {
            if (descriptor_ >= 0) ::close(descriptor_);
            descriptor_ = -1;
            throw std::runtime_error(
                "cannot acquire exclusive lock " + lock_path.string() +
                " - another bomber_alphazero_native.exe train or evaluate process is already "
                "running");
        }
        const std::string pid_line = "pid=" + std::to_string(::getpid()) + "\n";
        (void)::write(descriptor_, pid_line.data(), pid_line.size());
#endif
    }
    ~ProcessLock() {
#ifdef _WIN32
        if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
#else
        if (descriptor_ >= 0) { ::flock(descriptor_, LOCK_UN); ::close(descriptor_); }
#endif
    }
    ProcessLock(const ProcessLock&) = delete;
    ProcessLock& operator=(const ProcessLock&) = delete;

private:
    std::filesystem::path path_;
#ifdef _WIN32
    HANDLE handle_{INVALID_HANDLE_VALUE};
#else
    int descriptor_{-1};
#endif
};

torch::Tensor string_tensor(const std::string& value) {
    return torch::from_blob(const_cast<char*>(value.data()),
                            {static_cast<int64_t>(value.size())}, torch::kUInt8).clone();
}

std::string tensor_string(torch::Tensor value) {
    value = value.to(torch::kCPU, torch::kUInt8).contiguous();
    return std::string(reinterpret_cast<const char*>(value.data_ptr<uint8_t>()), value.numel());
}

std::string config_signature(const TrainConfig& config) {
    std::ostringstream output;
    output << kFormatVersion << '|' << config.width << '|' << config.height << '|'
           << config.max_steps << '|' << config.crate_density << '|' << config.channels
           << '|' << config.residual_blocks << '|' << config.replay_capacity;
    return output.str();
}

std::string runtime_config_signature(const TrainConfig& config) {
    std::ostringstream output;
    output << "games=" << config.self_play_games
           << ";simulations=" << config.simulations
           << ";train_steps=" << config.train_steps
           << ";batch_size=" << config.batch_size
           << ";learning_rate=" << config.learning_rate
           << ";min_learning_rate=" << config.min_learning_rate
           << ";lr_schedule_start=" << config.learning_rate_schedule_start_update
           << ";lr_schedule_updates=" << config.learning_rate_schedule_updates
           << ";weight_decay=" << config.weight_decay
           << ";c_puct=" << config.c_puct
           << ";dirichlet_alpha=" << config.dirichlet_alpha
           << ";dirichlet_fraction=" << config.dirichlet_fraction
           << ";temperature=" << config.temperature
           << ";temperature_steps=" << config.temperature_steps
           << ";teacher_games=" << config.teacher_games
           << ";teacher_iterations=" << config.teacher_iterations
           << ";bootstrap_weight=" << config.bootstrap_value_weight
           << ";bootstrap_iterations=" << config.bootstrap_value_iterations
           << ";evaluation_games=" << config.evaluation_games
           << ";evaluation_simulations=" << config.evaluation_simulations
           << ";promotion_games=" << config.promotion_games
           << ";promotion_simulations=" << config.promotion_simulations
           << ";baseline_mcts_simulations=" << config.baseline_mcts_simulations
           << ";baseline_mcts_depth=" << config.baseline_mcts_depth
           << ";seed=" << config.seed;
    return output.str();
}

/* KL-101: fields that change what a checkpoint's WEIGHTS or SEARCH mean - reward shaping,
   league composition, mechanics timing, the resolved LR schedule horizon, and
   exploration/search settings that shape self-play's data distribution - as opposed to (a)
   pure shape/ABI fields (width/height/channels/... - still hard-gated by config_signature()
   above; untouched by this) or (b) pure search-BUDGET fields that legitimately differ
   between training and evaluation by design (simulations, eval-games, batch-size...).
   {internal_key, CLI flag} so load_checkpoint() can both serialize/parse these by name and
   tell whether a field was explicitly requested on this process's own command line. */
const std::vector<std::pair<std::string, std::string>>& semantic_field_flags() {
    static const std::vector<std::pair<std::string, std::string>> fields = {
        {"flame_duration", "--flame-duration"},
        {"sudden_death_start", "--sudden-death-start"},
        {"shrink_interval", "--shrink-interval"},
        {"timeout_draw_value", "--timeout-draw-value"},
        {"mutual_death_value", "--mutual-death-value"},
        {"arena_crush_win_value", "--arena-crush-win-value"},
        {"selfkill_win_value", "--selfkill-win-value"},
        {"league_heuristic_fraction", "--league-heuristic-fraction"},
        {"c_puct", "--c-puct"},
        {"dirichlet_alpha", "--dirichlet-alpha"},
        {"dirichlet_fraction", "--dirichlet-fraction"},
        {"temperature", "--temperature"},
        {"temperature_steps", "--temperature-steps"},
        {"learning_rate", "--learning-rate"},
        {"min_learning_rate", "--min-learning-rate"},
        {"learning_rate_schedule_start_update", "--lr-schedule-start-update"},
        {"learning_rate_schedule_updates", "--lr-schedule-updates"},
        {"seed", "--seed"},
    };
    return fields;
}

std::string semantic_manifest_string(const TrainConfig& config) {
    std::ostringstream output;
    output << "flame_duration=" << config.flame_duration
           << ";sudden_death_start=" << config.sudden_death_start
           << ";shrink_interval=" << config.shrink_interval
           << ";timeout_draw_value=" << config.timeout_draw_value
           << ";mutual_death_value=" << config.mutual_death_value
           << ";arena_crush_win_value=" << config.arena_crush_win_value
           << ";selfkill_win_value=" << config.selfkill_win_value
           << ";league_heuristic_fraction=" << config.league_heuristic_fraction
           << ";c_puct=" << config.c_puct
           << ";dirichlet_alpha=" << config.dirichlet_alpha
           << ";dirichlet_fraction=" << config.dirichlet_fraction
           << ";temperature=" << config.temperature
           << ";temperature_steps=" << config.temperature_steps
           << ";learning_rate=" << config.learning_rate
           << ";min_learning_rate=" << config.min_learning_rate
           << ";learning_rate_schedule_start_update="
           << config.learning_rate_schedule_start_update
           << ";learning_rate_schedule_updates=" << config.learning_rate_schedule_updates
           << ";seed=" << config.seed;
    return output.str();
}

std::map<std::string, std::string> parse_key_value(const std::string& raw) {
    std::map<std::string, std::string> result;
    std::istringstream stream(raw);
    std::string pair;
    while (std::getline(stream, pair, ';')) {
        const auto separator = pair.find('=');
        if (separator == std::string::npos) continue;
        result[pair.substr(0, separator)] = pair.substr(separator + 1);
    }
    return result;
}

/* Reconcile config's semantic fields against a checkpoint's stored manifest: a field left at
   its CLI default is OVERWRITTEN with the checkpoint's stored value (inheritance - this is
   what makes resume/evaluate load semantics from the checkpoint rather than the trainer's
   struct defaults). A field the CLI explicitly requested is left as the CLI's value, and any
   difference from the checkpoint is returned as a human-readable fork line rather than passed
   through silently. Returns the fork lines (empty = pure inheritance, no explicit overrides
   diverged). */
std::vector<std::string> apply_semantic_manifest(TrainConfig& config,
                                                  const std::string& stored_manifest) {
    const auto stored = parse_key_value(stored_manifest);
    std::vector<std::string> forks;
    auto reconcile_int = [&](const char* key, int TrainConfig::* field) {
        const auto it = stored.find(key);
        if (it == stored.end()) return;
        const int stored_value = std::stoi(it->second);
        if (config.explicit_semantic_flags.count(key)) {
            if (config.*field != stored_value) {
                std::ostringstream fork;
                fork << key << ": checkpoint=" << stored_value
                     << " -> explicit CLI=" << (config.*field);
                forks.push_back(fork.str());
            }
        } else {
            config.*field = stored_value;
        }
    };
    auto reconcile_int64 = [&](const char* key, int64_t TrainConfig::* field) {
        const auto it = stored.find(key);
        if (it == stored.end()) return;
        const int64_t stored_value = std::stoll(it->second);
        if (config.explicit_semantic_flags.count(key)) {
            if (config.*field != stored_value) {
                std::ostringstream fork;
                fork << key << ": checkpoint=" << stored_value
                     << " -> explicit CLI=" << (config.*field);
                forks.push_back(fork.str());
            }
        } else {
            config.*field = stored_value;
        }
    };
    auto reconcile_double = [&](const char* key, double TrainConfig::* field) {
        const auto it = stored.find(key);
        if (it == stored.end()) return;
        const double stored_value = std::stod(it->second);
        if (config.explicit_semantic_flags.count(key)) {
            if (std::abs(config.*field - stored_value) > 1e-9) {
                std::ostringstream fork;
                fork << key << ": checkpoint=" << stored_value
                     << " -> explicit CLI=" << (config.*field);
                forks.push_back(fork.str());
            }
        } else {
            config.*field = stored_value;
        }
    };
    reconcile_int("flame_duration", &TrainConfig::flame_duration);
    reconcile_int("sudden_death_start", &TrainConfig::sudden_death_start);
    reconcile_int("shrink_interval", &TrainConfig::shrink_interval);
    reconcile_double("timeout_draw_value", &TrainConfig::timeout_draw_value);
    reconcile_double("mutual_death_value", &TrainConfig::mutual_death_value);
    reconcile_double("arena_crush_win_value", &TrainConfig::arena_crush_win_value);
    reconcile_double("selfkill_win_value", &TrainConfig::selfkill_win_value);
    reconcile_double("league_heuristic_fraction", &TrainConfig::league_heuristic_fraction);
    reconcile_double("c_puct", &TrainConfig::c_puct);
    reconcile_double("dirichlet_alpha", &TrainConfig::dirichlet_alpha);
    reconcile_double("dirichlet_fraction", &TrainConfig::dirichlet_fraction);
    reconcile_double("temperature", &TrainConfig::temperature);
    reconcile_int("temperature_steps", &TrainConfig::temperature_steps);
    reconcile_double("learning_rate", &TrainConfig::learning_rate);
    reconcile_double("min_learning_rate", &TrainConfig::min_learning_rate);
    reconcile_int64("learning_rate_schedule_start_update",
                    &TrainConfig::learning_rate_schedule_start_update);
    reconcile_int64("learning_rate_schedule_updates",
                    &TrainConfig::learning_rate_schedule_updates);
    reconcile_int("seed", &TrainConfig::seed);
    return forks;
}

std::string json_escape(std::string_view value) {
    std::string result;
    for (const char character : value) {
        if (character == '\\' || character == '"') result.push_back('\\');
        result.push_back(character);
    }
    return result;
}

std::string utc_timestamp() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc_time{};
#ifdef _WIN32
    gmtime_s(&utc_time, &now);
#else
    gmtime_r(&now, &utc_time);
#endif
    std::ostringstream output;
    output << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

std::vector<std::string> invocation_arguments(int argc, char** argv) {
    std::vector<std::string> result;
    result.reserve(static_cast<size_t>(argc));
    for (int index = 0; index < argc; ++index) result.emplace_back(argv[index]);
    return result;
}

template <typename T>
T parse_number(int argc, char** argv, int first, const std::string& option, T fallback) {
    for (int index = first; index < argc; ++index) {
        if (argv[index] == option) {
            if (index + 1 >= argc || std::string_view(argv[index + 1]).starts_with("--"))
                throw std::invalid_argument("missing value for " + option);
            std::istringstream input(argv[index + 1]);
            T result{};
            input >> result;
            if (!input || !input.eof()) throw std::invalid_argument("invalid value for " + option);
            return result;
        }
    }
    return fallback;
}

std::string parse_string(int argc, char** argv, int first, const std::string& option,
                         std::string fallback) {
    for (int index = first; index < argc; ++index) {
        if (argv[index] == option) {
            if (index + 1 >= argc || std::string_view(argv[index + 1]).starts_with("--"))
                throw std::invalid_argument("missing value for " + option);
            return argv[index + 1];
        }
    }
    return fallback;
}

bool has_flag(int argc, char** argv, int first, const std::string& option) {
    for (int index = first; index < argc; ++index)
        if (argv[index] == option) return true;
    return false;
}

void validate_train_cli_options(int argc, char** argv, int first) {
    static const std::set<std::string_view> value_options = {
        "--run-dir", "--checkpoint", "--output", "--per-match-output", "--trace-output",
        "--replay-out", "--replay-incumbent", "--incumbent-eval-games", "--width",
        "--height", "--max-steps", "--crate-density", "--flame-duration",
        "--sudden-death-start", "--shrink-interval", "--iterations", "--games",
        "--simulations", "--train-steps", "--batch-size", "--replay-capacity",
        "--channels", "--blocks", "--teacher-games", "--teacher-iterations",
        "--eval-interval", "--eval-games", "--eval-simulations", "--promotion-games",
        "--promotion-simulations", "--mcts-eval-interval", "--mcts-eval-games",
        "--baseline-mcts-simulations", "--baseline-mcts-depth", "--eval-seed-base",
        "--promotion-seed-base", "--mcts-eval-seed-base", "--snapshot-interval",
        "--temperature-steps", "--seed", "--learning-rate", "--min-learning-rate",
        "--lr-schedule-start-update", "--lr-schedule-updates", "--weight-decay",
        "--c-puct", "--dirichlet-alpha", "--dirichlet-fraction", "--temperature",
        "--bootstrap-weight", "--bootstrap-iterations", "--draw-value",
        "--timeout-draw-value", "--mutual-death-value", "--arena-crush-win-value",
        "--selfkill-win-value", "--league-heuristic-fraction", "--promotion-margin",
        "--promotion-confidence-z", "--random-score-floor", "--heuristic-score-floor",
        "--heuristic-regression-margin", "--fork-from", "--dirty-diff-digest",
    };
    static const std::set<std::string_view> flag_options = {
        "--fresh", "--no-progress", "--eval-mcts", "--overwrite-evidence",
        "--legacy-accept-unverified-semantics",
    };
    for (int index = first; index < argc; ++index) {
        const std::string_view option(argv[index]);
        if (flag_options.count(option)) continue;
        if (!value_options.count(option))
            throw std::invalid_argument("unknown native AlphaZero option: " +
                                        std::string(option));
        if (index + 1 >= argc || std::string_view(argv[index + 1]).starts_with("--"))
            throw std::invalid_argument("missing value for " + std::string(option));
        ++index;
    }
}

void validate_config(const TrainConfig& config) {
    if (config.width < 5 || config.width > 31 || config.width % 2 == 0 ||
        config.height < 5 || config.height > 31 || config.height % 2 == 0)
        throw std::invalid_argument("map dimensions must be odd values from 5 through 31");
    if (config.iterations < 0 || config.self_play_games <= 0 || config.simulations <= 0 ||
        config.train_steps < 0 || config.batch_size <= 0 || config.replay_capacity <= 0 ||
        config.channels <= 0 || config.residual_blocks <= 0 || config.max_steps <= 0 ||
        config.flame_duration <= 0 ||
        config.evaluation_interval <= 0 || config.evaluation_games <= 0 ||
        config.evaluation_simulations <= 0 || config.mcts_evaluation_interval <= 0 ||
        config.mcts_evaluation_games <= 0 || config.promotion_games <= 0 ||
        config.promotion_simulations <= 0 || config.baseline_mcts_simulations <= 0 ||
        config.baseline_mcts_depth <= 0 || config.baseline_mcts_depth > 24 ||
        config.snapshot_interval <= 0)
        throw std::invalid_argument("training counts and model dimensions must be positive");
    if (config.crate_density < 0 || config.crate_density > 100)
        throw std::invalid_argument("crate density must be from 0 through 100");
    if (config.timeout_draw_value < -1.0 || config.timeout_draw_value > 0.0 ||
        config.mutual_death_value < -1.0 || config.mutual_death_value > 0.0)
        throw std::invalid_argument("draw value targets must lie in [-1, 0]");
    if (config.arena_crush_win_value <= 0.0 || config.arena_crush_win_value > 1.0 ||
        config.selfkill_win_value <= 0.0 || config.selfkill_win_value > 1.0)
        throw std::invalid_argument("arena crush / selfkill win values must lie in (0, 1]");
    if (config.league_heuristic_fraction < 0.0 || config.league_heuristic_fraction > 1.0)
        throw std::invalid_argument("league heuristic fraction must lie in [0, 1]");
    if (config.learning_rate <= 0.0 || config.min_learning_rate <= 0.0 ||
        config.min_learning_rate > config.learning_rate)
        throw std::invalid_argument("learning rates are invalid");
    if (config.learning_rate_schedule_start_update < 0 ||
        config.learning_rate_schedule_updates < 0)
        throw std::invalid_argument("learning-rate schedule updates cannot be negative");
    if (!config.fork_from.empty() && !config.fresh)
        throw std::invalid_argument(
            "--fork-from requires --fresh (a fork establishes a new lineage/run-dir, "
            "it is not an ordinary resume)");
    if (config.promotion_margin < 0.0 || config.promotion_margin >= 0.5 ||
        config.promotion_confidence_z < 0.0 || config.random_score_floor < 0.0 ||
        config.random_score_floor > 1.0 || config.heuristic_score_floor < 0.0 ||
        config.heuristic_score_floor > 1.0 || config.heuristic_regression_margin < 0.0 ||
        config.heuristic_regression_margin > 1.0)
        throw std::invalid_argument("promotion gate values are invalid");
    const auto overlaps = [](uint64_t first_base, int first_count,
                             uint64_t second_base, int second_count) {
        const uint64_t first_end = first_base + static_cast<uint64_t>(first_count);
        const uint64_t second_end = second_base + static_cast<uint64_t>(second_count);
        return first_base < second_end && second_base < first_end;
    };
    if (overlaps(config.evaluation_seed_base, config.evaluation_games,
                 config.promotion_seed_base, config.promotion_games) ||
        overlaps(config.evaluation_seed_base, config.evaluation_games,
                 config.mcts_evaluation_seed_base, config.mcts_evaluation_games) ||
        overlaps(config.promotion_seed_base, config.promotion_games,
                 config.mcts_evaluation_seed_base, config.mcts_evaluation_games))
        throw std::invalid_argument("evaluation, promotion, and MCTS seed blocks overlap");
}

}  // namespace

struct Trainer::Impl {
    explicit Impl(TrainConfig requested)
        : config(std::move(requested)), device(torch::kCUDA, 0),
          model(BOMBER_TRAINING_CHANNELS, config.channels, config.residual_blocks,
                kActions, BOMBER_TRAINING_VIEW_SIZE),
          optimizer(model->parameters(), torch::optim::AdamWOptions(config.learning_rate)
              .weight_decay(config.weight_decay)),
          replay(static_cast<size_t>(config.replay_capacity)), rng(config.seed) {
        validate_config(config);
        std::filesystem::create_directories(config.run_dir);
        latest_path = config.run_dir / "latest.pt";
        best_path = config.run_dir / "best.pt";
        requested_checkpoint_path = config.run_dir / config.checkpoint;
        metrics_path = config.run_dir / "metrics.jsonl";
        /* Every native train/evaluate path consumes the same GPU. Keep one global native
           process at a time: an evaluation is read-only with respect to the checkpoint, but
           it still contends for CUDA memory/compute and can invalidate timing or kill a
           trainer. Training additionally owns its run directory. Locks are acquired before
           any evidence/config write so refusal leaves no partial artifact. */
        system_lock = std::make_unique<ProcessLock>(
            /* Keep the deployed filename so a repaired evaluator also excludes an older
               trainer binary that only knew the original training-only lock name. */
            std::filesystem::temp_directory_path() / "bomber-alphazero-native-trainer.lock");
        if (!config.evaluation_only) {
            run_dir_lock = std::make_unique<ProcessLock>(config.run_dir / ".trainer.lock");
        }
        model->to(device);
        bool forked = false;
        if (config.fresh) {
            for (const auto& entry : std::filesystem::directory_iterator(config.run_dir)) {
                const auto name = entry.path().filename().string();
                if (entry.is_regular_file() &&
                    (entry.path().extension() == ".pt" || name == "metrics.jsonl" ||
                     name == "config-history.jsonl" ||
                     name.ends_with(".tmp")))
                    std::filesystem::remove(entry.path());
            }
            if (!config.fork_from.empty()) {
                /* KL-101 Part C: explicit fork - load the parent's weights/optimizer/replay/
                   RNG/semantics/champion-lineage into this fresh run, then record full
                   provenance. Reuses load_checkpoint() so a fork gets the exact same ABI
                   hard-fail, semantic inheritance, and legacy-checkpoint handling as an
                   ordinary resume - a fork is not a special, less-verified code path. */
                if (!std::filesystem::exists(config.fork_from))
                    throw std::runtime_error("--fork-from checkpoint not found: " +
                                             config.fork_from.string());
                load_checkpoint(config.fork_from);
                forked = true;
                std::cout << "Forked from " << config.fork_from.string()
                          << " at iteration " << iteration
                          << " with " << replay.size() << " replay samples\n";
            }
        } else if (std::filesystem::exists(requested_checkpoint_path)) {
            load_checkpoint(requested_checkpoint_path);
            std::cout << "Loaded " << requested_checkpoint_path.string()
                      << " at iteration " << iteration
                      << " with " << replay.size() << " replay samples\n";
        }
        /* A legacy checkpoint with schedule_updates=0 does not contain enough information to
           recover the horizon that produced its optimizer history. Deriving from this
           process's short bootstrap target created a false "matched" control at 1e-5 while
           its treatment ran near 1e-4. Evaluation may accept the uncertainty because LR is
           unused there; any resumed/forked TRAIN must supply the absolute horizon explicitly. */
        if (loaded_legacy_checkpoint && !config.evaluation_only && iteration > 0 &&
            config.learning_rate_schedule_updates <= 0) {
            throw std::runtime_error(
                "legacy checkpoint has no verified learning-rate schedule horizon; training "
                "or forking it requires a positive explicit --lr-schedule-updates (derive it from the "
                "original run evidence, not the new --iterations target)");
        }
        /* Resolve all semantics BEFORE writing best.pt, fork-manifest.json, or config.json.
           Earlier code wrote a fork manifest and a synthetic champion with horizon 0, then
           silently changed the live run to a nonzero horizon. */
        if (config.learning_rate_schedule_updates <= 0 &&
            !(loaded_legacy_checkpoint && config.evaluation_only)) {
            config.learning_rate_schedule_updates = std::max<int64_t>(
                static_cast<int64_t>(config.iterations) * config.train_steps, 1);
        }
        if (forked) {
            inherit_champion_artifact(config.fork_from);
            write_fork_manifest();
        } else if (!config.evaluation_only) {
            reconcile_or_restore_champion(requested_checkpoint_path);
        }
        if (!config.evaluation_only) {
            write_config();
            append_config_history();
        }
    }

    struct Evaluation {
        int wins{};
        int draws{};
        int losses{};
        std::array<int, 2> wins_by_seat{};
        std::array<int, 2> draws_by_seat{};
        std::array<int, 2> losses_by_seat{};
        double score{};
        double lower_confidence_bound{};
        double mean_steps{};
        /* Honest "how did it win/lose" breakdown (win = opponent died, loss = learner died):
           bomb = died to the OTHER side's bomb (a real tactical kill); selfkill = died to its
           own bomb (a blunder by whoever died); crush = died to the closing sudden-death arena
           (attrition, not a tactical kill). Distinguishes real tactical wins from wins that are
           just "survive until the opponent gets crushed by the shrinking arena." */
        int win_by_bomb{};
        int win_by_selfkill{};
        int win_by_crush{};
        int loss_by_bomb{};
        int loss_by_selfkill{};
        int loss_by_crush{};
        int draw_mutual_death{};
        int draw_timeout_alive{};
        /* Fraction of the LEARNER's own action choices that were WAIT (idling). High WAIT,
           especially once sudden death starts, is a sign of passively waiting for the arena
           to do the work rather than forcing the position. */
        int64_t learner_wait_steps{};
        int64_t learner_total_steps{};
    };

    struct OptimizationMetrics {
        double loss{};
        double policy_loss{};
        double value_loss{};
        double entropy{};
        double learning_rate{};
    };

    struct SelfPlayMetrics {
        int wins{};
        int draws{};
        int losses{};
        double mean_steps{};
        /* Honest win-cause breakdown for TRAINING self-play itself (see Evaluation::
           win_by_bomb for field meanings) - decisive games only, either seat. Answers
           "is the training signal itself crush-dominated, or is that only an eval-time
           behavior against opponents that don't understand the closing arena?" */
        int win_by_bomb{};
        int win_by_selfkill{};
        int win_by_crush{};
        int draw_mutual_death{};
        int draw_timeout_alive{};
        int64_t wait_steps{};
        int64_t total_steps{};
    };

    TrainConfig config;
    torch::Device device;
    PolicyValueNet model;
    torch::optim::AdamW optimizer;
    ReplayBuffer replay;
    std::mt19937_64 rng;
    int iteration{};
    int64_t global_updates{};
    double best_score{-std::numeric_limits<double>::infinity()};
    int best_iteration{-1};
    int64_t promotion_count{};
    std::filesystem::path latest_path;
    std::filesystem::path best_path;
    std::filesystem::path requested_checkpoint_path;
    std::filesystem::path metrics_path;
    SelfPlayMetrics last_self_play;
    Evaluation last_league_play;
    /* KL-101 Part D: global native-GPU lock is held by train and evaluate for the object's
       lifetime; the run-dir lock is training-only. OS handles release on every exit path. */
    std::unique_ptr<ProcessLock> system_lock;
    std::unique_ptr<ProcessLock> run_dir_lock;
    bool loaded_legacy_checkpoint{};
    std::filesystem::path inherited_champion_source{};
    /* KL-101 Part E: phase timings for the current iteration (reset at loop top) and a
       cumulative action histogram across the whole run (the network's own moves only - both
       seats during mirror self-play, the network-controlled seat only during league play;
       never the scripted opponent's actions). */
    PhaseTimings last_phase_timings;
    std::array<int64_t, kActions> action_histogram{};

    /* KL-101 Part C: immutable fork provenance, written once at fork time (--fresh
       --fork-from PATH). Deliberately a SEPARATE file from config.json/config-history.jsonl
       (which describe THIS run's own evolving config) - a fork record answers "where did
       this lineage's starting point come from and can I trust it," which needs to survive
       and stay unambiguous even as config.json is later overwritten iteration after
       iteration. Any semantic differences between the parent and this run's explicit CLI
       overrides at fork time are already captured by load_checkpoint()'s own
       semantic-fork-log.jsonl (called just before this) - referenced here, not duplicated. */
    void write_fork_manifest() const {
        const auto destination = config.run_dir / "fork-manifest.json";
        const std::string parent_sha256 = sha256_file(config.fork_from);
        const std::string executable_path = current_executable_path().string();
        const std::string executable_sha256 = sha256_file(executable_path);
        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm utc_time{};
#ifdef _WIN32
        gmtime_s(&utc_time, &now);
#else
        gmtime_r(&now, &utc_time);
#endif
        std::ostringstream timestamp;
        timestamp << std::put_time(&utc_time, "%Y-%m-%dT%H:%M:%SZ");

        std::ofstream output(destination, std::ios::trunc);
        output << std::setprecision(9) << "{\n"
               << "  \"forked_at_utc\": \"" << timestamp.str() << "\",\n"
               << "  \"source_checkpoint\": \""
               << json_escape(config.fork_from.string()) << "\",\n"
               << "  \"source_checkpoint_sha256\": \"" << parent_sha256 << "\",\n"
               << "  \"source_champion_checkpoint\": \""
               << json_escape(inherited_champion_source.string()) << "\",\n"
               << "  \"source_champion_sha256\": \""
               << (inherited_champion_source.empty() ? std::string{} :
                   sha256_file(inherited_champion_source)) << "\",\n"
               << "  \"source_run_dir\": \""
               << json_escape(config.fork_from.parent_path().string()) << "\",\n"
               << "  \"target_run_dir\": \"" << json_escape(config.run_dir.string()) << "\",\n"
               << "  \"target_iteration_at_fork\": " << iteration << ",\n"
               << "  \"executable_path\": \"" << json_escape(executable_path) << "\",\n"
               << "  \"executable_sha256\": \"" << executable_sha256 << "\",\n"
               << "  \"git_commit\": \"" << AI_BOMBER_GIT_SHA << "\",\n"
               << "  \"dirty_diff_digest\": \""
               << json_escape(config.dirty_diff_digest) << "\",\n"
               << "  \"seed\": " << config.seed << ",\n"
               << "  \"inherited_champion_lineage\": {\"best_iteration\": " << best_iteration
               << ", \"best_score\": " << best_score
               << ", \"promotion_count\": " << promotion_count << "},\n"
               << "  \"resolved_semantics\": \"" << json_escape(semantic_manifest_string(config))
               << "\",\n"
               << "  \"semantic_forks_note\": \"any explicit CLI overrides that diverged from "
                  "the parent at fork time are in semantic-fork-log.jsonl, not duplicated here\""
               << "\n}\n";
    }

    void write_config() const {
        const auto temporary = config.run_dir / "config.json.tmp";
        const auto destination = config.run_dir / "config.json";
        std::ofstream output(temporary, std::ios::trunc);
        output << "{\n"
               << "  \"format_version\": " << kFormatVersion << ",\n"
               << "  \"git_sha\": \"" << AI_BOMBER_GIT_SHA << "\",\n"
               << "  \"torch_version\": \"" << TORCH_VERSION << "\",\n"
               << "  \"architecture\": \"native-cpp-libtorch-resnet\",\n"
               << "  \"precision\": {\"inference\": \"bf16\", \"optimization\": \"fp32\"},\n"
               << "  \"observation\": {\"channels\": " << BOMBER_TRAINING_CHANNELS
               << ", \"view_size\": " << BOMBER_TRAINING_VIEW_SIZE
               << ", \"actions\": " << kActions << "},\n"
               << "  \"map\": {\"width\": " << config.width
               << ", \"height\": " << config.height
               << ", \"max_steps\": " << config.max_steps
               << ", \"crate_density\": " << config.crate_density
               << ", \"flame_duration\": " << config.flame_duration
               << ", \"sudden_death_start\": " << config.sudden_death_start
               << ", \"shrink_interval\": " << config.shrink_interval << "},\n"
               << "  \"channels\": " << config.channels << ",\n"
               << "  \"residual_blocks\": " << config.residual_blocks << ",\n"
               << "  \"parameters\": " << model->parameter_count() << ",\n"
               << "  \"iterations\": " << config.iterations << ",\n"
               << "  \"self_play_games\": " << config.self_play_games << ",\n"
               << "  \"simulations\": " << config.simulations << ",\n"
               << "  \"train_steps\": " << config.train_steps << ",\n"
               << "  \"batch_size\": " << config.batch_size << ",\n"
               << "  \"replay_capacity\": " << config.replay_capacity << ",\n"
               << "  \"learning_rate\": " << config.learning_rate << ",\n"
               << "  \"min_learning_rate\": " << config.min_learning_rate << ",\n"
               << "  \"learning_rate_schedule_start_update\": "
               << config.learning_rate_schedule_start_update << ",\n"
               << "  \"learning_rate_schedule_updates\": "
               << config.learning_rate_schedule_updates << ",\n"
               << "  \"weight_decay\": " << config.weight_decay << ",\n"
               << "  \"c_puct\": " << config.c_puct << ",\n"
               << "  \"dirichlet_alpha\": " << config.dirichlet_alpha << ",\n"
               << "  \"dirichlet_fraction\": " << config.dirichlet_fraction << ",\n"
               << "  \"temperature\": " << config.temperature << ",\n"
               << "  \"temperature_steps\": " << config.temperature_steps << ",\n"
               << "  \"teacher_games\": " << config.teacher_games << ",\n"
               << "  \"teacher_iterations\": " << config.teacher_iterations << ",\n"
               << "  \"bootstrap_value_weight\": " << config.bootstrap_value_weight << ",\n"
               << "  \"bootstrap_value_iterations\": " << config.bootstrap_value_iterations << ",\n"
               << "  \"timeout_draw_value\": " << config.timeout_draw_value << ",\n"
               << "  \"mutual_death_value\": " << config.mutual_death_value << ",\n"
               << "  \"arena_crush_win_value\": " << config.arena_crush_win_value << ",\n"
               << "  \"selfkill_win_value\": " << config.selfkill_win_value << ",\n"
               << "  \"league_heuristic_fraction\": " << config.league_heuristic_fraction << ",\n"
               << "  \"evaluation_interval\": " << config.evaluation_interval << ",\n"
               << "  \"evaluation_games\": " << config.evaluation_games << ",\n"
               << "  \"evaluation_simulations\": " << config.evaluation_simulations << ",\n"
               << "  \"evaluation_seed_base\": " << config.evaluation_seed_base << ",\n"
               << "  \"promotion_games\": " << config.promotion_games << ",\n"
               << "  \"promotion_simulations\": " << config.promotion_simulations << ",\n"
               << "  \"promotion_seed_base\": " << config.promotion_seed_base << ",\n"
               << "  \"promotion_margin\": " << config.promotion_margin << ",\n"
               << "  \"promotion_confidence_z\": " << config.promotion_confidence_z << ",\n"
               << "  \"random_score_floor\": " << config.random_score_floor << ",\n"
               << "  \"heuristic_score_floor\": " << config.heuristic_score_floor << ",\n"
               << "  \"heuristic_regression_margin\": "
               << config.heuristic_regression_margin << ",\n"
               << "  \"mcts_evaluation_interval\": " << config.mcts_evaluation_interval << ",\n"
               << "  \"mcts_evaluation_games\": " << config.mcts_evaluation_games << ",\n"
               << "  \"baseline_mcts_simulations\": "
               << config.baseline_mcts_simulations << ",\n"
               << "  \"baseline_mcts_depth\": " << config.baseline_mcts_depth << ",\n"
               << "  \"mcts_evaluation_seed_base\": "
               << config.mcts_evaluation_seed_base << ",\n"
               << "  \"snapshot_interval\": " << config.snapshot_interval << ",\n"
               << "  \"seed\": " << config.seed << "\n"
               << "}\n";
        output.close();
        atomic_replace(temporary, destination);
    }

    void append_config_history() const {
        std::ofstream output(config.run_dir / "config-history.jsonl", std::ios::app);
        output << "{\"iteration\":" << iteration
               << ",\"global_updates\":" << global_updates
               << ",\"checkpoint\":\""
               << json_escape(config.checkpoint.string()) << "\""
               << ",\"runtime_config\":\""
               << json_escape(runtime_config_signature(config)) << "\"}\n";
        output.flush();
    }

    std::vector<Sample> collect_teacher(int count) {
        std::vector<Sample> result;
        PhaseProgress progress("teacher", count, config.progress);
        const BomberConfig base = game_config(config);
        for (int game_index = 0; game_index < count && !stop_requested.load(); ++game_index) {
            const uint64_t seed = static_cast<uint64_t>(config.seed) * 1'000'003ULL +
                                  static_cast<uint64_t>(iteration + 1) * 10'007ULL + game_index;
            BomberEnv env{};
            env_init(&env, &base);
            env_reset(&env, seed);
            const int expert_seat = static_cast<int>(seed & 1ULL);
            Agent agents[2];
            agent_init(&agents[0], expert_seat == 0 ? AGENT_HEURISTIC : AGENT_RANDOM);
            agent_init(&agents[1], expert_seat == 1 ? AGENT_HEURISTIC : AGENT_RANDOM);
            agent_reset(&agents[0], seed * 2);
            agent_reset(&agents[1], seed * 2 + 1);
            std::vector<Sample> trajectory;
            bool done = false;
            while (!done) {
                Observation observations[2];
                DebugSnapshot snapshot;
                env_observe(&env, 0, &observations[0]);
                env_observe(&env, 1, &observations[1]);
                env_get_debug_snapshot(&env, &snapshot);
                const Action actions[2] = {agent_act(&agents[0], &observations[0], &snapshot),
                                           agent_act(&agents[1], &observations[1], &snapshot)};
                Sample sample;
                encode_state(env, expert_seat, sample.state);
                sample.policy[static_cast<int>(actions[expert_seat])] = 1.0f;
                trajectory.push_back(std::move(sample));
                done = env_step_joint(&env, actions, 2).done != 0;
            }
            const float value = terminal_training_value(env, expert_seat,
                config.timeout_draw_value, config.mutual_death_value,
                config.arena_crush_win_value, config.selfkill_win_value);
            for (auto& sample : trajectory) {
                sample.value = value;
                result.push_back(std::move(sample));
            }
            progress.update(game_index + 1);
        }
        return result;
    }

    std::vector<Sample> collect_self_play(int games_count) {
        struct Game {
            BomberEnv env{};
            std::vector<Sample> trajectory;
            bool done{};
        };
        const BomberConfig base = game_config(config);
        std::vector<Game> games(games_count);
        for (int index = 0; index < games_count; ++index) {
            const uint64_t seed = static_cast<uint64_t>(config.seed) * 100'000'007ULL +
                                  static_cast<uint64_t>(iteration + 1) * 100'003ULL + index;
            env_init(&games[index].env, &base);
            env_reset(&games[index].env, seed);
        }
        std::vector<Sample> result;
        last_self_play = {};
        int64_t total_game_steps = 0;
        PhaseProgress progress("self-play", games_count, config.progress);
        BatchedMcts search(model, device, config, rng, iteration);
        int completed = 0;
        while (completed < games_count && !stop_requested.load()) {
            std::vector<BomberEnv*> active;
            std::vector<int> indices;
            std::vector<SearchConstraint> constraints;
            for (int index = 0; index < games_count; ++index) {
                if (!games[index].done) {
                    active.push_back(&games[index].env);
                    indices.push_back(index);
                    constraints.emplace_back();
                }
            }
            auto searches = search.search(active, constraints, true);
            for (size_t active_index = 0; active_index < active.size(); ++active_index) {
                Game& game = games[indices[active_index]];
                const auto& visits = searches[active_index].visits;
                const int total_visits = std::accumulate(visits.begin(), visits.end(), 0);
                Sample zero;
                Sample one;
                encode_state(game.env, 0, zero.state);
                encode_state(game.env, 1, one.state);
                for (int action = 0; action < kActions; ++action) {
                    int marginal_zero = 0;
                    int marginal_one = 0;
                    for (int opponent = 0; opponent < kActions; ++opponent) {
                        marginal_zero += visits[action * kActions + opponent];
                        marginal_one += visits[opponent * kActions + action];
                    }
                    zero.policy[action] = static_cast<float>(marginal_zero) /
                                          std::max(total_visits, 1);
                    one.policy[action] = static_cast<float>(marginal_one) /
                                         std::max(total_visits, 1);
                }
                game.trajectory.push_back(std::move(zero));
                game.trajectory.push_back(std::move(one));
                const float temperature = game.env.state.step < config.temperature_steps ?
                    static_cast<float>(config.temperature) : 0.0f;
                const int joint = sample_joint_action(visits, temperature, rng);
                const Action actions[2] = {static_cast<Action>(joint / kActions),
                                           static_cast<Action>(joint % kActions)};
                last_self_play.total_steps += 2;
                if (actions[0] == ACTION_WAIT) last_self_play.wait_steps++;
                if (actions[1] == ACTION_WAIT) last_self_play.wait_steps++;
                action_histogram[static_cast<size_t>(actions[0])]++;
                action_histogram[static_cast<size_t>(actions[1])]++;
                game.done = env_step_joint(&game.env, actions, 2).done != 0;
                if (game.done) {
                    /* Per-seat terminal values: decisive games are antisymmetric (+1/-1),
                       but a timeout/mutual-death draw is negative for BOTH seats, so the
                       two trajectories must be valued independently rather than negated. */
                    const float value_seat0 = terminal_training_value(game.env, 0,
                        config.timeout_draw_value, config.mutual_death_value,
                        config.arena_crush_win_value, config.selfkill_win_value);
                    const float value_seat1 = terminal_training_value(game.env, 1,
                        config.timeout_draw_value, config.mutual_death_value,
                        config.arena_crush_win_value, config.selfkill_win_value);
                    const int game_outcome = outcome(game.env, 0);
                    last_self_play.wins += game_outcome > 0;
                    last_self_play.losses += game_outcome < 0;
                    last_self_play.draws += game_outcome == 0;
                    /* Classify HOW the game was decided, either seat, same convention as
                       Evaluation's win_by_bomb/selfkill/crush. */
                    if (game_outcome != 0) {
                        const int winner_seat = game_outcome > 0 ? 0 : 1;
                        const int loser_seat = 1 - winner_seat;
                        const int died_owner = game.env.state.death_owner[loser_seat];
                        if (died_owner == loser_seat) last_self_play.win_by_selfkill++;
                        else if (died_owner == winner_seat) last_self_play.win_by_bomb++;
                        else last_self_play.win_by_crush++;
                    } else {
                        const bool seat0_alive = game.env.state.agents[0].alive != 0;
                        const bool seat1_alive = game.env.state.agents[1].alive != 0;
                        if (!seat0_alive && !seat1_alive) last_self_play.draw_mutual_death++;
                        else last_self_play.draw_timeout_alive++;
                    }
                    total_game_steps += game.env.state.step;
                    for (size_t sample_index = 0; sample_index < game.trajectory.size(); ++sample_index) {
                        game.trajectory[sample_index].value =
                            sample_index % 2 == 0 ? value_seat0 : value_seat1;
                        result.push_back(std::move(game.trajectory[sample_index]));
                    }
                    ++completed;
                    progress.update(completed, "samples=" + std::to_string(result.size()));
                }
            }
        }
        if (completed > 0)
            last_self_play.mean_steps = static_cast<double>(total_game_steps) / completed;
        return result;
    }

    /* League self-play: one seat is the network (searched, contributes training samples),
       the other is a scripted agent (real moves via agent_act, never MCTS-searched) — a
       deterministic-mode search opponent per game via SearchConstraint so lookahead treats
       the opponent seat's hypothetical future moves as opponent_type would actually play,
       not as another copy of the network. root_noise stays ON (true): this is real training
       data, not evaluation, so dirichlet exploration must not be dropped. Which seat is the
       learner is balanced per game (SD-off diagnostic showed a hard seat asymmetry — a
       fixed-seat league would only teach half the game). */
    std::vector<Sample> collect_league_play(int games_count, AgentType opponent_type) {
        struct Game {
            BomberEnv env{};
            Agent opponent{};
            int learner_seat{};
            std::vector<Sample> trajectory;
            bool done{};
        };
        const BomberConfig base = game_config(config);
        std::vector<Game> games(games_count);
        for (int index = 0; index < games_count; ++index) {
            const uint64_t seed = static_cast<uint64_t>(config.seed) * 100'000'007ULL +
                                  static_cast<uint64_t>(iteration + 1) * 100'003ULL +
                                  5'000'000ULL + index;
            Game& game = games[index];
            game.learner_seat = static_cast<int>(rng() % 2);
            env_init(&game.env, &base);
            env_reset(&game.env, seed);
            agent_init(&game.opponent, opponent_type);
            agent_reset(&game.opponent, seed * 2 + static_cast<uint64_t>(1 - game.learner_seat));
        }
        std::vector<Sample> result;
        last_league_play = {};
        int64_t total_game_steps = 0;
        PhaseProgress progress("league-" + std::string(agent_type_name(opponent_type)),
                               games_count, config.progress);
        BatchedMcts search(model, device, config, rng, iteration);
        int completed = 0;
        while (completed < games_count && !stop_requested.load()) {
            std::vector<BomberEnv*> active;
            std::vector<int> indices;
            std::vector<SearchConstraint> constraints;
            for (int index = 0; index < games_count; ++index) {
                if (!games[index].done) {
                    active.push_back(&games[index].env);
                    indices.push_back(index);
                    const int opponent_seat = 1 - games[index].learner_seat;
                    /* Do not recursively invoke a full MCTS baseline at every search leaf
                       (mirrors evaluate_baseline's identical guard); irrelevant while the
                       only caller passes AGENT_HEURISTIC, kept for future opponent types. */
                    const int modeled_seat = opponent_type == AGENT_MCTS ? -1 : opponent_seat;
                    constraints.push_back({modeled_seat, opponent_type,
                                           800'001ULL + static_cast<uint64_t>(index)});
                }
            }
            auto searches = search.search(active, constraints, true);
            for (size_t active_index = 0; active_index < active.size(); ++active_index) {
                Game& game = games[indices[active_index]];
                const int learner_seat = game.learner_seat;
                const int opponent_seat = 1 - learner_seat;
                const auto& visits = searches[active_index].visits;
                const int total_visits = std::accumulate(visits.begin(), visits.end(), 0);
                Sample sample;
                encode_state(game.env, learner_seat, sample.state);
                for (int action = 0; action < kActions; ++action) {
                    int marginal = 0;
                    for (int opponent_action = 0; opponent_action < kActions; ++opponent_action)
                        marginal += learner_seat == 0
                            ? visits[action * kActions + opponent_action]
                            : visits[opponent_action * kActions + action];
                    sample.policy[action] = static_cast<float>(marginal) /
                                            std::max(total_visits, 1);
                }
                game.trajectory.push_back(std::move(sample));

                const float temperature = game.env.state.step < config.temperature_steps ?
                    static_cast<float>(config.temperature) : 0.0f;
                const int joint = sample_joint_action(visits, temperature, rng);
                const int learner_action = learner_seat == 0 ? joint / kActions : joint % kActions;

                Observation observation;
                DebugSnapshot snapshot;
                env_observe(&game.env, opponent_seat, &observation);
                env_get_debug_snapshot(&game.env, &snapshot);
                const int opponent_action = static_cast<int>(
                    agent_act(&game.opponent, &observation, &snapshot));

                Action actions[2];
                actions[learner_seat] = static_cast<Action>(learner_action);
                actions[opponent_seat] = static_cast<Action>(opponent_action);

                last_league_play.learner_total_steps++;
                if (learner_action == static_cast<int>(ACTION_WAIT))
                    last_league_play.learner_wait_steps++;
                action_histogram[static_cast<size_t>(learner_action)]++;

                const StepResult step_result = env_step_joint(&game.env, actions, 2);
                game.done = step_result.done != 0;
                if (game.done) {
                    const float value = terminal_training_value(game.env, learner_seat,
                        config.timeout_draw_value, config.mutual_death_value,
                        config.arena_crush_win_value, config.selfkill_win_value);
                    const int game_outcome = outcome(game.env, learner_seat);
                    last_league_play.wins += game_outcome > 0;
                    last_league_play.losses += game_outcome < 0;
                    last_league_play.draws += game_outcome == 0;
                    const bool learner_alive = game.env.state.agents[learner_seat].alive != 0;
                    const bool opp_alive = game.env.state.agents[opponent_seat].alive != 0;
                    if (game_outcome > 0) {
                        const int died_owner = game.env.state.death_owner[opponent_seat];
                        if (died_owner == opponent_seat) last_league_play.win_by_selfkill++;
                        else if (died_owner == learner_seat) last_league_play.win_by_bomb++;
                        else last_league_play.win_by_crush++;
                    } else if (game_outcome < 0) {
                        const int died_owner = game.env.state.death_owner[learner_seat];
                        if (died_owner == learner_seat) last_league_play.loss_by_selfkill++;
                        else if (died_owner == opponent_seat) last_league_play.loss_by_bomb++;
                        else last_league_play.loss_by_crush++;
                    } else if (!learner_alive && !opp_alive) {
                        last_league_play.draw_mutual_death++;
                    } else {
                        last_league_play.draw_timeout_alive++;
                    }
                    total_game_steps += game.env.state.step;
                    for (auto& s : game.trajectory) {
                        s.value = value;
                        result.push_back(std::move(s));
                    }
                    ++completed;
                    progress.update(completed, "samples=" + std::to_string(result.size()));
                }
            }
        }
        if (completed > 0)
            last_league_play.mean_steps = static_cast<double>(total_game_steps) / completed;
        return result;
    }

    double learning_rate() const {
        const int64_t schedule_updates = config.learning_rate_schedule_updates > 0 ?
            config.learning_rate_schedule_updates :
            std::max<int64_t>(static_cast<int64_t>(config.iterations) *
                              config.train_steps, 1);
        const double progress = std::clamp(
            static_cast<double>(global_updates -
                config.learning_rate_schedule_start_update) / schedule_updates,
            0.0, 1.0);
        const double cosine = 0.5 * (1.0 + std::cos(std::numbers::pi * progress));
        return config.min_learning_rate +
               (config.learning_rate - config.min_learning_rate) * cosine;
    }

    OptimizationMetrics optimize() {
        OptimizationMetrics metrics;
        if (replay.size() == 0 || config.train_steps == 0) return metrics;
        PhaseProgress progress("optimize", config.train_steps, config.progress);
        model->train();
        int steps_completed = 0;
        /* Once optimization starts, finish the phase. A partial optimizer phase
           would require persisting an intra-iteration cursor and would replay
           the same self-play samples after resume. */
        for (int step = 0; step < config.train_steps; ++step) {
            const double rate = learning_rate();
            for (auto& group : optimizer.param_groups())
                static_cast<torch::optim::AdamWOptions&>(group.options()).lr(rate);
            auto [states_cpu, target_policy_cpu, target_value_cpu] =
                replay.batch(config.batch_size, rng);
            auto states = states_cpu.to(device);
            auto target_policy = target_policy_cpu.to(device);
            auto target_value = target_value_cpu.to(device);
            optimizer.zero_grad();
            torch::Tensor logits;
            torch::Tensor predicted_value;
            /* Keep optimization in FP32. BF16 remains enabled for the much
               larger inference workload; FP32 backward avoids scaler state
               and gives exact resume semantics across driver revisions. */
            std::tie(logits, predicted_value) = model->forward(states);
            logits = logits.to(torch::kFloat32);
            predicted_value = predicted_value.to(torch::kFloat32);
            auto policy_loss = -(target_policy * torch::log_softmax(logits, 1))
                .sum(1).mean();
            auto value_loss = torch::mse_loss(predicted_value, target_value);
            auto loss = policy_loss + value_loss;
            loss.backward();
            torch::nn::utils::clip_grad_norm_(model->parameters(), 5.0);
            optimizer.step();
            auto probabilities = torch::softmax(logits.detach(), 1);
            auto entropy = -(probabilities * torch::log(probabilities + 1e-8))
                .sum(1).mean();
            metrics.loss += loss.item<double>();
            metrics.policy_loss += policy_loss.item<double>();
            metrics.value_loss += value_loss.item<double>();
            metrics.entropy += entropy.item<double>();
            metrics.learning_rate = rate;
            ++global_updates;
            ++steps_completed;
            progress.update(step + 1);
        }
        if (steps_completed > 0) {
            metrics.loss /= steps_completed;
            metrics.policy_loss /= steps_completed;
            metrics.value_loss /= steps_completed;
            metrics.entropy /= steps_completed;
        }
        return metrics;
    }

    Evaluation evaluate_baseline(AgentType type, int games_count, int simulations,
                                 uint64_t seed_base,
                                 const std::filesystem::path& replay_out = {},
                                 const std::string& candidate_label = {},
                                 const std::filesystem::path& per_match_output = {},
                                 const std::filesystem::path& trace_output = {}) {
        struct Match {
            BomberEnv env{};
            Agent opponent{};
            int learner_seat{};
            bool done{};
            uint64_t seed{};
            int wait_steps{};
            int total_steps{};
        };
        const BomberConfig base = game_config(config);
        const int total_matches = games_count * 2;
        std::vector<Match> matches(total_matches);
        for (int index = 0; index < total_matches; ++index) {
            const int seed_index = index / 2;
            const uint64_t seed = seed_base + static_cast<uint64_t>(seed_index);
            Match& match = matches[index];
            match.learner_seat = index % 2;
            match.seed = seed;
            env_init(&match.env, &base);
            env_reset(&match.env, seed);
            agent_init(&match.opponent, type);
            agent_reset(&match.opponent, seed * 2 + (1 - match.learner_seat));
            if (type == AGENT_MCTS && !mcts_agent_configure(
                    &match.opponent, config.baseline_mcts_simulations,
                    config.baseline_mcts_depth))
                throw std::runtime_error("invalid native MCTS baseline configuration");
        }
        /* KL-108 Brick 2: immutable per-match rows - one JSON line per completed match,
           written as each match finishes (not buffered/reordered) so a killed process still
           leaves a valid partial record. Aggregate Evaluation stats alone can't answer
           per-seed/per-seat questions or be re-sliced later without rerunning the eval. */
        std::unique_ptr<std::ofstream> per_match_log;
        if (!per_match_output.empty()) {
            if (!per_match_output.parent_path().empty())
                std::filesystem::create_directories(per_match_output.parent_path());
            per_match_log = std::make_unique<std::ofstream>(per_match_output, std::ios::trunc);
            if (!*per_match_log)
                throw std::runtime_error("could not open per-match evidence output: " +
                                         per_match_output.string());
        }
        /* KL-107: per-LEARNER-STEP neural decision trace, one JSON line per step across all
           matches. Stamped with checkpoint/executable hashes (reusing KL-101 Part C's
           sha256_file/current_executable_path - the same provenance question, "what exactly
           produced this," applies here too) so a trace file is self-describing without a
           separate manifest lookup. Trace-off (trace_output empty, the default) means this
           whole block never executes and nothing about the search or chosen actions changes -
           every value read here (priors, visits, value sums) was already computed by the
           search regardless of whether anyone is watching.
           v3 adds a genuinely raw (pre-mask) policy/value head recomputation, the safe-action
           mask used to derive masked_prior from it, a wait_forced flag (idling forced by the
           mask vs chosen among alternatives), and root Q per learner action - the raw/value
           recomputation costs one extra single-position forward pass per traced step, still
           entirely gated behind trace_output being set. */
        std::unique_ptr<std::ofstream> trace_log;
        if (!trace_output.empty()) {
            if (!trace_output.parent_path().empty())
                std::filesystem::create_directories(trace_output.parent_path());
            trace_log = std::make_unique<std::ofstream>(trace_output, std::ios::trunc);
            if (!*trace_log)
                throw std::runtime_error("could not open neural trace output: " +
                                         trace_output.string());
            *trace_log << "{\"trace_format_version\":3,\"checkpoint_sha256\":\""
                       << sha256_file(requested_checkpoint_path)
                       << "\",\"executable_sha256\":\""
                       << sha256_file(current_executable_path())
                       << "\",\"git_commit\":\"" << AI_BOMBER_GIT_SHA
                       << "\",\"opponent_type\":\"" << agent_type_name(type) << "\"}\n";
        }
        /* Optional: record match 0 (both seats' joint actions + full flame/arena state
           each step) into a v4 replay bomber_viz can play back. Match 0 uses seed_base. */
        std::unique_ptr<Replay> recorder;
        if (!replay_out.empty()) {
            recorder = std::make_unique<Replay>();
            replay_init(recorder.get(), &base, seed_base);
        }
        Evaluation result;
        /* Selection and reporting must measure the network, never the tactical
           heuristic used only to bootstrap early self-play search. */
        BatchedMcts search(model, device, config, rng, iteration, false);
        PhaseProgress progress(std::string("evaluate-") + agent_type_name(type),
                               total_matches, config.progress);
        int completed = 0;
        int64_t total_steps = 0;
        while (completed < total_matches && !stop_requested.load()) {
            std::vector<BomberEnv*> active;
            std::vector<int> indices;
            std::vector<SearchConstraint> constraints;
            for (int index = 0; index < total_matches; ++index) {
                if (!matches[index].done) {
                    active.push_back(&matches[index].env);
                    indices.push_back(index);
                    /* Do not recursively invoke the native MCTS baseline at
                       every neural-search leaf. Strong-search evaluation uses
                       the real MCTS agent at the root game only. */
                    const int modeled_seat = type == AGENT_MCTS ? -1 :
                                             1 - matches[index].learner_seat;
                    constraints.push_back({modeled_seat, type,
                                           700'001ULL + static_cast<uint64_t>(index)});
                }
            }
            auto searches = search.search(active, constraints, false, simulations);
            for (size_t active_index = 0; active_index < active.size(); ++active_index) {
                Match& match = matches[indices[active_index]];
                const auto& search_result = searches[active_index];
                const int learner_action = marginal_action(
                    search_result.visits, match.learner_seat);
                if (trace_log) {
                    /* Root priors have already passed through safe-action masking and
                       renormalization. Name them accordingly: agreement with this prior can
                       implicate the policy+mask path, but cannot isolate the raw policy head.
                       Search value is likewise a backed-up root average, not raw value-head
                       output. */
                    const auto masked_prior = marginal_distribution(
                        search_result.priors, match.learner_seat);
                    const auto mcts_policy = marginal_distribution(search_result.visits, match.learner_seat);
                    const auto& value_sum = match.learner_seat == 0 ? search_result.value_sum0
                                                                     : search_result.value_sum1;
                    const int root_visits = std::accumulate(
                        search_result.visits.begin(), search_result.visits.end(), 0);
                    double value_estimate = 0.0;
                    for (size_t joint = 0; joint < kJointActions; ++joint) value_estimate += value_sum[joint];
                    value_estimate = root_visits > 0 ? value_estimate / root_visits : 0.0;

                    /* KL-107 v3: genuinely raw (pre-mask) policy/value head output at this exact
                       root position, via a fresh single-position forward pass. expand_and_backup
                       masks and renormalizes every node it expands, root or interior alike, so
                       there is no hook inside the search that ever holds the unmasked values -
                       they are gone by the time SearchResult exists. match.env here is the same
                       pre-step position the search evaluated (Node copies the env on construction
                       and never mutates the original, and no step has been applied to match.env
                       yet), so this reproduces the root evaluation rather than approximating it.
                       Recomputed, not captured - name fields accordingly so this cannot be
                       misread as "the policy the search used" the way v1's raw_policy was. */
                    std::array<float, kObservationSize> encoded{};
                    if (bomber_training_encode_env(&match.env, match.learner_seat, encoded.data(),
                                                    kObservationSize) != kObservationSize)
                        throw std::runtime_error("C observation encoder failed during KL-107 trace capture");
                    auto raw_input = torch::from_blob(encoded.data(),
                        {1, BOMBER_TRAINING_CHANNELS, BOMBER_TRAINING_VIEW_SIZE,
                         BOMBER_TRAINING_VIEW_SIZE}, torch::kFloat32).to(device);
                    torch::Tensor raw_logits, raw_value_tensor;
                    model->eval();
                    {
                        torch::InferenceMode inference;
                        AutocastGuard autocast;
                        std::tie(raw_logits, raw_value_tensor) = model->forward(raw_input);
                    }
                    const auto raw_policy_tensor =
                        torch::softmax(raw_logits.to(torch::kFloat32), 1).to(torch::kCPU);
                    const auto raw_policy_accessor = raw_policy_tensor.accessor<float, 2>();
                    std::array<double, kActions> raw_policy{};
                    for (int action = 0; action < kActions; ++action)
                        raw_policy[action] = raw_policy_accessor[0][action];
                    const double raw_value_head =
                        raw_value_tensor.to(torch::kFloat32).to(torch::kCPU).item<float>();

                    /* Same safe-action computation the search itself used to build masked_prior
                       above (shared helper, not a re-derivation) - safe_action_count==1 with
                       that one action being WAIT means idling was the position's only legal
                       move, not a policy preference, distinguishing forced from chosen idling. */
                    int safe_action_count = 0;
                    const auto safe_mask = safe_action_mask_for(
                        match.env, match.learner_seat, safe_action_count);
                    const bool wait_forced = safe_action_count == 1 &&
                        safe_mask[static_cast<int>(ACTION_WAIT)] == 1;

                    /* Root Q per learner action, marginalized from the same seat-aware
                       accumulation select_joint uses internally (value_sum / visits) - reusing
                       marginal_distribution on both arrays instead of hand-rolling the seat-
                       dependent joint-index layout (a*kActions+opp for seat 0, opp*kActions+a
                       for seat 1) avoids a transposed-but-plausible Q vector. */
                    const auto q_visits = marginal_distribution(
                        search_result.visits, match.learner_seat);
                    const auto q_value_sums = marginal_distribution(value_sum, match.learner_seat);
                    std::array<double, kActions> root_q{};
                    for (int action = 0; action < kActions; ++action)
                        root_q[action] = q_visits[action] > 0.0
                            ? q_value_sums[action] / q_visits[action] : 0.0;

                    *trace_log << "{\"seed\":" << match.seed
                        << ",\"learner_seat\":" << match.learner_seat
                        << ",\"step\":" << match.env.state.step
                        << ",\"chosen_action\":" << learner_action
                        << ",\"policy_prior_after_safety_mask\":[";
                    for (int action = 0; action < kActions; ++action)
                        *trace_log << (action ? "," : "") << masked_prior[action];
                    *trace_log << "],\"policy_prior_entropy\":"
                        << distribution_entropy(masked_prior)
                        << ",\"mcts_policy\":[";
                    for (int action = 0; action < kActions; ++action)
                        *trace_log << (action ? "," : "") << mcts_policy[action];
                    *trace_log << "],\"root_visits\":" << root_visits
                        << ",\"search_value_estimate\":" << value_estimate
                        << ",\"policy_head_raw_recomputed\":[";
                    for (int action = 0; action < kActions; ++action)
                        *trace_log << (action ? "," : "") << raw_policy[action];
                    *trace_log << "],\"policy_head_raw_recomputed_entropy\":"
                        << distribution_entropy(raw_policy)
                        << ",\"value_head_raw_recomputed\":" << raw_value_head
                        << ",\"safe_action_mask\":[";
                    for (int action = 0; action < kActions; ++action)
                        *trace_log << (action ? "," : "") << safe_mask[action];
                    *trace_log << "],\"safe_action_count\":" << safe_action_count
                        << ",\"wait_forced\":" << (wait_forced ? "true" : "false")
                        << ",\"search_root_q_values\":[";
                    for (int action = 0; action < kActions; ++action)
                        *trace_log << (action ? "," : "") << root_q[action];
                    *trace_log << "],\"running_wait_fraction\":"
                        << (match.total_steps > 0
                                ? static_cast<double>(match.wait_steps) / match.total_steps
                                : 0.0)
                        << "}\n";
                    trace_log->flush();
                }
                Observation observation;
                DebugSnapshot snapshot;
                const int opponent_seat = 1 - match.learner_seat;
                env_observe(&match.env, opponent_seat, &observation);
                env_get_debug_snapshot(&match.env, &snapshot);
                const int opponent_action = static_cast<int>(
                    agent_act(&match.opponent, &observation, &snapshot));
                Action actions[2];
                actions[match.learner_seat] = static_cast<Action>(learner_action);
                actions[opponent_seat] = static_cast<Action>(opponent_action);
                result.learner_total_steps++;
                match.total_steps++;
                if (learner_action == static_cast<int>(ACTION_WAIT)) {
                    result.learner_wait_steps++;
                    match.wait_steps++;
                }
                const StepResult step_result = env_step_joint(&match.env, actions, 2);
                match.done = step_result.done != 0;
                if (recorder && indices[active_index] == 0) {
                    replay_record_env(recorder.get(), &match.env, step_result);
                    if (match.done) {
                        replay_set_policies(recorder.get(), candidate_label.c_str(),
                                            agent_type_name(type));
                        replay_save(recorder.get(), replay_out.string().c_str());
                    }
                }
                if (match.done) {
                    const int match_outcome = outcome(match.env, match.learner_seat);
                    result.wins += match_outcome > 0;
                    result.losses += match_outcome < 0;
                    result.draws += match_outcome == 0;
                    result.wins_by_seat[match.learner_seat] += match_outcome > 0;
                    result.losses_by_seat[match.learner_seat] += match_outcome < 0;
                    result.draws_by_seat[match.learner_seat] += match_outcome == 0;
                    /* Classify HOW the match was decided, not just who won. cause_name feeds
                       both the aggregate counters below and the per-match JSONL row - one
                       classification, not duplicated logic that could silently drift apart. */
                    const bool learner_alive = match.env.state.agents[match.learner_seat].alive != 0;
                    const bool opp_alive = match.env.state.agents[opponent_seat].alive != 0;
                    const char* cause_name;
                    if (match_outcome > 0) {
                        const int died_owner = match.env.state.death_owner[opponent_seat];
                        if (died_owner == opponent_seat) { result.win_by_selfkill++; cause_name = "opponent_selfkill"; }
                        else if (died_owner == match.learner_seat) { result.win_by_bomb++; cause_name = "bomb"; }
                        else { result.win_by_crush++; cause_name = "arena_crush"; }
                    } else if (match_outcome < 0) {
                        const int died_owner = match.env.state.death_owner[match.learner_seat];
                        if (died_owner == match.learner_seat) { result.loss_by_selfkill++; cause_name = "selfkill"; }
                        else if (died_owner == opponent_seat) { result.loss_by_bomb++; cause_name = "bomb"; }
                        else { result.loss_by_crush++; cause_name = "arena_crush"; }
                    } else if (!learner_alive && !opp_alive) {
                        result.draw_mutual_death++;
                        cause_name = "mutual_death";
                    } else {
                        result.draw_timeout_alive++;
                        cause_name = "timeout_both_alive";
                    }
                    if (per_match_log) {
                        *per_match_log << "{\"seed\":" << match.seed
                            << ",\"learner_seat\":" << match.learner_seat
                            << ",\"outcome\":\""
                            << (match_outcome > 0 ? "win" : match_outcome < 0 ? "loss" : "draw")
                            << "\",\"cause\":\"" << cause_name << "\",\"steps\":"
                            << match.env.state.step << ",\"learner_wait_steps\":"
                            << match.wait_steps << ",\"learner_total_steps\":"
                            << match.total_steps << ",\"learner_wait_fraction\":"
                            << (match.total_steps > 0
                                    ? static_cast<double>(match.wait_steps) / match.total_steps
                                    : 0.0)
                            << "}\n";
                        per_match_log->flush();
                    }
                    total_steps += match.env.state.step;
                    ++completed;
                    progress.update(completed);
                }
            }
        }
        const int played = result.wins + result.draws + result.losses;
        result.score = played ? (result.wins + 0.5 * result.draws) / played : 0.0;
        result.lower_confidence_bound = wilson_lower_bound(
            result.score, played, config.promotion_confidence_z);
        result.mean_steps = played ? static_cast<double>(total_steps) / played : 0.0;
        return result;
    }

    PolicyValueNet load_model_from_checkpoint(const std::filesystem::path& source) const {
        PolicyValueNet loaded(BOMBER_TRAINING_CHANNELS, config.channels,
                              config.residual_blocks, kActions,
                              BOMBER_TRAINING_VIEW_SIZE);
        loaded->to(device);
        torch::serialize::InputArchive archive;
        archive.load_from(source.string(), device);
        loaded->load(archive);
        loaded->eval();
        return loaded;
    }

    Evaluation evaluate_incumbent(const PolicyValueNet& incumbent, int games_count,
                                  int simulations, uint64_t seed_base,
                                  const std::filesystem::path& replay_out = {},
                                  const std::string& candidate_label = {},
                                  const std::string& incumbent_label = {}) {
        struct Match {
            BomberEnv env{};
            int candidate_seat{};
            bool done{};
        };
        const BomberConfig base = game_config(config);
        const int total_matches = games_count * 2;
        std::vector<Match> matches(total_matches);
        for (int index = 0; index < total_matches; ++index) {
            Match& match = matches[index];
            match.candidate_seat = index % 2;
            env_init(&match.env, &base);
            env_reset(&match.env, seed_base + static_cast<uint64_t>(index / 2));
        }

        /* Optional: record match 0 (candidate-vs-incumbent) into a v4 replay. */
        std::unique_ptr<Replay> recorder;
        if (!replay_out.empty()) {
            recorder = std::make_unique<Replay>();
            replay_init(recorder.get(), &base, seed_base);
        }
        Evaluation result;
        BatchedMcts candidate_search(model, device, config, rng, iteration, false);
        BatchedMcts incumbent_search(incumbent, device, config, rng, iteration, false);
        PhaseProgress progress("evaluate-incumbent", total_matches, config.progress);
        int completed = 0;
        int64_t total_steps = 0;
        while (completed < total_matches && !stop_requested.load()) {
            std::vector<BomberEnv*> active;
            std::vector<int> indices;
            std::vector<SearchConstraint> constraints;
            for (int index = 0; index < total_matches; ++index) {
                if (!matches[index].done) {
                    active.push_back(&matches[index].env);
                    indices.push_back(index);
                    constraints.emplace_back();
                }
            }
            const auto candidate_results = candidate_search.search(
                active, constraints, false, simulations);
            const auto incumbent_results = incumbent_search.search(
                active, constraints, false, simulations);
            for (size_t active_index = 0; active_index < active.size(); ++active_index) {
                Match& match = matches[indices[active_index]];
                const int incumbent_seat = 1 - match.candidate_seat;
                const int candidate_action = marginal_action(
                    candidate_results[active_index].visits, match.candidate_seat);
                Action actions[2];
                actions[match.candidate_seat] = static_cast<Action>(candidate_action);
                actions[incumbent_seat] = static_cast<Action>(marginal_action(
                    incumbent_results[active_index].visits, incumbent_seat));
                result.learner_total_steps++;
                if (candidate_action == static_cast<int>(ACTION_WAIT)) result.learner_wait_steps++;
                const StepResult step_result = env_step_joint(&match.env, actions, 2);
                match.done = step_result.done != 0;
                if (recorder && indices[active_index] == 0) {
                    replay_record_env(recorder.get(), &match.env, step_result);
                    if (match.done) {
                        replay_set_policies(recorder.get(), candidate_label.c_str(),
                                            incumbent_label.c_str());
                        replay_save(recorder.get(), replay_out.string().c_str());
                    }
                }
                if (match.done) {
                    const int match_outcome = outcome(match.env, match.candidate_seat);
                    result.wins += match_outcome > 0;
                    result.losses += match_outcome < 0;
                    result.draws += match_outcome == 0;
                    result.wins_by_seat[match.candidate_seat] += match_outcome > 0;
                    result.losses_by_seat[match.candidate_seat] += match_outcome < 0;
                    result.draws_by_seat[match.candidate_seat] += match_outcome == 0;
                    const bool candidate_alive = match.env.state.agents[match.candidate_seat].alive != 0;
                    const bool incumbent_alive = match.env.state.agents[incumbent_seat].alive != 0;
                    if (match_outcome > 0) {
                        const int died_owner = match.env.state.death_owner[incumbent_seat];
                        if (died_owner == incumbent_seat) result.win_by_selfkill++;
                        else if (died_owner == match.candidate_seat) result.win_by_bomb++;
                        else result.win_by_crush++;
                    } else if (match_outcome < 0) {
                        const int died_owner = match.env.state.death_owner[match.candidate_seat];
                        if (died_owner == match.candidate_seat) result.loss_by_selfkill++;
                        else if (died_owner == incumbent_seat) result.loss_by_bomb++;
                        else result.loss_by_crush++;
                    } else if (!candidate_alive && !incumbent_alive) {
                        result.draw_mutual_death++;
                    } else {
                        result.draw_timeout_alive++;
                    }
                    total_steps += match.env.state.step;
                    progress.update(++completed);
                }
            }
        }
        const int played = result.wins + result.draws + result.losses;
        result.score = played ? (result.wins + 0.5 * result.draws) / played : 0.0;
        result.lower_confidence_bound = wilson_lower_bound(
            result.score, played, config.promotion_confidence_z);
        result.mean_steps = played ? static_cast<double>(total_steps) / played : 0.0;
        return result;
    }

    struct CheckpointLineage {
        int iteration{-1};
        int best_iteration{-1};
    };

    CheckpointLineage checkpoint_lineage(const std::filesystem::path& path) const {
        torch::serialize::InputArchive archive;
        archive.load_from(path.string(), torch::kCPU);
        torch::Tensor meta;
        archive.read("meta", meta);
        meta = meta.to(torch::kCPU);
        const auto meta_values = meta.accessor<int64_t, 1>();
        CheckpointLineage result;
        result.iteration = static_cast<int>(meta_values[1]);
        result.best_iteration = result.iteration;
        torch::Tensor selection_meta;
        if (archive.try_read("selection_meta", selection_meta)) {
            selection_meta = selection_meta.to(torch::kCPU);
            result.best_iteration = static_cast<int>(
                selection_meta.accessor<int64_t, 1>()[0]);
        }
        return result;
    }

    void copy_checkpoint_atomically(const std::filesystem::path& source,
                                    const std::filesystem::path& destination) const {
        const auto temporary = destination.string() + ".tmp";
        std::filesystem::copy_file(source, temporary,
                                   std::filesystem::copy_options::overwrite_existing);
        atomic_replace(temporary, destination);
    }

    /* A checkpoint's selection metadata names the historical champion, but its model tensors
       are the CURRENT iteration's tensors. Saving the just-loaded current model as best.pt
       therefore corrupts lineage whenever iteration != best_iteration. Preserve the exact
       parent champion artifact instead, or fail closed when it cannot be recovered. */
    void inherit_champion_artifact(const std::filesystem::path& source_checkpoint) {
        if (best_iteration < 0) return;
        auto candidate = source_checkpoint.parent_path() / "best.pt";
        if (!std::filesystem::exists(candidate)) {
            const auto source_lineage = checkpoint_lineage(source_checkpoint);
            if (source_lineage.iteration == best_iteration) {
                candidate = source_checkpoint;
            } else {
                throw std::runtime_error(
                    "fork checkpoint inherits champion iteration " +
                    std::to_string(best_iteration) + " but the exact parent best.pt is missing; "
                    "refusing to label current iteration " + std::to_string(source_lineage.iteration) +
                    " weights as that historical champion");
            }
        }
        const auto candidate_lineage = checkpoint_lineage(candidate);
        if (candidate_lineage.iteration != best_iteration ||
            candidate_lineage.best_iteration != best_iteration) {
            throw std::runtime_error(
                "parent champion artifact does not match inherited best_iteration=" +
                std::to_string(best_iteration) + " (artifact iteration=" +
                std::to_string(candidate_lineage.iteration) + ", lineage=" +
                std::to_string(candidate_lineage.best_iteration) + ")");
        }
        copy_checkpoint_atomically(candidate, best_path);
        inherited_champion_source = candidate;
        reconcile_champion_state();
        std::cout << "Inherited exact champion artifact " << candidate.string()
                  << " (iteration " << best_iteration << ") into " << best_path.string()
                  << '\n';
    }

    void reconcile_or_restore_champion(const std::filesystem::path& loaded_checkpoint) {
        if (std::filesystem::exists(best_path)) {
            const auto champion = checkpoint_lineage(best_path);
            if (best_iteration < 0 || champion.iteration != best_iteration ||
                champion.best_iteration != best_iteration) {
                throw std::runtime_error(
                    "existing best.pt champion artifact does not match loaded lineage "
                    "best_iteration=" + std::to_string(best_iteration) +
                    " (artifact iteration=" + std::to_string(champion.iteration) +
                    ", lineage=" + std::to_string(champion.best_iteration) +
                    "); refusing to use current/latest weights as a historical champion");
            }
            reconcile_champion_state();
            return;
        }
        if (best_iteration < 0 || !std::filesystem::exists(loaded_checkpoint)) return;
        const auto lineage = checkpoint_lineage(loaded_checkpoint);
        if (lineage.iteration != best_iteration) {
            throw std::runtime_error(
                "checkpoint records champion iteration " + std::to_string(best_iteration) +
                " but best.pt is missing and loaded checkpoint contains iteration " +
                std::to_string(lineage.iteration) + " weights; refusing a fake incumbent");
        }
        copy_checkpoint_atomically(loaded_checkpoint, best_path);
        inherited_champion_source = loaded_checkpoint;
        reconcile_champion_state();
    }

    void reconcile_champion_state() {
        if (!std::filesystem::exists(best_path)) return;
        torch::serialize::InputArchive archive;
        archive.load_from(best_path.string(), torch::kCPU);
        torch::Tensor stored_best;
        torch::Tensor meta;
        archive.read("best_score", stored_best);
        archive.read("meta", meta);
        const double champion_score = stored_best.to(torch::kCPU).item<double>();
        if (std::isfinite(champion_score)) best_score = std::max(best_score, champion_score);
        meta = meta.to(torch::kCPU);
        const auto meta_values = meta.accessor<int64_t, 1>();
        int champion_iteration = static_cast<int>(meta_values[1]);
        torch::Tensor selection_meta;
        if (archive.try_read("selection_meta", selection_meta)) {
            selection_meta = selection_meta.to(torch::kCPU);
            const auto values = selection_meta.accessor<int64_t, 1>();
            champion_iteration = static_cast<int>(values[0]);
            promotion_count = std::max(promotion_count, values[1]);
        }
        best_iteration = std::max(best_iteration, champion_iteration);
    }

    void save_checkpoint(const std::filesystem::path& destination) {
        const auto temporary = destination.string() + ".tmp";
        torch::serialize::OutputArchive archive;
        model->save(archive);
        torch::serialize::OutputArchive optimizer_archive;
        optimizer.save(optimizer_archive);
        archive.write("optimizer", optimizer_archive);
        torch::Tensor states, policies, values;
        {
            ScopedTimer timer(last_phase_timings.replay_serialization_seconds);
            std::tie(states, policies, values) = replay.tensors();
        }
        archive.write("replay_states", states);
        archive.write("replay_policies", policies);
        archive.write("replay_values", values);
        archive.write("meta", torch::tensor({static_cast<int64_t>(kFormatVersion),
                                               static_cast<int64_t>(iteration), global_updates,
                                               static_cast<int64_t>(replay.next())},
                                              torch::kInt64));
        archive.write("best_score", torch::tensor(best_score, torch::kFloat64));
        archive.write("selection_meta", torch::tensor(
            {static_cast<int64_t>(best_iteration), promotion_count}, torch::kInt64));
        std::ostringstream gate_config;
        gate_config << "evaluation_seed_base=" << config.evaluation_seed_base
                    << ";promotion_seed_base=" << config.promotion_seed_base
                    << ";mcts_evaluation_seed_base=" << config.mcts_evaluation_seed_base
                    << ";promotion_games=" << config.promotion_games
                    << ";promotion_simulations=" << config.promotion_simulations
                    << ";promotion_margin=" << config.promotion_margin
                    << ";promotion_confidence_z=" << config.promotion_confidence_z
                    << ";random_score_floor=" << config.random_score_floor
                    << ";heuristic_score_floor=" << config.heuristic_score_floor
                    << ";heuristic_regression_margin="
                    << config.heuristic_regression_margin
                    << ";baseline_mcts_simulations="
                    << config.baseline_mcts_simulations
                    << ";baseline_mcts_depth=" << config.baseline_mcts_depth;
        archive.write("selection_config", string_tensor(gate_config.str()));
        std::ostringstream rng_state;
        rng_state << rng;
        archive.write("rng_state", string_tensor(rng_state.str()));
        archive.write("config_signature", string_tensor(config_signature(config)));
        archive.write("runtime_config", string_tensor(runtime_config_signature(config)));
        archive.write("semantic_manifest", string_tensor(semantic_manifest_string(config)));
        {
            ScopedTimer timer(last_phase_timings.checkpoint_serialization_seconds);
            archive.save_to(temporary);
        }
        {
            ScopedTimer timer(last_phase_timings.durable_flush_seconds);
            durable_flush(temporary);
        }
        atomic_replace(temporary, destination);
    }

    void load_checkpoint(const std::filesystem::path& source) {
        torch::serialize::InputArchive archive;
        archive.load_from(source.string(), device);
        torch::Tensor stored_signature;
        archive.read("config_signature", stored_signature);
        if (tensor_string(stored_signature) != config_signature(config))
            throw std::runtime_error("checkpoint configuration does not match this run");
        torch::Tensor stored_runtime_config;
        if (archive.try_read("runtime_config", stored_runtime_config) &&
            tensor_string(stored_runtime_config) != runtime_config_signature(config))
            std::cout << "Runtime configuration differs from the checkpoint "
                         "(expected for evaluation overrides); training resumes "
                         "record transitions in config-history.jsonl\n";
        /* KL-101: load semantics (reward shaping, league, mechanics timing, resolved LR
           schedule horizon, search settings) from the checkpoint's manifest by default,
           rather than trusting whatever this process's CLI defaults happen to be. This is
           what makes a checkpoint trained at arena-crush-win-value 0.1 impossible to
           silently evaluate at the trainer's struct default of 0.3. */
        torch::Tensor stored_manifest;
        if (archive.try_read("semantic_manifest", stored_manifest)) {
            const auto forks = apply_semantic_manifest(config, tensor_string(stored_manifest));
            if (!forks.empty()) {
                std::cout << "SEMANTIC FORK - explicit CLI overrides diverge from this "
                             "checkpoint's trained semantics (all other unlisted fields "
                             "were inherited from the checkpoint):\n";
                for (const auto& fork : forks) std::cout << "  " << fork << '\n';
                std::ofstream fork_log(config.run_dir / "semantic-fork-log.jsonl",
                                       std::ios::app);
                fork_log << "{\"checkpoint\":\"" << json_escape(source.string())
                         << "\",\"forks\":[";
                for (size_t index = 0; index < forks.size(); ++index)
                    fork_log << (index ? "," : "") << '"' << json_escape(forks[index]) << '"';
                fork_log << "]}\n";
            }
        } else if (config.legacy_accept_unverified_semantics) {
            loaded_legacy_checkpoint = true;
            std::cout << "UNVERIFIED SEMANTICS - " << source.string() << " predates the "
                         "semantic manifest (KL-101). Its trained arena-crush-win-value, "
                         "selfkill-win-value, league-heuristic-fraction, and other reward/"
                         "mechanics fields cannot be verified from the checkpoint. Proceeding "
                         "ONLY because --legacy-accept-unverified-semantics was passed; this "
                         "process's CLI values/defaults are being used UNVERIFIED:\n  "
                         << semantic_manifest_string(config) << '\n';
        } else {
            throw std::runtime_error(
                source.string() + " predates the semantic manifest (KL-101) - its trained "
                "reward/mechanics/schedule semantics cannot be verified from the checkpoint "
                "file, so loading it would silently risk evaluating or resuming under the "
                "wrong values (this is exactly the bug that produced an invalid crush01 "
                "gate read). Pass --legacy-accept-unverified-semantics to proceed anyway "
                "with this process's CLI values/defaults, understood as unverified.");
        }
        model->load(archive);
        torch::serialize::InputArchive optimizer_archive;
        archive.read("optimizer", optimizer_archive);
        optimizer.load(optimizer_archive);
        torch::Tensor states, policies, values, meta, stored_best, stored_rng;
        archive.read("replay_states", states);
        archive.read("replay_policies", policies);
        archive.read("replay_values", values);
        archive.read("meta", meta);
        archive.read("best_score", stored_best);
        archive.read("rng_state", stored_rng);
        meta = meta.to(torch::kCPU);
        auto meta_values = meta.accessor<int64_t, 1>();
        if (meta_values[0] != kFormatVersion)
            throw std::runtime_error("unsupported native checkpoint format");
        iteration = static_cast<int>(meta_values[1]);
        global_updates = meta_values[2];
        replay.load(states, policies, values, static_cast<size_t>(meta_values[3]));
        best_score = stored_best.to(torch::kCPU).item<double>();
        torch::Tensor selection_meta;
        if (archive.try_read("selection_meta", selection_meta)) {
            selection_meta = selection_meta.to(torch::kCPU);
            const auto selection_values = selection_meta.accessor<int64_t, 1>();
            best_iteration = static_cast<int>(selection_values[0]);
            promotion_count = selection_values[1];
        }
        std::istringstream rng_state(tensor_string(stored_rng));
        rng_state >> rng;
        if (!rng_state) throw std::runtime_error("checkpoint RNG state is corrupt");
    }

    void append_metrics(const OptimizationMetrics& optimization,
                        const Evaluation* random, const Evaluation* heuristic,
                        const Evaluation* incumbent, const Evaluation* mcts,
                        double elapsed, size_t new_samples, bool promoted,
                        std::string_view promotion_reason) const {
        std::ofstream output(metrics_path, std::ios::app);
        output << std::setprecision(9)
               << "{\"schema_version\":2,\"iteration\":" << iteration
               << ",\"global_updates\":" << global_updates
               << ",\"replay_size\":" << replay.size()
               << ",\"new_samples\":" << new_samples
               << ",\"elapsed_seconds\":" << elapsed
               << ",\"self_play\":{\"wins\":" << last_self_play.wins
               << ",\"draws\":" << last_self_play.draws
               << ",\"losses\":" << last_self_play.losses
               << ",\"mean_steps\":" << last_self_play.mean_steps << '}'
               << ",\"optimization\":{\"loss\":" << optimization.loss
               << ",\"policy_loss\":" << optimization.policy_loss
               << ",\"value_loss\":" << optimization.value_loss
               << ",\"entropy\":" << optimization.entropy
               << ",\"learning_rate\":" << optimization.learning_rate << '}'
               /* KL-101 Part E: this iteration's phase timings (all 7 phases KL-101/KL-102
                  ask for) - the "same-machine/same-config baseline" KL-102's throughput work
                  needs as its own first step, captured here instead of duplicated there. */
               << ",\"phase_timings\":{\"mirror_collection_seconds\":"
               << last_phase_timings.mirror_collection_seconds
               << ",\"league_collection_seconds\":"
               << last_phase_timings.league_collection_seconds
               << ",\"optimization_seconds\":" << last_phase_timings.optimization_seconds
               << ",\"evaluation_random_seconds\":"
               << last_phase_timings.evaluation_random_seconds
               << ",\"evaluation_heuristic_seconds\":"
               << last_phase_timings.evaluation_heuristic_seconds
               << ",\"evaluation_incumbent_seconds\":"
               << last_phase_timings.evaluation_incumbent_seconds
               << ",\"evaluation_mcts_seconds\":" << last_phase_timings.evaluation_mcts_seconds
               << ",\"replay_serialization_seconds\":"
               << last_phase_timings.replay_serialization_seconds
               << ",\"checkpoint_serialization_seconds\":"
               << last_phase_timings.checkpoint_serialization_seconds
               << ",\"durable_flush_seconds\":" << last_phase_timings.durable_flush_seconds
               << '}'
               /* Cumulative across the whole run (not reset per iteration) - the network's
                  own moves only, both seats during mirror self-play, learner seat only
                  during league play. */
               << ",\"action_histogram_cumulative\":{\"up\":" << action_histogram[ACTION_UP]
               << ",\"down\":" << action_histogram[ACTION_DOWN]
               << ",\"left\":" << action_histogram[ACTION_LEFT]
               << ",\"right\":" << action_histogram[ACTION_RIGHT]
               << ",\"place_bomb\":" << action_histogram[ACTION_PLACE_BOMB]
               << ",\"wait\":" << action_histogram[ACTION_WAIT] << '}';
        auto write_evaluation = [&output](const char* name, const Evaluation* value) {
            if (!value) return;
            output << ",\"" << name << "\":{\"wins\":" << value->wins
                   << ",\"draws\":" << value->draws
                   << ",\"losses\":" << value->losses
                   << ",\"score\":" << value->score
                   << ",\"lower_confidence_bound\":"
                   << value->lower_confidence_bound
                   << ",\"mean_steps\":" << value->mean_steps
                   << ",\"by_seat\":[{\"seat\":0,\"wins\":"
                   << value->wins_by_seat[0] << ",\"draws\":"
                   << value->draws_by_seat[0] << ",\"losses\":"
                   << value->losses_by_seat[0]
                   << "},{\"seat\":1,\"wins\":" << value->wins_by_seat[1]
                   << ",\"draws\":" << value->draws_by_seat[1]
                   << ",\"losses\":" << value->losses_by_seat[1] << "}]}";
        };
        write_evaluation("random", random);
        write_evaluation("heuristic", heuristic);
        write_evaluation("incumbent", incumbent);
        write_evaluation("mcts", mcts);
        output << ",\"promoted\":" << (promoted ? "true" : "false")
               << ",\"promotion_reason\":\"" << json_escape(promotion_reason) << "\""
               << ",\"best_score\":";
        if (std::isfinite(best_score)) output << best_score;
        else output << "null";
        output << ",\"best_iteration\":" << best_iteration
               << ",\"promotion_count\":" << promotion_count
               << ",\"selection\":{\"evaluation_seed_base\":"
               << config.evaluation_seed_base
               << ",\"promotion_seed_base\":" << config.promotion_seed_base
               << ",\"mcts_evaluation_seed_base\":"
               << config.mcts_evaluation_seed_base
               << ",\"promotion_games\":" << config.promotion_games
               << ",\"promotion_simulations\":" << config.promotion_simulations
               << ",\"promotion_margin\":" << config.promotion_margin
               << ",\"promotion_confidence_z\":" << config.promotion_confidence_z
               << ",\"random_score_floor\":" << config.random_score_floor
               << ",\"heuristic_score_floor\":" << config.heuristic_score_floor
               << ",\"heuristic_regression_margin\":"
               << config.heuristic_regression_margin
               << ",\"baseline_mcts_simulations\":"
               << config.baseline_mcts_simulations
               << ",\"baseline_mcts_depth\":" << config.baseline_mcts_depth << '}';
        output << "}\n";
        output.flush();
    }

    void run() {
        ConsoleTee console_tee(config.run_dir / "train-console.log");
        stop_requested.store(false);
        const auto previous = std::signal(SIGINT, signal_handler);
        PhaseProgress iterations("iterations", std::max(config.iterations - iteration, 1),
                                 config.progress);
        int completed_this_run = 0;
        while (iteration < config.iterations && !stop_requested.load()) {
            const auto started = std::chrono::steady_clock::now();
            last_phase_timings = {};
            std::vector<Sample> collected;
            if (iteration < config.teacher_iterations && config.teacher_games > 0) {
                auto teacher = collect_teacher(config.teacher_games);
                collected.insert(collected.end(), std::make_move_iterator(teacher.begin()),
                                 std::make_move_iterator(teacher.end()));
            }
            const int league_games = config.league_heuristic_fraction > 0.0
                ? std::clamp(static_cast<int>(std::lround(
                      config.self_play_games * config.league_heuristic_fraction)),
                      0, config.self_play_games)
                : 0;
            const int mirror_games = config.self_play_games - league_games;
            std::vector<Sample> self_play;
            {
                ScopedTimer timer(last_phase_timings.mirror_collection_seconds);
                self_play = collect_self_play(mirror_games);
            }
            {
                const int decisive = last_self_play.win_by_bomb + last_self_play.win_by_selfkill +
                                     last_self_play.win_by_crush;
                std::cout << "  self-play[" << iteration << "]: mean_steps="
                          << last_self_play.mean_steps << ", decisive=" << decisive
                          << " (bomb=" << last_self_play.win_by_bomb
                          << " selfkill=" << last_self_play.win_by_selfkill
                          << " crush=" << last_self_play.win_by_crush << ")"
                          << ", draws=" << last_self_play.draws
                          << " (mutual=" << last_self_play.draw_mutual_death
                          << " timeout=" << last_self_play.draw_timeout_alive << ")"
                          << ", wait=" << (last_self_play.total_steps > 0
                              ? 100.0 * static_cast<double>(last_self_play.wait_steps) /
                                    static_cast<double>(last_self_play.total_steps)
                              : 0.0) << "%\n";
            }
            collected.insert(collected.end(), std::make_move_iterator(self_play.begin()),
                             std::make_move_iterator(self_play.end()));
            if (league_games > 0 && !stop_requested.load()) {
                std::vector<Sample> league_play;
                {
                    ScopedTimer timer(last_phase_timings.league_collection_seconds);
                    league_play = collect_league_play(league_games, AGENT_HEURISTIC);
                }
                std::cout << "  league-heuristic[" << iteration << "]: " << last_league_play.wins
                          << "W " << last_league_play.draws << "D " << last_league_play.losses
                          << "L mean_steps=" << last_league_play.mean_steps << '\n';
                print_behavior_report("league-heuristic", last_league_play);
                collected.insert(collected.end(), std::make_move_iterator(league_play.begin()),
                                 std::make_move_iterator(league_play.end()));
            }
            if (stop_requested.load()) break;
            const size_t new_samples = collected.size();
            replay.add(collected);
            OptimizationMetrics optimization;
            {
                ScopedTimer timer(last_phase_timings.optimization_seconds);
                optimization = optimize();
            }
            ++iteration;

            if (stop_requested.load()) {
                const double elapsed = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - started).count();
                save_checkpoint(latest_path);
                append_metrics(optimization, nullptr, nullptr, nullptr, nullptr,
                               elapsed, new_samples, false, "interrupted");
                break;
            }

            Evaluation random_result;
            Evaluation heuristic_result;
            Evaluation incumbent_result;
            Evaluation mcts_result;
            Evaluation* random_ptr = nullptr;
            Evaluation* heuristic_ptr = nullptr;
            Evaluation* incumbent_ptr = nullptr;
            Evaluation* mcts_ptr = nullptr;
            bool promoted = false;
            std::string promotion_reason = "not_evaluated";
            if (!stop_requested.load() && iteration % config.evaluation_interval == 0) {
                {
                    ScopedTimer timer(last_phase_timings.evaluation_random_seconds);
                    random_result = evaluate_baseline(AGENT_RANDOM, config.evaluation_games,
                                                      config.evaluation_simulations,
                                                      config.evaluation_seed_base);
                }
                {
                    ScopedTimer timer(last_phase_timings.evaluation_heuristic_seconds);
                    heuristic_result = evaluate_baseline(AGENT_HEURISTIC, config.evaluation_games,
                                                         config.evaluation_simulations,
                                                         config.evaluation_seed_base);
                }
                random_ptr = &random_result;
                heuristic_ptr = &heuristic_result;
                const bool random_gate = random_result.score >= config.random_score_floor;
                const bool has_incumbent = std::filesystem::exists(best_path);
                if (!random_gate) {
                    promotion_reason = "random_floor_failed";
                } else if (!has_incumbent &&
                           heuristic_result.score < config.heuristic_score_floor) {
                    promotion_reason = "heuristic_initial_floor_failed";
                } else if (!has_incumbent) {
                    promoted = true;
                    promotion_reason = "promoted_initial_quality_gate";
                } else if (heuristic_result.score <
                           best_score - config.heuristic_regression_margin) {
                    promotion_reason = "heuristic_regression_gate_failed";
                } else {
                    const auto incumbent = load_model_from_checkpoint(best_path);
                    {
                        ScopedTimer timer(last_phase_timings.evaluation_incumbent_seconds);
                        incumbent_result = evaluate_incumbent(
                            incumbent, config.promotion_games,
                            config.promotion_simulations, config.promotion_seed_base);
                    }
                    incumbent_ptr = &incumbent_result;
                    if (incumbent_result.lower_confidence_bound >
                        0.5 + config.promotion_margin) {
                        promoted = true;
                        promotion_reason = "promoted_incumbent_confidence_gate";
                    } else {
                        promotion_reason = "incumbent_confidence_gate_failed";
                    }
                }
                if (promoted) {
                    best_score = std::max(best_score, heuristic_result.score);
                    best_iteration = iteration;
                    ++promotion_count;
                }
            }
            if (!stop_requested.load() && iteration % config.mcts_evaluation_interval == 0) {
                ScopedTimer timer(last_phase_timings.evaluation_mcts_seconds);
                mcts_result = evaluate_baseline(AGENT_MCTS, config.mcts_evaluation_games,
                                                config.evaluation_simulations,
                                                config.mcts_evaluation_seed_base);
                mcts_ptr = &mcts_result;
            }
            const double elapsed = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - started).count();
            save_checkpoint(latest_path);
            if (iteration % config.snapshot_interval == 0) {
                const auto snapshot = config.run_dir /
                    ("iteration_" + [&] { std::ostringstream name; name << std::setw(6)
                        << std::setfill('0') << iteration; return name.str(); }() + ".pt");
                atomic_copy_file(latest_path, snapshot);
            }
            if (promoted)
                atomic_copy_file(latest_path, best_path);
            append_metrics(optimization, random_ptr, heuristic_ptr, incumbent_ptr,
                           mcts_ptr, elapsed, new_samples, promoted, promotion_reason);
            std::cout << "iteration=" << iteration << " samples=" << new_samples
                      << " replay=" << replay.size() << " loss=" << optimization.loss;
            if (heuristic_ptr)
                std::cout << " random=" << random_result.score
                          << " heuristic=" << heuristic_result.score
                          << " gate=" << promotion_reason;
            if (incumbent_ptr)
                std::cout << " incumbent=" << incumbent_result.score
                          << " lcb=" << incumbent_result.lower_confidence_bound;
            std::cout << " elapsed=" << format_duration(elapsed) << '\n';
            iterations.update(++completed_this_run);
        }
        if (stop_requested.load()) {
            const auto emergency = config.run_dir / "emergency.pt";
            std::cout << "Interrupt requested; writing complete emergency checkpoint...\n";
            save_checkpoint(emergency);
            save_checkpoint(latest_path);
            std::cout << "Recovery checkpoint: " << emergency.string() << '\n';
        }
        std::signal(SIGINT, previous);
    }

    /* Honest "is it playing or cheesing" report: how wins/losses were actually decided
       (bomb kill vs self-kill vs sudden-death arena crush) and how much the learner idles.
       Printed for every evaluate_only() baseline so a strong W-D-L score is never read
       without also seeing whether it came from real tactical kills or from attrition. */
    static void print_behavior_report(const char* label, const Evaluation& e) {
        const int decided_wins = e.win_by_bomb + e.win_by_selfkill + e.win_by_crush;
        const int decided_losses = e.loss_by_bomb + e.loss_by_selfkill + e.loss_by_crush;
        const double wait_pct = e.learner_total_steps > 0
            ? 100.0 * static_cast<double>(e.learner_wait_steps) / static_cast<double>(e.learner_total_steps)
            : 0.0;
        std::cout << "  " << label << " behavior: learner WAIT=" << std::fixed
                  << std::setprecision(1) << wait_pct << "% of its own actions\n";
        if (decided_wins > 0)
            std::cout << "    wins  (" << decided_wins << "): bomb-kill=" << e.win_by_bomb
                      << " (" << (100.0 * e.win_by_bomb / decided_wins) << "%)  opponent-selfkill="
                      << e.win_by_selfkill << "  arena-crush=" << e.win_by_crush << " ("
                      << (100.0 * e.win_by_crush / decided_wins) << "%)\n";
        if (decided_losses > 0)
            std::cout << "    losses(" << decided_losses << "): bomb-kill=" << e.loss_by_bomb
                      << "  self-kill=" << e.loss_by_selfkill << "  arena-crush="
                      << e.loss_by_crush << "\n";
        if (e.draw_mutual_death || e.draw_timeout_alive)
            std::cout << "    draws: mutual-death=" << e.draw_mutual_death
                      << "  timeout-both-alive=" << e.draw_timeout_alive << "\n";
        std::cout << std::defaultfloat;
    }

    void require_available_evidence_path(const std::filesystem::path& path,
                                         std::string_view label) const {
        if (path.empty() || config.overwrite_evidence || !std::filesystem::exists(path)) return;
        throw std::runtime_error(
            std::string(label) + " already exists: " + path.string() +
            "; refusing to overwrite research evidence (choose a unique path or pass "
            "--overwrite-evidence explicitly)");
    }

    void evaluate_only() {
        if (!std::filesystem::exists(requested_checkpoint_path))
            throw std::runtime_error("checkpoint not found: " + requested_checkpoint_path.string());
        require_available_evidence_path(config.evaluation_output, "aggregate evaluation output");
        require_available_evidence_path(config.per_match_output, "per-match evaluation output");
        require_available_evidence_path(config.trace_output, "neural trace output");
        /* KL-101: print the RESOLVED semantic config actually in effect for this evaluation
           (after load_checkpoint()'s inheritance in the constructor above ran) - so a strong
           W-D-L score is never read without also seeing whether the reward/mechanics
           semantics used to compute it were the checkpoint's own trained values or a
           deliberate, logged fork. */
        std::cout << "resolved semantics: " << semantic_manifest_string(config) << '\n';
        auto random = evaluate_baseline(AGENT_RANDOM, config.evaluation_games,
                                        config.evaluation_simulations,
                                        config.evaluation_seed_base);
        auto heuristic = evaluate_baseline(AGENT_HEURISTIC, config.evaluation_games,
                                           config.evaluation_simulations,
                                           config.evaluation_seed_base);
        std::cout << "random: " << random.wins << "W " << random.draws << "D "
                  << random.losses << "L score=" << random.score << '\n'
                  << "heuristic: " << heuristic.wins << "W " << heuristic.draws << "D "
                  << heuristic.losses << "L score=" << heuristic.score << '\n';
        print_behavior_report("heuristic", heuristic);
        std::optional<Evaluation> mcts_result;
        if (config.evaluate_mcts) {
            mcts_result = evaluate_baseline(AGENT_MCTS, config.mcts_evaluation_games,
                                            config.evaluation_simulations,
                                            config.mcts_evaluation_seed_base,
                                            {}, {}, config.per_match_output,
                                            config.trace_output);
            std::cout << "mcts: " << mcts_result->wins << "W " << mcts_result->draws << "D "
                      << mcts_result->losses << "L score=" << mcts_result->score << '\n';
            print_behavior_report("mcts", *mcts_result);
        }
        std::optional<Evaluation> mirror_result;
        if (!config.replay_incumbent.empty()) {
            /* checkpoint vs replay_incumbent (NN-vs-NN). games=1 (old default) just records
               a replay; incumbent_eval_games > 0 additionally runs a full statistical
               mirror-match with win-cause/WAIT instrumentation - the closest available
               proxy for "what does self-play look like at this skill level." */
            const std::string candidate = config.checkpoint.stem().string();
            const auto incumbent = load_model_from_checkpoint(config.replay_incumbent);
            const int mirror_games = std::max(1, config.incumbent_eval_games);
            mirror_result = evaluate_incumbent(incumbent, mirror_games, config.evaluation_simulations,
                                               config.evaluation_seed_base, config.replay_output,
                                               candidate, config.replay_incumbent.stem().string());
            if (mirror_games > 1) {
                std::cout << "mirror(" << config.replay_incumbent.stem().string() << "): "
                          << mirror_result->wins << "W " << mirror_result->draws << "D "
                          << mirror_result->losses << "L score=" << mirror_result->score << '\n';
                print_behavior_report(config.replay_incumbent.stem().string().c_str(), *mirror_result);
            }
            if (!config.replay_output.empty())
                std::cout << "replay: wrote " << config.replay_output.string()
                          << " (watch: bomber_viz --replay " << config.replay_output.string()
                          << ")\n";
        } else if (!config.replay_output.empty()) {
            /* Emit one representative game of the loaded checkpoint for the polished viewer.
               A single seed (both seats via games_count=1 -> 2 matches; match 0 recorded). */
            const std::string candidate = config.checkpoint.stem().string();
            const AgentType opponent = config.evaluate_mcts ? AGENT_MCTS : AGENT_HEURISTIC;
            (void)evaluate_baseline(opponent, 1, config.evaluation_simulations,
                                    config.evaluation_seed_base, config.replay_output,
                                    candidate);
            std::cout << "replay: wrote " << config.replay_output.string()
                      << " (watch: bomber_viz --replay " << config.replay_output.string()
                      << ")\n";
        }
        if (!config.evaluation_output.empty()) {
            const auto temporary = config.evaluation_output.string() + ".tmp";
            if (!config.evaluation_output.parent_path().empty())
                std::filesystem::create_directories(config.evaluation_output.parent_path());
            std::ofstream output(temporary, std::ios::trunc);
            if (!output)
                throw std::runtime_error("could not open aggregate evaluation output: " +
                                         temporary);
            auto write_result = [&output](const char* name, const Evaluation& value) {
                output << "  \"" << name << "\": {\"wins\": " << value.wins
                       << ", \"draws\": " << value.draws
                       << ", \"losses\": " << value.losses
                       << ", \"score\": " << value.score
                       << ", \"lower_confidence_bound\": "
                       << value.lower_confidence_bound
                       << ", \"mean_steps\": " << value.mean_steps
                       << ", \"by_seat\": [{\"seat\": 0, \"wins\": "
                       << value.wins_by_seat[0] << ", \"draws\": "
                       << value.draws_by_seat[0] << ", \"losses\": "
                       << value.losses_by_seat[0]
                       << "}, {\"seat\": 1, \"wins\": "
                       << value.wins_by_seat[1] << ", \"draws\": "
                       << value.draws_by_seat[1] << ", \"losses\": "
                       << value.losses_by_seat[1] << "}]"
                       << ", \"behavior\": {\"learner_wait_fraction\": "
                       << (value.learner_total_steps > 0
                               ? static_cast<double>(value.learner_wait_steps) /
                                     static_cast<double>(value.learner_total_steps)
                               : 0.0)
                       << ", \"win_by_bomb\": " << value.win_by_bomb
                       << ", \"win_by_selfkill\": " << value.win_by_selfkill
                       << ", \"win_by_arena_crush\": " << value.win_by_crush
                       << ", \"loss_by_bomb\": " << value.loss_by_bomb
                       << ", \"loss_by_selfkill\": " << value.loss_by_selfkill
                       << ", \"loss_by_arena_crush\": " << value.loss_by_crush
                       << ", \"draw_mutual_death\": " << value.draw_mutual_death
                       << ", \"draw_timeout_alive\": " << value.draw_timeout_alive
                       << "}}";
            };
            output << "{\n"
                   << "  \"schema_version\": 2,\n"
                   << "  \"generated_at_utc\": \"" << utc_timestamp() << "\",\n"
                   << "  \"invocation_argv\": [";
            for (size_t index = 0; index < config.invocation_argv.size(); ++index) {
                if (index) output << ',';
                output << '"' << json_escape(config.invocation_argv[index]) << '"';
            }
            output << "],\n"
                   << "  \"working_directory\": \""
                   << json_escape(std::filesystem::current_path().string()) << "\",\n"
                   << "  \"checkpoint\": \"" << json_escape(config.checkpoint.string()) << "\",\n"
                   << "  \"checkpoint_path\": \""
                   << json_escape(std::filesystem::absolute(requested_checkpoint_path)
                                      .lexically_normal().string()) << "\",\n"
                   << "  \"checkpoint_sha256\": \""
                   << sha256_file(requested_checkpoint_path) << "\",\n"
                   << "  \"executable_path\": \""
                   << json_escape(current_executable_path().string()) << "\",\n"
                   << "  \"executable_sha256\": \""
                   << sha256_file(current_executable_path()) << "\",\n"
                   << "  \"git_commit\": \"" << AI_BOMBER_GIT_SHA << "\",\n"
                   << "  \"runtime_config_signature\": \""
                   << json_escape(runtime_config_signature(config)) << "\",\n"
                   << "  \"checkpoint_semantics_verified\": "
                   << (loaded_legacy_checkpoint ? "false" : "true") << ",\n"
                   << "  \"checkpoint_semantics_source\": \""
                   << (loaded_legacy_checkpoint ? "legacy_cli_unverified" : "checkpoint_manifest")
                   << "\",\n"
                   << "  \"checkpoint_iteration\": " << iteration << ",\n"
                   << "  \"seed_base\": " << config.evaluation_seed_base << ",\n"
                   << "  \"seeds_per_opponent\": " << config.evaluation_games << ",\n"
                   << "  \"search_simulations\": " << config.evaluation_simulations << ",\n"
                   /* KL-101: the resolved semantics actually used for this evaluation's
                      search backup (post checkpoint-inheritance) - the regression-tested
                      field that proves a 0.1-trained checkpoint cannot silently evaluate at
                      the trainer's struct default of 0.3. */
                   << "  \"resolved_semantics\": {\"flame_duration\": " << config.flame_duration
                   << ", \"sudden_death_start\": " << config.sudden_death_start
                   << ", \"shrink_interval\": " << config.shrink_interval
                   << ", \"timeout_draw_value\": " << config.timeout_draw_value
                   << ", \"mutual_death_value\": " << config.mutual_death_value
                   << ", \"arena_crush_win_value\": " << config.arena_crush_win_value
                   << ", \"selfkill_win_value\": " << config.selfkill_win_value
                   << ", \"league_heuristic_fraction\": " << config.league_heuristic_fraction
                   << ", \"c_puct\": " << config.c_puct
                   << ", \"learning_rate_schedule_updates\": ";
            if (loaded_legacy_checkpoint && config.learning_rate_schedule_updates <= 0)
                output << "null";
            else
                output << config.learning_rate_schedule_updates;
            output << "},\n";
            if (config.evaluate_mcts)
                output << "  \"baseline_mcts_simulations\": "
                       << config.baseline_mcts_simulations << ",\n"
                       << "  \"baseline_mcts_depth\": "
                       << config.baseline_mcts_depth << ",\n"
                       << "  \"mcts_seed_base\": "
                       << config.mcts_evaluation_seed_base << ",\n"
                       << "  \"mcts_seeds_per_opponent\": "
                       << config.mcts_evaluation_games << ",\n";
            write_result("random", random);
            output << ",\n";
            write_result("heuristic", heuristic);
            if (mcts_result) {
                output << ",\n";
                write_result("mcts", *mcts_result);
            }
            output << "\n}\n";
            output.close();
            atomic_replace(temporary, config.evaluation_output);
        }
    }
};

TrainConfig parse_train_config(int argc, char** argv, int first) {
    validate_train_cli_options(argc, argv, first);
    TrainConfig config;
    config.invocation_argv = invocation_arguments(argc, argv);
    config.run_dir = parse_string(argc, argv, first, "--run-dir", config.run_dir.string());
    config.checkpoint = parse_string(argc, argv, first, "--checkpoint", config.checkpoint.string());
    config.evaluation_output = parse_string(argc, argv, first, "--output", "");
    config.per_match_output = parse_string(argc, argv, first, "--per-match-output", "");
    config.trace_output = parse_string(argc, argv, first, "--trace-output", "");
    config.replay_output = parse_string(argc, argv, first, "--replay-out", "");
    config.replay_incumbent = parse_string(argc, argv, first, "--replay-incumbent", "");
    config.incumbent_eval_games = parse_number(argc, argv, first, "--incumbent-eval-games",
                                               config.incumbent_eval_games);
    config.width = parse_number(argc, argv, first, "--width", config.width);
    config.height = parse_number(argc, argv, first, "--height", config.height);
    config.max_steps = parse_number(argc, argv, first, "--max-steps", config.max_steps);
    config.crate_density = parse_number(argc, argv, first, "--crate-density", config.crate_density);
    config.flame_duration = parse_number(argc, argv, first, "--flame-duration", config.flame_duration);
    config.sudden_death_start = parse_number(argc, argv, first, "--sudden-death-start", config.sudden_death_start);
    config.shrink_interval = parse_number(argc, argv, first, "--shrink-interval", config.shrink_interval);
    config.iterations = parse_number(argc, argv, first, "--iterations", config.iterations);
    config.self_play_games = parse_number(argc, argv, first, "--games", config.self_play_games);
    config.simulations = parse_number(argc, argv, first, "--simulations", config.simulations);
    config.train_steps = parse_number(argc, argv, first, "--train-steps", config.train_steps);
    config.batch_size = parse_number(argc, argv, first, "--batch-size", config.batch_size);
    config.replay_capacity = parse_number(argc, argv, first, "--replay-capacity", config.replay_capacity);
    config.channels = parse_number(argc, argv, first, "--channels", config.channels);
    config.residual_blocks = parse_number(argc, argv, first, "--blocks", config.residual_blocks);
    config.teacher_games = parse_number(argc, argv, first, "--teacher-games", config.teacher_games);
    config.teacher_iterations = parse_number(argc, argv, first, "--teacher-iterations", config.teacher_iterations);
    config.evaluation_interval = parse_number(argc, argv, first, "--eval-interval", config.evaluation_interval);
    config.evaluation_games = parse_number(argc, argv, first, "--eval-games", config.evaluation_games);
    config.evaluation_simulations = parse_number(argc, argv, first, "--eval-simulations", config.evaluation_simulations);
    config.promotion_games = parse_number(argc, argv, first, "--promotion-games", config.promotion_games);
    config.promotion_simulations = parse_number(argc, argv, first, "--promotion-simulations", config.promotion_simulations);
    config.mcts_evaluation_interval = parse_number(argc, argv, first, "--mcts-eval-interval", config.mcts_evaluation_interval);
    config.mcts_evaluation_games = parse_number(argc, argv, first, "--mcts-eval-games", config.mcts_evaluation_games);
    config.baseline_mcts_simulations = parse_number(argc, argv, first, "--baseline-mcts-simulations", config.baseline_mcts_simulations);
    config.baseline_mcts_depth = parse_number(argc, argv, first, "--baseline-mcts-depth", config.baseline_mcts_depth);
    config.evaluation_seed_base = parse_number(argc, argv, first, "--eval-seed-base", config.evaluation_seed_base);
    config.promotion_seed_base = parse_number(argc, argv, first, "--promotion-seed-base", config.promotion_seed_base);
    config.mcts_evaluation_seed_base = parse_number(argc, argv, first, "--mcts-eval-seed-base", config.mcts_evaluation_seed_base);
    config.snapshot_interval = parse_number(argc, argv, first, "--snapshot-interval", config.snapshot_interval);
    config.temperature_steps = parse_number(argc, argv, first, "--temperature-steps", config.temperature_steps);
    config.seed = parse_number(argc, argv, first, "--seed", config.seed);
    config.learning_rate = parse_number(argc, argv, first, "--learning-rate", config.learning_rate);
    config.min_learning_rate = parse_number(argc, argv, first, "--min-learning-rate", config.min_learning_rate);
    config.learning_rate_schedule_start_update = parse_number(
        argc, argv, first, "--lr-schedule-start-update",
        config.learning_rate_schedule_start_update);
    config.learning_rate_schedule_updates = parse_number(
        argc, argv, first, "--lr-schedule-updates",
        config.learning_rate_schedule_updates);
    config.weight_decay = parse_number(argc, argv, first, "--weight-decay", config.weight_decay);
    config.c_puct = parse_number(argc, argv, first, "--c-puct", config.c_puct);
    config.dirichlet_alpha = parse_number(argc, argv, first, "--dirichlet-alpha", config.dirichlet_alpha);
    config.dirichlet_fraction = parse_number(argc, argv, first, "--dirichlet-fraction", config.dirichlet_fraction);
    config.temperature = parse_number(argc, argv, first, "--temperature", config.temperature);
    config.bootstrap_value_weight = parse_number(argc, argv, first, "--bootstrap-weight", config.bootstrap_value_weight);
    config.bootstrap_value_iterations = parse_number(argc, argv, first, "--bootstrap-iterations", config.bootstrap_value_iterations);
    /* Convenience: --draw-value X sets BOTH per-seat draw values (timeout stall and mutual
       death) to X, the single "how much worse than a win is a draw" aggression dial from
       docs/REWARD_AND_MODEL_DESIGN.md. It is a base; a following --timeout-draw-value /
       --mutual-death-value still overrides its seat. validate_config enforces X in [-1,0]. */
    constexpr double kDrawValueUnset = 1e9;
    const double draw_value = parse_number(argc, argv, first, "--draw-value", kDrawValueUnset);
    if (draw_value != kDrawValueUnset) {
        config.timeout_draw_value = draw_value;
        config.mutual_death_value = draw_value;
    }
    config.timeout_draw_value = parse_number(argc, argv, first, "--timeout-draw-value", config.timeout_draw_value);
    config.mutual_death_value = parse_number(argc, argv, first, "--mutual-death-value", config.mutual_death_value);
    config.arena_crush_win_value = parse_number(argc, argv, first, "--arena-crush-win-value", config.arena_crush_win_value);
    config.selfkill_win_value = parse_number(argc, argv, first, "--selfkill-win-value", config.selfkill_win_value);
    config.league_heuristic_fraction = parse_number(argc, argv, first, "--league-heuristic-fraction",
                                                     config.league_heuristic_fraction);
    config.promotion_margin = parse_number(argc, argv, first, "--promotion-margin", config.promotion_margin);
    config.promotion_confidence_z = parse_number(argc, argv, first, "--promotion-confidence-z", config.promotion_confidence_z);
    config.random_score_floor = parse_number(argc, argv, first, "--random-score-floor", config.random_score_floor);
    config.heuristic_score_floor = parse_number(argc, argv, first, "--heuristic-score-floor", config.heuristic_score_floor);
    config.heuristic_regression_margin = parse_number(argc, argv, first, "--heuristic-regression-margin", config.heuristic_regression_margin);
    config.fork_from = parse_string(argc, argv, first, "--fork-from", config.fork_from.string());
    config.dirty_diff_digest = parse_string(argc, argv, first, "--dirty-diff-digest",
                                            config.dirty_diff_digest);
    config.fresh = has_flag(argc, argv, first, "--fresh");
    config.progress = !has_flag(argc, argv, first, "--no-progress");
    config.evaluate_mcts = has_flag(argc, argv, first, "--eval-mcts");
    config.overwrite_evidence = has_flag(argc, argv, first, "--overwrite-evidence");
    config.legacy_accept_unverified_semantics =
        has_flag(argc, argv, first, "--legacy-accept-unverified-semantics");
    for (const auto& [key, flag] : semantic_field_flags())
        if (has_flag(argc, argv, first, flag)) config.explicit_semantic_flags.insert(key);
    if (has_flag(argc, argv, first, "--draw-value")) {
        config.explicit_semantic_flags.insert("timeout_draw_value");
        config.explicit_semantic_flags.insert("mutual_death_value");
    }
    validate_config(config);
    return config;
}

Trainer::Trainer(TrainConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}
Trainer::~Trainer() = default;
void Trainer::run() { impl_->run(); }
void Trainer::evaluate_only() { impl_->evaluate_only(); }

void print_native_help() {
    std::cout <<
        "Native C++/LibTorch AlphaZero trainer\n\n"
        "Usage:\n"
        "  bomber_alphazero_native train [options]\n"
        "  bomber_alphazero_native evaluate [options]\n"
        "  bomber_alphazero_native benchmark [options]\n\n"
        "Key training options:\n"
        "  --run-dir PATH            Checkpoint/result directory\n"
        "  --checkpoint FILE         Checkpoint to load (default latest.pt)\n"
        "  --output PATH             Write evaluation results as JSON\n"
        "  --per-match-output PATH   (evaluate --eval-mcts) Write one JSON line per\n"
        "                            completed MCTS-baseline match: seed, seat, outcome, cause,\n"
        "                            steps, WAIT - rows for paired/seat-delta descriptive\n"
        "                            comparison; causal use also requires matched training\n"
        "                            lineage, schedule, binary, and non-treatment semantics\n"
        "  --trace-output PATH       (evaluate --eval-mcts) Write one JSON line per LEARNER\n"
        "                            STEP: safety-masked policy prior + entropy, MCTS-refined\n"
        "                            policy, search backup value, root visits, chosen action,\n"
        "                            plus a genuinely raw pre-mask policy/value recomputation,\n"
        "                            the safe-action mask, a wait_forced flag, and root Q per\n"
        "                            action (trace_format_version 3)\n"
        "  --overwrite-evidence      Explicitly allow existing output/per-match/trace paths\n"
        "                            to be replaced (default is fail closed)\n"
        "  --draw-value X            Set both per-seat draw values (timeout+mutual death) to X\n"
        "                            in [-1,0]; the aggression dial (default -0.5/-0.2)\n"
        "  --arena-crush-win-value X Value of a win by sudden-death arena crush (default 0.3)\n"
        "  --selfkill-win-value X    Value of a win where the loser blew itself up (default 0.3)\n"
        "                            Both in (0,1]; only a demonstrated kill (you killed them\n"
        "                            with your bomb) still values at 1.0 - loss values untouched\n"
        "  --league-heuristic-fraction X  Fraction of self-play games (in [0,1], default 0)\n"
        "                            played vs the heuristic agent instead of a network mirror;\n"
        "                            mirrors structurally suppress clean kills (both seats share\n"
        "                            dodge skill) - a non-mirror opponent creates reachable ones\n"
        "  --legacy-accept-unverified-semantics\n"
        "                            Required to resume/evaluate a checkpoint saved before the\n"
        "                            semantic manifest (KL-101) - its trained reward/mechanics/\n"
        "                            schedule values cannot be verified from the checkpoint, so\n"
        "                            this loudly opts in to using this process's CLI values as\n"
        "                            unverified rather than failing closed\n"
        "  --fork-from PATH          (train --fresh) Seed this run's weights/optimizer/replay/\n"
        "                            RNG/semantics/champion-lineage from an external checkpoint\n"
        "                            and record full provenance to fork-manifest.json - the\n"
        "                            explicit, verified alternative to copying a .pt file into\n"
        "                            a new run-dir by hand\n"
        "  --dirty-diff-digest STR   Opaque working-tree diff digest (e.g. `git diff | sha256`,\n"
        "                            computed by the calling script) recorded verbatim in\n"
        "                            fork-manifest.json alongside the compiled-in git commit\n"
        "  --replay-out FILE         (evaluate) Write a v4 replay of one checkpoint game for\n"
        "                            bomber_viz --replay (vs heuristic, or MCTS with --eval-mcts)\n"
        "  --replay-incumbent FILE   (evaluate) Record/evaluate checkpoint-vs-checkpoint; with\n"
        "                            --incumbent-eval-games N>1, run a full N-game mirror-match\n"
        "                            with win-cause/WAIT behavior stats instead of just 1 replay\n"
        "  --iterations N            Total iteration target (resume-safe)\n"
        "  --games N                 Concurrent self-play games\n"
        "  --simulations N           PUCT simulations per move\n"
        "  --channels N --blocks N   Residual tower size\n"
        "  --train-steps N           Optimizer updates per iteration\n"
        "  --batch-size N            Replay minibatch size\n"
        "  --lr-schedule-start-update N  Cosine schedule restart update\n"
        "  --lr-schedule-updates N   Cosine decay span (0 derives from target)\n"
        "  --promotion-games N       Seeds for two-seat champion arena\n"
        "  --promotion-simulations N Search simulations in champion arena\n"
        "  --promotion-margin X      Required confidence-bound margin over 0.5\n"
        "  --promotion-seed-base N   Disjoint champion-selection seed block\n"
        "  --baseline-mcts-simulations N  Native MCTS opponent budget\n"
        "  --baseline-mcts-depth N   Native MCTS opponent rollout depth (max 24)\n"
        "  --fresh                    Remove known artifacts and restart\n"
        "  --no-progress              Disable progress bars and ETA\n";
}

void benchmark_model(int argc, char** argv, int first) {
    const int channels = parse_number(argc, argv, first, "--channels", 128);
    const int blocks = parse_number(argc, argv, first, "--blocks", 10);
    const int batch = parse_number(argc, argv, first, "--batch-size", 512);
    const int warmup = parse_number(argc, argv, first, "--warmup", 10);
    const int repeats = parse_number(argc, argv, first, "--repeats", 50);
    torch::manual_seed(1);
    const auto device = torch::Device(torch::kCUDA, 0);
    PolicyValueNet model(BOMBER_TRAINING_CHANNELS, channels, blocks, kActions,
                         BOMBER_TRAINING_VIEW_SIZE);
    model->to(device);
    model->eval();
    auto input = torch::randn({batch, BOMBER_TRAINING_CHANNELS,
                               BOMBER_TRAINING_VIEW_SIZE, BOMBER_TRAINING_VIEW_SIZE},
                              torch::TensorOptions().device(device));
    torch::InferenceMode inference;
    for (int index = 0; index < warmup; ++index) {
        AutocastGuard autocast;
        (void)model->forward(input);
    }
    torch::cuda::synchronize();
    const auto started = std::chrono::steady_clock::now();
    for (int index = 0; index < repeats; ++index) {
        AutocastGuard autocast;
        (void)model->forward(input);
    }
    torch::cuda::synchronize();
    const double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    std::cout << "native_libtorch_cuda=true\n"
              << "precision=bf16_autocast\n"
              << "channels=" << channels << "\nblocks=" << blocks
              << "\nparameters=" << model->parameter_count()
              << "\nbatch_size=" << batch << std::fixed << std::setprecision(3)
              << "\nmilliseconds_per_batch=" << seconds * 1000.0 / repeats
              << "\npositions_per_second=" << static_cast<double>(batch) * repeats / seconds
              << '\n';
}

}  // namespace bomber::az
