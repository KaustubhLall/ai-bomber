#include "agents/search_agent.h"
#include "sim/evaluator.h"
#include "env/bomber_rules.h"
#include "env/bomber_map.h"
#include <float.h>
#include <math.h>
#include <string.h>

typedef struct {
    RNG rng;
    int budget;
    int depth;
    int recent_x[12];
    int recent_y[12];
    int recent_count;
    int recent_cursor;
    Action last_action;
    Action escape_plan[12];
    int escape_length;
    int escape_cursor;
    int last_observed_x;
    int last_observed_y;
    int blocked_streak;
} SearchImpl;

typedef struct {
    int parent;
    int children[ACTION_COUNT];
    int visits;
    float value_sum;
    Action action;
} UctNode;

#define UCT_MAX_NODES 512
#define UCT_MAX_DEPTH 24

static void reset_search(Agent* agent, uint64_t seed) {
    SearchImpl* impl = (SearchImpl*)agent->impl;
    rng_init(&impl->rng, seed);
    impl->recent_count = 0; impl->recent_cursor = 0; impl->last_action = ACTION_WAIT;
    impl->escape_length = 0; impl->escape_cursor = 0;
    impl->last_observed_x = -1; impl->last_observed_y = -1; impl->blocked_streak = 0;
    for (int i = 0; i < 12; i++) { impl->recent_x[i] = -1; impl->recent_y[i] = -1; }
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
        if (legal[i] == ACTION_PLACE_BOMB) continue; /* defensive search baseline */
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

static int line_of_fire(const BomberState* state, int x, int y, int tx, int ty, int range) {
    int dx = tx == x ? 0 : (tx > x ? 1 : -1);
    int dy = ty == y ? 0 : (ty > y ? 1 : -1);
    if (dx && ty != y) return 0;
    if (dy && tx != x) return 0;
    int distance = dx ? (tx > x ? tx - x : x - tx) : (ty > y ? ty - y : y - ty);
    if (distance <= 0 || distance > range) return 0;
    for (int step = 1; step <= distance; step++) {
        int nx = x + dx * step, ny = y + dy * step;
        TileType tile = state->tiles[ny][nx];
        if (tile == TILE_SOLID_WALL || (tile == TILE_CRATE && step < distance)) return 0;
    }
    return 1;
}

static int tile_exits(const BomberState* state, int x, int y) {
    static const int dx[] = {0, 0, -1, 1}; static const int dy[] = {-1, 1, 0, 0};
    int exits = 0;
    for (int d = 0; d < 4; d++) if (map_is_walkable(state, x + dx[d], y + dy[d])) exits++;
    return exits;
}

static int path_distance(const BomberState* state, int sx, int sy, int tx, int ty) {
    typedef struct { short x, y, distance; } PathNode;
    PathNode queue[MAX_WIDTH * MAX_HEIGHT];
    unsigned char visited[MAX_HEIGHT][MAX_WIDTH] = {{0}};
    static const int dx[] = {0, 0, -1, 1}; static const int dy[] = {-1, 1, 0, 0};
    int head = 0, tail = 0;
    queue[tail++] = (PathNode){(short)sx, (short)sy, 0}; visited[sy][sx] = 1;
    while (head < tail) {
        PathNode node = queue[head++];
        if (node.x == tx && node.y == ty) return node.distance;
        for (int d = 0; d < 4; d++) {
            int nx = node.x + dx[d], ny = node.y + dy[d];
            if (!map_in_bounds(state, nx, ny) || visited[ny][nx]) continue;
            if ((nx != tx || ny != ty) && !map_is_walkable(state, nx, ny)) continue;
            if (state->tiles[ny][nx] == TILE_SOLID_WALL || state->tiles[ny][nx] == TILE_CRATE) continue;
            visited[ny][nx] = 1;
            queue[tail++] = (PathNode){(short)nx, (short)ny, (short)(node.distance + 1)};
        }
    }
    return 999;
}

static int nearest_crate_distance(const BomberState* state, int sx, int sy) {
    typedef struct { short x, y, distance; } PathNode;
    PathNode queue[MAX_WIDTH * MAX_HEIGHT];
    unsigned char visited[MAX_HEIGHT][MAX_WIDTH] = {{0}};
    static const int dx[] = {0, 0, -1, 1}; static const int dy[] = {-1, 1, 0, 0};
    int head = 0, tail = 0;
    queue[tail++] = (PathNode){(short)sx, (short)sy, 0}; visited[sy][sx] = 1;
    while (head < tail) {
        PathNode node = queue[head++];
        for (int d = 0; d < 4; d++) {
            int nx = node.x + dx[d], ny = node.y + dy[d];
            if (!map_in_bounds(state, nx, ny)) continue;
            if (state->tiles[ny][nx] == TILE_CRATE) return node.distance;
            if (visited[ny][nx] || !map_is_walkable(state, nx, ny)) continue;
            visited[ny][nx] = 1;
            queue[tail++] = (PathNode){(short)nx, (short)ny, (short)(node.distance + 1)};
        }
    }
    return 999;
}

static int nearest_powerup_distance(const BomberState* state, int sx, int sy) {
    typedef struct { short x, y, distance; } PathNode;
    PathNode queue[MAX_WIDTH * MAX_HEIGHT];
    unsigned char visited[MAX_HEIGHT][MAX_WIDTH] = {{0}};
    static const int dx[] = {0, 0, -1, 1}; static const int dy[] = {-1, 1, 0, 0};
    int head = 0, tail = 0;
    queue[tail++] = (PathNode){(short)sx, (short)sy, 0}; visited[sy][sx] = 1;
    while (head < tail) {
        PathNode node = queue[head++];
        TileType tile = state->tiles[node.y][node.x];
        if (tile == TILE_POWERUP_BOMB || tile == TILE_POWERUP_RANGE || tile == TILE_POWERUP_SPEED)
            return node.distance;
        for (int d = 0; d < 4; d++) {
            int nx = node.x + dx[d], ny = node.y + dy[d];
            if (!map_in_bounds(state, nx, ny) || visited[ny][nx] || !map_is_walkable(state, nx, ny)) continue;
            visited[ny][nx] = 1;
            queue[tail++] = (PathNode){(short)nx, (short)ny, (short)(node.distance + 1)};
        }
    }
    return 999;
}

static int tactically_useful_bomb(const BomberEnv* env, int actor, int target) {
    const BomberAgentState* me = &env->state.agents[actor];
    const BomberAgentState* foe = &env->state.agents[target];
    int dx = me->x > foe->x ? me->x - foe->x : foe->x - me->x;
    int dy = me->y > foe->y ? me->y - foe->y : foe->y - me->y;
    if (line_of_fire(&env->state, me->x, me->y, foe->x, foe->y, me->blast_range)) return 1;
    if (dx + dy <= me->blast_range + 1 && tile_exits(&env->state, foe->x, foe->y) <= 2) return 1;
    static const int ax[] = {0, 0, -1, 1}; static const int ay[] = {-1, 1, 0, 0};
    for (int i = 0; i < 4; i++) {
        int x = me->x + ax[i], y = me->y + ay[i];
        if (map_in_bounds(&env->state, x, y) && env->state.tiles[y][x] == TILE_CRATE) return 1;
    }
    return 0;
}

static int owner_has_active_bomb(const BomberState* state, int owner) {
    for (int b = 0; b < MAX_BOMBS; b++)
        if (state->bombs[b].active && state->bombs[b].owner_id == owner) return 1;
    return 0;
}

static int survival_search(const BomberEnv* env, int actor, int depth,
                           Action* plan, int* plan_length) {
    if (!env->state.agents[actor].alive) return 0;
    if (!owner_has_active_bomb(&env->state, actor)) { *plan_length = 0; return 1; }
    if (depth <= 0) return 0;
    Observation observation; env_observe(env, actor, &observation);
    for (int action = ACTION_UP; action <= ACTION_WAIT; action++) {
        if (action == ACTION_PLACE_BOMB || !observation.valid_actions[action] ||
            !observation.safe_actions[action]) continue;
        BomberEnv child; env_copy(&child, env);
        Action joint[MAX_AGENTS]; wait_actions(joint); joint[actor] = (Action)action;
        env_step_joint(&child, joint, child.state.agent_count);
        Action suffix[12]; int suffix_length = 0;
        if (survival_search(&child, actor, depth - 1, suffix, &suffix_length)) {
            plan[0] = (Action)action;
            for (int i = 0; i < suffix_length && i + 1 < 12; i++) plan[i + 1] = suffix[i];
            *plan_length = suffix_length + 1;
            return 1;
        }
    }
    return 0;
}

static int bomb_survival_plan(const BomberEnv* env, int actor, Action* plan, int* plan_length) {
    int before = env->state.agents[actor].bombs_active;
    if (before >= 2 || env->state.agents[actor].bomb_ammo <= 0) return 0;
    BomberEnv child; env_copy(&child, env);
    Action joint[MAX_AGENTS]; wait_actions(joint); joint[actor] = ACTION_PLACE_BOMB;
    env_step_joint(&child, joint, child.state.agent_count);
    if (child.state.agents[actor].bombs_active <= before || !child.state.agents[actor].alive) return 0;
    return survival_search(&child, actor, child.config.bomb_timer + 2, plan, plan_length);
}

static int bomb_has_survival_plan(const BomberEnv* env, int actor) {
    Action plan[12]; int plan_length = 0;
    return bomb_survival_plan(env, actor, plan, &plan_length);
}

static void survival_actions(const BomberEnv* env, int actor, Action* actions, int* count,
                             int allow_bomb) {
    Observation observation; env_observe(env, actor, &observation);
    int n = 0;
    for (int action = 0; action < ACTION_COUNT; action++) {
        if (!allow_bomb && action == ACTION_PLACE_BOMB) continue;
        if (observation.valid_actions[action] && observation.safe_actions[action])
            actions[n++] = (Action)action;
    }
    if (n == 0) {
        env_legal_actions(env, actor, actions, &n);
        if (!allow_bomb) {
            int write = 0;
            for (int i = 0; i < n; i++) if (actions[i] != ACTION_PLACE_BOMB) actions[write++] = actions[i];
            n = write;
        }
    }
    *count = n;
}

static int robust_survival(const BomberEnv* env, int actor, int depth, Action* first_action) {
    if (!env->state.agents[actor].alive) return 0;
    if (!owner_has_active_bomb(&env->state, actor)) return 1;
    if (depth <= 0) return 0;
    Action ours[ACTION_COUNT]; int our_count = 0;
    survival_actions(env, actor, ours, &our_count, 0);
    int opponent = first_alive_opponent(env, actor);
    Action replies[ACTION_COUNT]; int reply_count = 0;
    if (opponent >= 0) survival_actions(env, opponent, replies, &reply_count, 1);
    if (reply_count == 0) { replies[0] = ACTION_WAIT; reply_count = 1; }
    for (int i = 0; i < our_count; i++) {
        int survives_every_reply = 1;
        for (int r = 0; r < reply_count; r++) {
            BomberEnv child; env_copy(&child, env);
            Action joint[MAX_AGENTS]; wait_actions(joint);
            joint[actor] = ours[i]; if (opponent >= 0) joint[opponent] = replies[r];
            env_step_joint(&child, joint, child.state.agent_count);
            if (!robust_survival(&child, actor, depth - 1, NULL)) {
                survives_every_reply = 0; break;
            }
        }
        if (survives_every_reply) {
            if (first_action) *first_action = ours[i];
            return 1;
        }
    }
    return 0;
}

static int robust_bomb_survival(const BomberEnv* env, int actor) {
    if (env->state.agents[actor].bombs_active >= 2 ||
        env->state.agents[actor].bomb_ammo <= 0) return 0;
    int opponent = first_alive_opponent(env, actor);
    Action replies[ACTION_COUNT]; int reply_count = 0;
    if (opponent >= 0) survival_actions(env, opponent, replies, &reply_count, 1);
    if (reply_count == 0) { replies[0] = ACTION_WAIT; reply_count = 1; }
    for (int r = 0; r < reply_count; r++) {
        BomberEnv child; env_copy(&child, env);
        Action joint[MAX_AGENTS]; wait_actions(joint);
        joint[actor] = ACTION_PLACE_BOMB; if (opponent >= 0) joint[opponent] = replies[r];
        env_step_joint(&child, joint, child.state.agent_count);
        if (!child.state.agents[actor].alive ||
            !robust_survival(&child, actor, child.config.bomb_timer + 1, NULL)) return 0;
    }
    return 1;
}

int search_bomb_is_robustly_safe(const DebugSnapshot* debug, int actor) {
    BomberEnv env; snapshot_env(debug, &env);
    return robust_bomb_survival(&env, actor);
}

Action search_robust_escape_action(const DebugSnapshot* debug, int actor, int* found) {
    BomberEnv env; snapshot_env(debug, &env);
    Action action = ACTION_WAIT;
    int ok = robust_survival(&env, actor, env.config.bomb_timer + 1, &action);
    if (found) *found = ok;
    return action;
}

static Action tactical_action(const BomberEnv* env, int actor, int target, RNG* rng) {
    if (target < 0 || target >= env->state.agent_count) return ACTION_WAIT;
    Observation obs; env_observe(env, actor, &obs);
    const BomberAgentState* me = &env->state.agents[actor];
    const BomberAgentState* foe = &env->state.agents[target];
    int safe_moves[ACTION_COUNT], safe_count = 0;
    for (int a = ACTION_UP; a <= ACTION_RIGHT; a++)
        if (obs.valid_actions[a] && obs.safe_actions[a]) safe_moves[safe_count++] = a;
    static const int dx[] = {0, 0, -1, 1}; static const int dy[] = {-1, 1, 0, 0};
    if (obs.danger_timer >= 0 || obs.in_danger) {
        Action escape = ACTION_WAIT; int best_escape = -999999;
        for (int i = 0; i < safe_count; i++) {
            Action candidate = (Action)safe_moves[i];
            int nx = me->x + dx[candidate], ny = me->y + dy[candidate];
            int timer = obs.local_danger[LOCAL_OBS_HALF + dy[candidate]][LOCAL_OBS_HALF + dx[candidate]];
            int score = (timer < 0 ? 10000 : timer * 100) + tile_exits(&env->state, nx, ny) * 10;
            if (score > best_escape) { best_escape = score; escape = candidate; }
        }
        return escape;
    }
    if (obs.valid_actions[ACTION_PLACE_BOMB] && obs.safe_actions[ACTION_PLACE_BOMB] &&
        tactically_useful_bomb(env, actor, target) && bomb_has_survival_plan(env, actor))
        return ACTION_PLACE_BOMB;
    Action powerup_move = ACTION_WAIT; int best_powerup = 999, best_powerup_exits = -1;
    for (int i = 0; i < safe_count; i++) {
        Action candidate = (Action)safe_moves[i];
        int nx = me->x + dx[candidate], ny = me->y + dy[candidate];
        int distance = nearest_powerup_distance(&env->state, nx, ny);
        int exits = tile_exits(&env->state, nx, ny);
        if (distance < best_powerup || (distance == best_powerup && exits > best_powerup_exits)) {
            best_powerup = distance; best_powerup_exits = exits; powerup_move = candidate;
        }
    }
    if (best_powerup < 999) return powerup_move;
    Action best = ACTION_WAIT; int best_score = -999999;
    for (int i = 0; i < safe_count; i++) {
        Action candidate = (Action)safe_moves[i];
        int nx = me->x + dx[candidate], ny = me->y + dy[candidate];
        int route = path_distance(&env->state, nx, ny, foe->x, foe->y);
        int score = -route * 10 + tile_exits(&env->state, nx, ny) * 4;
        if (route >= 999) score = -nearest_crate_distance(&env->state, nx, ny) * 15 +
                                  tile_exits(&env->state, nx, ny) * 4;
        if (line_of_fire(&env->state, nx, ny, foe->x, foe->y, me->blast_range)) score += 45;
        if (score > best_score) { best_score = score; best = candidate; }
    }
    if (best == ACTION_WAIT && safe_count) best = (Action)safe_moves[rng_range(rng, 0, safe_count)];
    return best;
}

static void safe_legal_actions(const BomberEnv* env, int actor, Action* actions, int* count) {
    Observation observation; env_observe(env, actor, &observation);
    int n = 0;
    for (int action = 0; action < ACTION_COUNT; action++)
        if (observation.valid_actions[action] && observation.safe_actions[action])
            actions[n++] = (Action)action;
    if (n == 0) env_legal_actions(env, actor, actions, &n);
    *count = n;
}

static Action adversarial_reply(const BomberEnv* env, int perspective, Action our_action, RNG* rng) {
    int opponent = first_alive_opponent(env, perspective);
    if (opponent < 0) return ACTION_WAIT;
    Action replies[ACTION_COUNT]; int reply_count = 0;
    safe_legal_actions(env, opponent, replies, &reply_count);
    Action preferred = tactical_action(env, opponent, perspective, rng);
    Action chosen = reply_count ? replies[0] : ACTION_WAIT;
    float worst = FLT_MAX;
    for (int i = 0; i < reply_count; i++) {
        BomberEnv child; env_copy(&child, env);
        Action joint[MAX_AGENTS]; wait_actions(joint);
        joint[perspective] = our_action; joint[opponent] = replies[i];
        env_step_joint(&child, joint, child.state.agent_count);
        float score = evaluator_score_state(&child, perspective);
        if (score < worst || (score == worst && replies[i] == preferred)) {
            worst = score; chosen = replies[i];
        }
    }
    return chosen;
}

static void apply_search_step(BomberEnv* env, int perspective, Action action, SearchImpl* impl) {
    Action joint[MAX_AGENTS]; wait_actions(joint);
    joint[perspective] = action;
    int opponent = first_alive_opponent(env, perspective);
    if (opponent >= 0) joint[opponent] = adversarial_reply(env, perspective, action, &impl->rng);
    env_step_joint(env, joint, env->state.agent_count);
}

static float bounded_value(const BomberEnv* env, int perspective, float root_score, int root_crates) {
    const BomberAgentState* me = &env->state.agents[perspective];
    int alive_enemies = 0, owned_eliminations = 0;
    for (int a = 0; a < env->state.agent_count; a++) if (a != perspective) {
        if (env->state.agents[a].alive) alive_enemies++;
        else if (env->state.death_owner[a] == perspective) owned_eliminations++;
    }
    if (!me->alive && alive_enemies == 0) return 0.0f;
    if (!me->alive) return -1.0f;
    if (alive_enemies == 0) return owned_eliminations ? 1.0f : 0.15f;
    if (env->state.step >= env->config.max_steps) return -0.8f;
    float value = (evaluator_score_state(env, perspective) - root_score) / 150.0f;
    int destroyed = root_crates - map_count_crates(&env->state);
    value += destroyed * 0.08f;
    if (value > 0.8f) value = 0.8f;
    if (value < -0.8f) value = -0.8f;
    return value;
}

static float rollout(BomberEnv* env, SearchImpl* impl, int perspective, int depth,
                     float root_score, int root_crates) {
    for (int step = 0; step < depth; step++) {
        int target = first_alive_opponent(env, perspective);
        if (target < 0 || rules_check_terminal(&env->state, perspective, env->config.max_steps) != TERMINAL_NONE) break;
        Action action = tactical_action(env, perspective, target, &impl->rng);
        apply_search_step(env, perspective, action, impl);
    }
    return bounded_value(env, perspective, root_score, root_crates);
}

static void uct_node_init(UctNode* node, int parent, Action action) {
    memset(node, 0, sizeof(*node));
    node->parent = parent; node->action = action;
    for (int i = 0; i < ACTION_COUNT; i++) node->children[i] = -1;
}

static int recent_position_count(const SearchImpl* impl, int x, int y) {
    int count = 0;
    for (int i = 0; i < impl->recent_count; i++)
        if (impl->recent_x[i] == x && impl->recent_y[i] == y) count++;
    return count;
}

static void remember_position(SearchImpl* impl, int x, int y) {
    impl->recent_x[impl->recent_cursor] = x; impl->recent_y[impl->recent_cursor] = y;
    impl->recent_cursor = (impl->recent_cursor + 1) % 12;
    if (impl->recent_count < 12) impl->recent_count++;
}

static Action mcts_act(Agent* agent, const Observation* obs, const DebugSnapshot* debug) {
    BomberEnv root; snapshot_env(debug, &root);
    SearchImpl* impl = (SearchImpl*)agent->impl;
    SearchDiagnostics* diag = &agent->diagnostics;
    int perspective = obs->agent_id;
    if (impl->last_observed_x >= 0 && impl->last_action <= ACTION_RIGHT &&
        obs->agent_x == impl->last_observed_x && obs->agent_y == impl->last_observed_y)
        impl->blocked_streak++;
    else
        impl->blocked_streak = 0;
    impl->last_observed_x = obs->agent_x; impl->last_observed_y = obs->agent_y;
    if (owner_has_active_bomb(&debug->state, perspective)) {
        BomberEnv emergency; snapshot_env(debug, &emergency);
        Action committed = ACTION_WAIT;
        int target = first_alive_opponent(&emergency, perspective);
        if (obs->valid_actions[ACTION_PLACE_BOMB] && obs->safe_actions[ACTION_PLACE_BOMB] &&
            tactically_useful_bomb(&emergency, perspective, target) &&
            robust_bomb_survival(&emergency, perspective)) {
            committed = ACTION_PLACE_BOMB;
        } else if (!robust_survival(&emergency, perspective,
                                    emergency.config.bomb_timer + 1, &committed))
            committed = tactical_action(&emergency, perspective,
                                        target, &impl->rng);
        remember_position(impl, obs->agent_x, obs->agent_y);
        memset(diag, 0, sizeof(*diag)); diag->selected_action = committed;
        impl->last_action = committed;
        return committed;
    } else if (!owner_has_active_bomb(&debug->state, perspective)) {
        impl->escape_length = 0; impl->escape_cursor = 0;
    }
    memset(diag, 0, sizeof(*diag)); diag->simulations = impl->budget; diag->depth = impl->depth;
    UctNode nodes[UCT_MAX_NODES]; int node_count = 1;
    uct_node_init(&nodes[0], -1, ACTION_WAIT);
    float root_score = evaluator_score_state(&root, perspective);
    int root_crates = map_count_crates(&root.state);
    for (int simulation = 0; simulation < impl->budget; simulation++) {
        BomberEnv state; env_copy(&state, &root);
        int node_index = 0, path[UCT_MAX_DEPTH], path_count = 1, depth = 0;
        path[0] = 0;
        while (depth < impl->depth && node_count < UCT_MAX_NODES &&
               rules_check_terminal(&state.state, perspective, state.config.max_steps) == TERMINAL_NONE) {
            Action legal[ACTION_COUNT]; int count = 0;
            safe_legal_actions(&state, perspective, legal, &count);
            if (!count) break;
            Action preferred = tactical_action(&state, perspective, first_alive_opponent(&state, perspective), &impl->rng);
            Action chosen_action = legal[0]; int child_index = -1, expanded = 0;
            for (int pass = 0; pass < 2 && child_index < 0; pass++) for (int i = 0; i < count; i++) {
                Action candidate = legal[i];
                if (nodes[node_index].children[candidate] < 0 && (pass || candidate == preferred)) {
                    chosen_action = candidate;
                    child_index = node_count++;
                    uct_node_init(&nodes[child_index], node_index, candidate);
                    nodes[node_index].children[candidate] = child_index;
                    expanded = 1;
                    break;
                }
            }
            if (child_index < 0) {
                float best_ucb = -FLT_MAX;
                for (int i = 0; i < count; i++) {
                    Action candidate = legal[i]; int candidate_index = nodes[node_index].children[candidate];
                    if (candidate_index < 0) continue;
                    UctNode* child = &nodes[candidate_index];
                    float mean = child->value_sum / (float)child->visits;
                    float explore = 1.2f * sqrtf(logf((float)nodes[node_index].visits + 1.0f) /
                                                    (float)child->visits);
                    float prior = candidate == preferred ? 0.08f / (float)(child->visits + 1) : 0.0f;
                    if (mean + explore + prior > best_ucb) {
                        best_ucb = mean + explore + prior; chosen_action = candidate; child_index = candidate_index;
                    }
                }
            }
            if (child_index < 0) break;
            apply_search_step(&state, perspective, chosen_action, impl);
            node_index = child_index; path[path_count++] = node_index; depth++;
            if (expanded) break;
        }
        float value = rollout(&state, impl, perspective, impl->depth - depth, root_score, root_crates);
        for (int p = 0; p < path_count; p++) {
            nodes[path[p]].visits++;
            nodes[path[p]].value_sum += value;
        }
    }
    remember_position(impl, obs->agent_x, obs->agent_y);
    int target = first_alive_opponent(&root, perspective);
    Action preferred = tactical_action(&root, perspective, target, &impl->rng);
    static const int move_dx[] = {0, 0, -1, 1}; static const int move_dy[] = {-1, 1, 0, 0};
    Action chosen = ACTION_WAIT; float best = -FLT_MAX;
    for (int action = 0; action < ACTION_COUNT; action++) {
        int child_index = nodes[0].children[action];
        if (child_index < 0 || nodes[child_index].visits == 0) continue;
        if (action == impl->last_action && impl->blocked_streak > 0) continue;
        float mean = nodes[child_index].value_sum / (float)nodes[child_index].visits;
        diag->visits[action] = nodes[child_index].visits;
        diag->action_values[action] = mean;
        float selection = (float)nodes[child_index].visits + mean * 5.0f;
        if (action <= ACTION_RIGHT)
            selection -= recent_position_count(impl, obs->agent_x + move_dx[action],
                                                obs->agent_y + move_dy[action]) * 12.0f;
        if (action == ACTION_WAIT && obs->danger_timer < 0) selection -= 20.0f;
        if (action == preferred) selection += 4.0f;
        if (action == ACTION_PLACE_BOMB && tactically_useful_bomb(&root, perspective, target) &&
            bomb_has_survival_plan(&root, perspective)) selection += 15.0f;
        if (selection > best) { best = selection; chosen = (Action)action; }
    }
    if (chosen == ACTION_WAIT && obs->danger_timer < 0 && !obs->in_danger) {
        int active_visits = -1; float active_mean = -FLT_MAX;
        for (int action = ACTION_UP; action <= ACTION_PLACE_BOMB; action++) {
            int child_index = nodes[0].children[action];
            if (action == impl->last_action && impl->blocked_streak > 0) continue;
            if (!obs->valid_actions[action] || !obs->safe_actions[action] || child_index < 0) continue;
            float mean = nodes[child_index].value_sum / (float)nodes[child_index].visits;
            if (nodes[child_index].visits > active_visits ||
                (nodes[child_index].visits == active_visits && mean > active_mean)) {
                active_visits = nodes[child_index].visits; active_mean = mean; chosen = (Action)action;
            }
        }
        if (active_visits >= 0) best = active_mean;
    }
    if (chosen == ACTION_PLACE_BOMB && !robust_bomb_survival(&root, perspective)) {
        if (preferred != ACTION_PLACE_BOMB && obs->valid_actions[preferred] && obs->safe_actions[preferred] &&
            !(preferred == impl->last_action && impl->blocked_streak > 0))
            chosen = preferred;
        else {
            chosen = ACTION_WAIT;
            for (int action = ACTION_UP; action <= ACTION_RIGHT; action++)
                if (obs->valid_actions[action] && obs->safe_actions[action] &&
                    !(action == impl->last_action && impl->blocked_streak > 0)) {
                    chosen = (Action)action; break;
                }
        }
    }
    if (chosen == ACTION_PLACE_BOMB) {
        int length = 0;
        if (bomb_survival_plan(&root, perspective, impl->escape_plan, &length)) {
            impl->escape_length = length; impl->escape_cursor = 0;
        }
    }
    diag->nodes = node_count;
    diag->selected_action = chosen; diag->value = best;
    impl->last_action = chosen;
    return chosen;
}

static void init_common(Agent* agent, const char* name, AgentActFn act, int budget, int depth) {
    SearchImpl* impl = (SearchImpl*)agent_impl_storage(agent, sizeof(SearchImpl));
    agent->impl = impl; impl->budget = budget; impl->depth = depth;
    agent->act = act; agent->reset = reset_search;
    strncpy(agent->name, name, sizeof(agent->name) - 1); reset_search(agent, 1);
}

void alphabeta_agent_init(Agent* agent) { init_common(agent, "alpha-beta", alphabeta_act, 0, 2); }
void mcts_agent_init(Agent* agent) { init_common(agent, "mcts", mcts_act, 96, 12); }
