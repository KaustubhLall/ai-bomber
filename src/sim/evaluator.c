#include "sim/evaluator.h"
#include "sim/runner.h"
#include "core/metrics.h"
#include "env/bomber_map.h"
#include <stdio.h>
#include <string.h>

int evaluator_opponent_escape_options(const BomberEnv* env, int agent_id) {
    if (!env || agent_id < 0 || agent_id >= env->state.agent_count) return -1;
    int horizon = 999;
    for (int b = 0; b < MAX_BOMBS; b++)
        if (env->state.bombs[b].active && env->state.bombs[b].owner_id == agent_id &&
            env->state.bombs[b].timer < horizon)
            horizon = env->state.bombs[b].timer;
    if (horizon == 999) return -1;
    if (horizon < 0) horizon = 0;

    unsigned char current[MAX_HEIGHT][MAX_WIDTH] = {{0}};
    unsigned char next[MAX_HEIGHT][MAX_WIDTH];
    for (int a = 0; a < env->state.agent_count; a++)
        if (a != agent_id && env->state.agents[a].alive)
            current[env->state.agents[a].y][env->state.agents[a].x] = 1;
    static const int dx[] = {0, 0, 0, -1, 1};
    static const int dy[] = {0, -1, 1, 0, 0};
    for (int tick = 1; tick <= horizon; tick++) {
        memset(next, 0, sizeof(next));
        for (int y = 0; y < env->state.height; y++) for (int x = 0; x < env->state.width; x++) {
            if (!current[y][x]) continue;
            for (int d = 0; d < 5; d++) {
                int nx = x + dx[d], ny = y + dy[d];
                if (!map_in_bounds(&env->state, nx, ny)) continue;
                if ((nx != x || ny != y) && !map_is_walkable(&env->state, nx, ny)) continue;
                if (env->danger.time_to_blast[ny][nx] == tick) continue;
                next[ny][nx] = 1;
            }
        }
        memcpy(current, next, sizeof(current));
    }
    int options = 0;
    for (int y = 0; y < env->state.height; y++) for (int x = 0; x < env->state.width; x++)
        options += current[y][x] ? 1 : 0;
    return options;
}

float evaluator_score_state(const BomberEnv* env, int agent_id) {
    if (!env || agent_id < 0 || agent_id >= env->state.agent_count) return -100000.0f;
    const BomberAgentState* me = &env->state.agents[agent_id];
    if (!me->alive) return -10000.0f;
    if (env->state.step >= env->config.max_steps) return -500.0f;
    int enemies = 0, mobility = 0, nearby_crates = 0, nearest_enemy = 999;
    int opponent_exits = 0, attack_lines = 0;
    int owned_bombs = 0, threatened_enemies = 0, threatened_crates = 0, owned_eliminations = 0;
    for (int a = 0; a < env->state.agent_count; a++) if (a != agent_id && env->state.agents[a].alive) {
        int dx = env->state.agents[a].x - me->x; if (dx < 0) dx = -dx;
        int dy = env->state.agents[a].y - me->y; if (dy < 0) dy = -dy;
        int distance = dx + dy; if (distance < nearest_enemy) nearest_enemy = distance;
        static const int mx[] = {0, 0, -1, 1}; static const int my[] = {-1, 1, 0, 0};
        for (int d = 0; d < 4; d++)
            if (map_is_walkable(&env->state, env->state.agents[a].x + mx[d],
                                env->state.agents[a].y + my[d])) opponent_exits++;
        if ((dx == 0 || dy == 0) && distance <= me->blast_range) {
            int step_x = dx == 0 ? 0 : (env->state.agents[a].x > me->x ? 1 : -1);
            int step_y = dy == 0 ? 0 : (env->state.agents[a].y > me->y ? 1 : -1);
            int clear = 1;
            for (int s = 1; s < distance; s++) {
                TileType tile = env->state.tiles[me->y + step_y * s][me->x + step_x * s];
                if (tile == TILE_SOLID_WALL || tile == TILE_CRATE) { clear = 0; break; }
            }
            if (clear) attack_lines++;
        }
        enemies++;
    }
    for (int a = 0; a < env->state.agent_count; a++)
        if (a != agent_id && !env->state.agents[a].alive && env->state.death_owner[a] == agent_id)
            owned_eliminations++;
    for (int b = 0; b < MAX_BOMBS; b++) if (env->state.bombs[b].active && env->state.bombs[b].owner_id == agent_id) {
        const BombState* bomb = &env->state.bombs[b]; owned_bombs++;
        static const int dx[] = {0, 0, -1, 1}; static const int dy[] = {-1, 1, 0, 0};
        for (int d = 0; d < 4; d++) for (int r = 1; r <= bomb->range; r++) {
            int x = bomb->x + dx[d] * r, y = bomb->y + dy[d] * r;
            if (x < 0 || y < 0 || x >= env->state.width || y >= env->state.height ||
                env->state.tiles[y][x] == TILE_SOLID_WALL) break;
            for (int a = 0; a < env->state.agent_count; a++)
                if (a != agent_id && env->state.agents[a].alive &&
                    env->state.agents[a].x == x && env->state.agents[a].y == y)
                    threatened_enemies++;
            if (env->state.tiles[y][x] == TILE_CRATE) { threatened_crates++; break; }
        }
    }
    if (env->state.agent_count > 1 && enemies == 0)
        return owned_eliminations > 0 ? 10000.0f : 2500.0f;
    Action legal[ACTION_COUNT]; env_legal_actions(env, agent_id, legal, &mobility);
    for (int y = me->y - 2; y <= me->y + 2; y++) for (int x = me->x - 2; x <= me->x + 2; x++)
        if (x >= 0 && y >= 0 && x < env->state.width && y < env->state.height && env->state.tiles[y][x] == TILE_CRATE) nearby_crates++;
    int danger = env->danger.time_to_blast[me->y][me->x];
    float score = 100.0f - enemies * 60.0f + me->bomb_ammo + me->blast_range * 3.0f;
    score += me->crates_destroyed * 5.0f + me->powerups_collected * 12.0f + me->eliminations * 200.0f;
    score += mobility * 3.0f + nearby_crates * 1.5f - owned_bombs * 1.0f;
    score += threatened_enemies * 35.0f + threatened_crates * 4.0f;
    score += attack_lines * 25.0f - opponent_exits * 4.0f;
    if (nearest_enemy < 999) score -= nearest_enemy * 2.0f;
    score -= env->state.step * 0.10f;
    if (danger >= 0) score -= 20.0f / (float)(danger + 1);
    return score;
}

EvalResult evaluator_run(AgentType agent_type, const BomberConfig* cfg,
                         int episodes, uint64_t seed) {
    RunConfig rc;
    rc.config = *cfg;
    rc.agent_type = agent_type;
    rc.enemy_type = (AgentType)-1;
    rc.seed = seed;
    rc.episodes = episodes;
    rc.record_replay = 0;

    Metrics metrics;
    runner_run(&rc, &metrics);

    EvalResult result;
    result.episodes = metrics.episodes;
    result.avg_reward = metrics.episodes > 0 ? metrics.total_reward / metrics.episodes : 0.0f;
    result.win_rate = metrics.episodes > 0 ? (float)metrics.wins / metrics.episodes : 0.0f;
    result.death_rate = metrics.episodes > 0 ? (float)metrics.deaths / metrics.episodes : 0.0f;
    result.avg_episode_length = metrics.episodes > 0 ? (float)metrics.total_steps / metrics.episodes : 0.0f;
    result.crate_destruction_rate = metrics.episodes > 0 ? (float)metrics.crates_destroyed / metrics.episodes : 0.0f;
    result.powerup_pickup_rate = metrics.episodes > 0 ? (float)metrics.powerups_collected / metrics.episodes : 0.0f;

    return result;
}

void evaluator_compare(AgentType* types, int num_types, const BomberConfig* cfg,
                       int episodes, uint64_t seed) {
    printf("=== Agent Comparison ===\n");
    printf("%-15s %10s %10s %10s %10s %10s %10s\n",
           "Agent", "AvgReward", "WinRate", "DeathRate", "AvgLen", "Crates", "Powerups");

    for (int i = 0; i < num_types; i++) {
        EvalResult r = evaluator_run(types[i], cfg, episodes, seed);
        printf("%-15s %10.4f %10.2f %10.2f %10.1f %10.2f %10.2f\n",
               agent_type_name(types[i]), r.avg_reward, r.win_rate, r.death_rate,
               r.avg_episode_length, r.crate_destruction_rate, r.powerup_pickup_rate);
    }
}
