#include "env/bomber_danger.h"
#include "env/bomber_map.h"
#include "env/bomber_blast.h"
#include "core/math_util.h"
#include <string.h>

/* BFS queue for reachability */
typedef struct {
    int x;
    int y;
    int dist;
} BFSNode;

static void bfs_reachable(const BomberState* state, int sx, int sy, int max_dist,
                          int reachable[MAX_HEIGHT][MAX_WIDTH]) {
    BFSNode queue[MAX_WIDTH * MAX_HEIGHT];
    int qhead = 0, qtail = 0;
    memset(reachable, 0, sizeof(int) * MAX_HEIGHT * MAX_WIDTH);

    queue[qtail].x = sx; queue[qtail].y = sy; queue[qtail].dist = 0;
    qtail++;
    reachable[sy][sx] = 1;

    int dx[4] = {0, 0, -1, 1};
    int dy[4] = {-1, 1, 0, 0};

    while (qhead < qtail) {
        BFSNode cur = queue[qhead++];
        if (cur.dist >= max_dist) continue;
        for (int d = 0; d < 4; d++) {
            int nx = cur.x + dx[d];
            int ny = cur.y + dy[d];
            if (!map_in_bounds(state, nx, ny)) continue;
            if (reachable[ny][nx]) continue;
            if (!map_is_walkable(state, nx, ny)) continue;
            reachable[ny][nx] = 1;
            queue[qtail].x = nx; queue[qtail].y = ny; queue[qtail].dist = cur.dist + 1;
            qtail++;
        }
    }
}

void danger_compute(DangerMap* dm, const BomberState* state) {
    memset(dm->current_blast, 0, sizeof(int) * MAX_HEIGHT * MAX_WIDTH);
    for (int y = 0; y < MAX_HEIGHT; y++) {
        for (int x = 0; x < MAX_WIDTH; x++) {
            dm->time_to_blast[y][x] = -1;
            dm->safe_now[y][x] = 1;
        }
    }
    memset(dm->reachable_safe, 0, sizeof(int) * MAX_HEIGHT * MAX_WIDTH);

    /* For each active bomb, compute predicted blast tiles and time-to-blast */
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!state->bombs[i].active) continue;
        int timer = state->bombs[i].timer;
        BlastResult blast;
        compute_blast_tiles(state, state->bombs[i].x, state->bombs[i].y,
                           state->bombs[i].range, &blast);
        for (int j = 0; j < blast.count; j++) {
            int bx = blast.tiles[j].x;
            int by = blast.tiles[j].y;
            if (timer == 0) {
                dm->current_blast[by][bx] = 1;
                dm->safe_now[by][bx] = 0;
            }
            /* Time to blast: minimum of existing and this bomb's timer */
            if (dm->time_to_blast[by][bx] < 0 || timer < dm->time_to_blast[by][bx]) {
                dm->time_to_blast[by][bx] = timer;
            }
            dm->safe_now[by][bx] = 0;
        }
    }
}

void danger_compute_escape(DangerMap* dm, const BomberState* state, int agent_id) {
    if (agent_id < 0 || agent_id >= state->agent_count) return;
    if (!state->agents[agent_id].alive) return;

    int ax = state->agents[agent_id].x;
    int ay = state->agents[agent_id].y;

    /* BFS to find reachable safe tiles within a reasonable distance */
    BFSNode queue[MAX_WIDTH * MAX_HEIGHT];
    int qhead = 0, qtail = 0;
    int visited[MAX_HEIGHT][MAX_WIDTH];
    memset(visited, 0, sizeof(int) * MAX_HEIGHT * MAX_WIDTH);

    queue[qtail].x = ax; queue[qtail].y = ay; queue[qtail].dist = 0;
    qtail++;
    visited[ay][ax] = 1;

    int dx[4] = {0, 0, -1, 1};
    int dy[4] = {-1, 1, 0, 0};

    while (qhead < qtail) {
        BFSNode cur = queue[qhead++];
        /* Check if this tile is safe (no time_to_blast or time_to_blast > dist) */
        int tblast = dm->time_to_blast[cur.y][cur.x];
        if (tblast < 0 || tblast > cur.dist) {
            dm->reachable_safe[cur.y][cur.x] = 1;
        }
        if (cur.dist >= 10) continue;
        for (int d = 0; d < 4; d++) {
            int nx = cur.x + dx[d];
            int ny = cur.y + dy[d];
            if (!map_in_bounds(state, nx, ny)) continue;
            if (visited[ny][nx]) continue;
            if (!map_is_walkable(state, nx, ny)) continue;
            visited[ny][nx] = 1;
            queue[qtail].x = nx; queue[qtail].y = ny; queue[qtail].dist = cur.dist + 1;
            qtail++;
        }
    }

    /* Compute action_safe: which actions lead to safe tiles.
       Movement actions are checked at arrival time (1 tick from now),
       not just safe_now, so a tile that will blast in 3 ticks is still
       safe to move into. WAIT and BOMB remain conservative. */
    dm->action_safe[ACTION_WAIT] = dm->safe_now[ay][ax] ? 1 : 0;

    int move_dx[4] = {0, 0, -1, 1};
    int move_dy[4] = {-1, 1, 0, 0};
    for (int a = 0; a < 4; a++) {
        int nx = ax + move_dx[a];
        int ny = ay + move_dy[a];
        if (!map_in_bounds(state, nx, ny) || !map_is_walkable(state, nx, ny)) {
            dm->action_safe[a] = 0;
        } else {
            dm->action_safe[a] = danger_is_action_safe_at_arrival(dm, nx, ny, 1);
        }
    }

    dm->action_safe[ACTION_PLACE_BOMB] = danger_would_trap_agent(state, agent_id, ax, ay) ? 0 : 1;
}

int danger_is_tile_safe(const DangerMap* dm, int x, int y, int ticks_ahead) {
    if (x < 0 || x >= MAX_WIDTH || y < 0 || y >= MAX_HEIGHT) return 0;
    int tblast = dm->time_to_blast[y][x];
    if (tblast < 0) return 1; /* no danger */
    return tblast > ticks_ahead;
}

int danger_is_action_safe_at_arrival(const DangerMap* dm, int x, int y, int arrival_ticks) {
    if (x < 0 || x >= MAX_WIDTH || y < 0 || y >= MAX_HEIGHT) return 0;
    /* A tile is safe at arrival if no blast is currently exploding there
       and the blast won't arrive before or at the same time the agent does. */
    if (dm->current_blast[y][x]) return 0;
    int tblast = dm->time_to_blast[y][x];
    if (tblast < 0) return 1; /* no danger scheduled */
    return tblast > arrival_ticks;
}

int danger_detect_dead_end(const BomberState* state, int x, int y) {
    int exits = 0;
    int dx[4] = {0, 0, -1, 1};
    int dy[4] = {-1, 1, 0, 0};
    for (int d = 0; d < 4; d++) {
        int nx = x + dx[d];
        int ny = y + dy[d];
        if (map_in_bounds(state, nx, ny) && map_is_walkable(state, nx, ny)) {
            exits++;
        }
    }
    return exits <= 1;
}

int danger_would_trap_agent(const BomberState* state, int agent_id, int bomb_x, int bomb_y) {
    if (agent_id < 0 || agent_id >= state->agent_count) return 1;
    int ax = state->agents[agent_id].x;
    int ay = state->agents[agent_id].y;

    /* Simulate: if a bomb were placed at (bomb_x, bomb_y) with agent's range,
       can the agent reach a safe tile? */
    int range = state->agents[agent_id].blast_range;
    int timer = 4; /* default bomb timer */

    /* Compute blast tiles for hypothetical bomb */
    BlastResult blast;
    compute_blast_tiles(state, bomb_x, bomb_y, range, &blast);

    /* Mark blast tiles */
    int blast_mask[MAX_HEIGHT][MAX_WIDTH];
    memset(blast_mask, 0, sizeof(int) * MAX_HEIGHT * MAX_WIDTH);
    for (int i = 0; i < blast.count; i++) {
        blast_mask[blast.tiles[i].y][blast.tiles[i].x] = 1;
    }

    /* BFS from agent position, avoiding blast tiles and the bomb tile */
    BFSNode queue[MAX_WIDTH * MAX_HEIGHT];
    int qhead = 0, qtail = 0;
    int visited[MAX_HEIGHT][MAX_WIDTH];
    memset(visited, 0, sizeof(int) * MAX_HEIGHT * MAX_WIDTH);

    queue[qtail].x = ax; queue[qtail].y = ay; queue[qtail].dist = 0;
    qtail++;
    visited[ay][ax] = 1;

    int dx[4] = {0, 0, -1, 1};
    int dy[4] = {-1, 1, 0, 0};

    while (qhead < qtail) {
        BFSNode cur = queue[qhead++];
        if (cur.dist >= timer) continue;
        /* If we reached a non-blast tile, escape is possible */
        if (!blast_mask[cur.y][cur.x] && !(cur.x == bomb_x && cur.y == bomb_y)) {
            /* But also need to not be on a blast tile at explosion time */
            /* Simple check: if we're outside blast radius, we're safe */
            if (cur.dist > 0) return 0; /* escape found */
        }
        for (int d = 0; d < 4; d++) {
            int nx = cur.x + dx[d];
            int ny = cur.y + dy[d];
            if (!map_in_bounds(state, nx, ny)) continue;
            if (visited[ny][nx]) continue;
            if (!map_is_walkable(state, nx, ny)) continue;
            /* Can't walk through the bomb tile (except starting on it) */
            if (nx == bomb_x && ny == bomb_y && !(cur.x == ax && cur.y == ay)) continue;
            visited[ny][nx] = 1;
            queue[qtail].x = nx; queue[qtail].y = ny; queue[qtail].dist = cur.dist + 1;
            qtail++;
        }
    }

    /* Check if any reachable tile is outside blast */
    for (int y = 0; y < state->height; y++) {
        for (int x = 0; x < state->width; x++) {
            if (visited[y][x] && !blast_mask[y][x] && !(x == bomb_x && y == bomb_y)) {
                return 0; /* escape exists */
            }
        }
    }

    return 1; /* trapped */
}
