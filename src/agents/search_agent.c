#include "agents/search_agent.h"
#include "sim/evaluator.h"
#include "env/bomber_rules.h"
#include "env/bomber_map.h"
#include <float.h>
#include <math.h>
#include <string.h>

typedef struct { RNG rng; int budget; int depth; } SearchImpl;

static void reset_search(Agent* agent, uint64_t seed) {
    SearchImpl* impl = (SearchImpl*)agent->impl;
    rng_init(&impl->rng, seed);
    memset(&agent->diagnostics, 0, sizeof(agent->diagnostics));
}

static void snapshot_env(const DebugSnapshot* debug, BomberEnv* env) {
    memset(env, 0, sizeof(*env));
    env->state = debug->state;
    env->config = debug->config;
    env->rng = debug->rng;
    env->danger = debug->danger;
}

static void wait_actions(Action* actions) {
    for (int a = 0; a < MAX_AGENTS; a++) actions[a] = ACTION_WAIT;
}

static int first_alive_opponent(const BomberEnv* env, int perspective) {
    for (int a = 0; a < env->state.agent_count; a++)
        if (a != perspective && env->state.agents[a].alive) return a;
    return -1;
}

static float minimax(BomberEnv* env, int perspective, int depth,
                     SearchDiagnostics* diag, float alpha, float beta) {
    diag->nodes++;
    TerminalReason terminal = rules_check_terminal(&env->state, perspective, env->config.max_steps);
    if (depth <= 0 || terminal != TERMINAL_NONE) return evaluator_score_state(env, perspective);

    Action legal[ACTION_COUNT]; int count = 0;
    env_legal_actions(env, perspective, legal, &count);
    int opponent = first_alive_opponent(env, perspective);
    Action replies[ACTION_COUNT]; int reply_count = 0;
    if (opponent >= 0) env_legal_actions(env, opponent, replies, &reply_count);
    if (reply_count == 0) { replies[0] = ACTION_WAIT; reply_count = 1; }

    float best = -FLT_MAX;
    for (int i = 0; i < count; i++) {
        float worst = FLT_MAX;
        float reply_beta = beta;
        for (int r = 0; r < reply_count; r++) {
            BomberEnv child; env_copy(&child, env);
            Action joint[MAX_AGENTS]; wait_actions(joint);
            joint[perspective] = legal[i];
            if (opponent >= 0) joint[opponent] = replies[r];
            env_step_joint(&child, joint, child.state.agent_count);
            float value = minimax(&child, perspective, depth - 1, diag, alpha, beta);
            if (value < worst) worst = value;
            if (worst <= alpha) { diag->prunes += reply_count - r - 1; break; }
            if (worst < reply_beta) reply_beta = worst;
        }
        if (worst > best) best = worst;
        if (best > alpha) alpha = best;
        if (alpha >= beta) { diag->prunes += count - i - 1; break; }
    }
    return best;
}

static Action alphabeta_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug) {
    BomberEnv env; snapshot_env(debug, &env);
    SearchImpl* impl = (SearchImpl*)agent->impl;
    SearchDiagnostics* diag = &agent->diagnostics;
    int perspective = obs->agent_id;
    memset(diag, 0, sizeof(*diag)); diag->depth = impl->depth;
    Action legal[ACTION_COUNT]; int count = 0;
    env_legal_actions(&env, perspective, legal, &count);
    float best = -FLT_MAX; Action chosen = ACTION_WAIT;
    for (int i = 0; i < count; i++) {
        BomberEnv child; env_copy(&child, &env);
        Action joint[MAX_AGENTS]; wait_actions(joint); joint[perspective] = legal[i];
        env_step_joint(&child, joint, child.state.agent_count);
        float value = minimax(&child, perspective, impl->depth - 1, diag, -FLT_MAX, FLT_MAX);
        diag->action_values[legal[i]] = value;
        if (value > best) { best = value; chosen = legal[i]; }
    }
    diag->selected_action = chosen; diag->value = best;
    return chosen;
}

static int tactically_useful_bomb(const BomberEnv* env, int actor, int target) {
    const BomberAgentState* me = &env->state.agents[actor];
    const BomberAgentState* foe = &env->state.agents[target];
    int dx = me->x > foe->x ? me->x - foe->x : foe->x - me->x;
    int dy = me->y > foe->y ? me->y - foe->y : foe->y - me->y;
    if ((dx == 0 || dy == 0) && dx + dy <= me->blast_range) return 1;
    static const int ax[] = {0, 0, -1, 1}; static const int ay[] = {-1, 1, 0, 0};
    for (int i = 0; i < 4; i++) {
        int x = me->x + ax[i], y = me->y + ay[i];
        if (map_in_bounds(&env->state, x, y) && env->state.tiles[y][x] == TILE_CRATE) return 1;
    }
    return 0;
}

static Action tactical_action(const BomberEnv* env, int actor, int target, RNG* rng) {
    Observation obs; env_observe(env, actor, &obs);
    const BomberAgentState* me = &env->state.agents[actor];
    const BomberAgentState* foe = &env->state.agents[target];
    int distance = (me->x > foe->x ? me->x - foe->x : foe->x - me->x) +
                   (me->y > foe->y ? me->y - foe->y : foe->y - me->y);
    int safe_moves[ACTION_COUNT], safe_count = 0;
    for (int a = ACTION_UP; a <= ACTION_RIGHT; a++)
        if (obs.valid_actions[a] && obs.safe_actions[a]) safe_moves[safe_count++] = a;
    if (obs.imminent_danger || obs.in_danger)
        return safe_count ? (Action)safe_moves[rng_range(rng, 0, safe_count)] : ACTION_WAIT;
    if (obs.valid_actions[ACTION_PLACE_BOMB] && obs.safe_actions[ACTION_PLACE_BOMB] &&
        tactically_useful_bomb(env, actor, target)) return ACTION_PLACE_BOMB;
    Action best = ACTION_WAIT; int best_distance = distance;
    static const int dx[] = {0, 0, -1, 1}; static const int dy[] = {-1, 1, 0, 0};
    for (int i = 0; i < safe_count; i++) {
        Action candidate = (Action)safe_moves[i];
        int nx = me->x + dx[candidate], ny = me->y + dy[candidate];
        int d = (nx > foe->x ? nx - foe->x : foe->x - nx) + (ny > foe->y ? ny - foe->y : foe->y - ny);
        if (d < best_distance) { best_distance = d; best = candidate; }
    }
    if (best == ACTION_WAIT && safe_count) best = (Action)safe_moves[rng_range(rng, 0, safe_count)];
    return best;
}

static float rollout(BomberEnv* env, SearchImpl* impl, int perspective, int depth) {
    for (int step = 0; step < depth; step++) {
        Action joint[MAX_AGENTS]; wait_actions(joint);
        int target = first_alive_opponent(env, perspective);
        if (target < 0) break;
        joint[perspective] = tactical_action(env, perspective, target, &impl->rng);
        for (int a = 0; a < env->state.agent_count; a++)
            if (a != perspective && env->state.agents[a].alive)
                joint[a] = tactical_action(env, a, perspective, &impl->rng);
        env_step_joint(env, joint, env->state.agent_count);
        if (rules_check_terminal(&env->state, perspective, env->config.max_steps) != TERMINAL_NONE) break;
    }
    return evaluator_score_state(env, perspective);
}

static Action mcts_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug) {
    BomberEnv root; snapshot_env(debug, &root);
    SearchImpl* impl = (SearchImpl*)agent->impl;
    SearchDiagnostics* diag = &agent->diagnostics;
    int perspective = obs->agent_id;
    memset(diag, 0, sizeof(*diag)); diag->simulations = impl->budget;
    Action legal[ACTION_COUNT]; int count = 0;
    env_legal_actions(&root, perspective, legal, &count);
    for (int simulation = 0; simulation < impl->budget && count; simulation++) {
        int pick = -1;
        for (int i = 0; i < count; i++) if (diag->visits[legal[i]] == 0) { pick = i; break; }
        if (pick < 0) {
            float best_ucb = -FLT_MAX;
            for (int i = 0; i < count; i++) {
                Action candidate = legal[i];
                float mean = diag->action_values[candidate] / (float)diag->visits[candidate];
                float explore = 1.4f * sqrtf(logf((float)simulation + 1.0f) /
                                                (float)diag->visits[candidate]);
                if (mean + explore > best_ucb) { best_ucb = mean + explore; pick = i; }
            }
        }
        BomberEnv child; env_copy(&child, &root);
        Action joint[MAX_AGENTS]; wait_actions(joint); joint[perspective] = legal[pick];
        int opponent = first_alive_opponent(&child, perspective);
        if (opponent >= 0) joint[opponent] = tactical_action(&child, opponent, perspective, &impl->rng);
        env_step_joint(&child, joint, child.state.agent_count);
        float value = rollout(&child, impl, perspective, impl->depth);
        Action action = legal[pick];
        diag->visits[action]++;
        diag->action_values[action] += value;
        diag->nodes += impl->depth + 1;
    }
    Action chosen = ACTION_WAIT; float best = -FLT_MAX;
    for (int i = 0; i < count; i++) {
        Action action = legal[i];
        if (diag->visits[action]) diag->action_values[action] /= diag->visits[action];
        float score = diag->action_values[action];
        if (score > best) { best = score; chosen = action; }
    }
    diag->selected_action = chosen; diag->value = best;
    return chosen;
}

static void init_common(Agent* agent, const char* name, AgentActFn act, int budget, int depth) {
    SearchImpl* impl = (SearchImpl*)agent_impl_storage(agent, sizeof(SearchImpl));
    agent->impl = impl; impl->budget = budget; impl->depth = depth;
    agent->act = act; agent->reset = reset_search;
    strncpy(agent->name, name, sizeof(agent->name) - 1); reset_search(agent, 1);
}

void alphabeta_agent_init(Agent* agent) { init_common(agent, "alpha-beta", alphabeta_act, 0, 2); }
void mcts_agent_init(Agent* agent) { init_common(agent, "mcts", mcts_act, 32, 4); }
