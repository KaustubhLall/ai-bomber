/* Replay round-trip coverage for the v4 state that the polished renderer depends on but
   the older test_replay never exercised: persistent flame (flame_ttl/flame_owner) and
   sudden-death walls. Drives a controlled battle so a real bomb detonation lays flame and
   the closing arena walls an interior tile, then asserts both survive replay_save/load
   byte-for-byte and re-simulate deterministically. Guards against a serialization change
   (e.g. a field-by-field writer) silently dropping the flame layer. */
#include "env/env.h"
#include "core/replay.h"
#include "env/bomber_map.h"
#include "env/bomber_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* First recorded frame carrying live flame; reports one lit tile and its owner. */
static int find_flame_frame(const Replay* r, int* out_x, int* out_y, int* out_owner) {
    for (int f = 0; f < r->frame_count; f++) {
        const BomberState* s = &r->frames[f].state;
        for (int y = 0; y < s->height; y++)
            for (int x = 0; x < s->width; x++)
                if (s->flame_ttl[y][x] > 0) {
                    if (out_x) *out_x = x;
                    if (out_y) *out_y = y;
                    if (out_owner) *out_owner = s->flame_owner[y][x];
                    return f;
                }
    }
    return -1;
}

static void test_flame_roundtrip(void) {
    BomberConfig cfg;
    config_battle(&cfg);
    cfg.seed = 7;
    cfg.crate_density = 0;      /* open interior: deterministic movement, no crate/powerup RNG */
    cfg.flame_duration = 3;     /* flame lingers a few ticks so it lands in recorded frames */
    cfg.blast_range = 2;
    cfg.bomb_timer = 4;
    cfg.sudden_death_start = 0; /* arena stays open for the flame scenario */
    cfg.max_steps = 40;
    config_normalize(&cfg);

    BomberEnv env;
    env_init(&env, &cfg);
    /* Corner spawns: agent 0 at (1,1), agent 1 at the opposite corner. */
    assert(env.state.agents[0].x == 1 && env.state.agents[0].y == 1);

    Replay* rec = (Replay*)calloc(1, sizeof(Replay));
    replay_init(rec, &cfg, cfg.seed);
    replay_set_policies(rec, "scripted-bomber", "idle");

    /* Place a bomb at (1,1), then flee DOWN,DOWN,RIGHT to (2,3) — a diagonal the
       cross-shaped range-2 blast (covering {(1,1),(1,2),(1,3),(2,1),(3,1)}) cannot reach.
       The bomb (timer 4, decremented on the placement step) detonates on the 4th step,
       after agent 0's move that step, so agent 0 is already clear. */
    Action seq0[] = { ACTION_PLACE_BOMB, ACTION_DOWN, ACTION_DOWN, ACTION_RIGHT,
                      ACTION_WAIT, ACTION_WAIT, ACTION_WAIT, ACTION_WAIT };
    int nseq = (int)(sizeof(seq0) / sizeof(seq0[0]));
    for (int i = 0; i < nseq; i++) {
        Action acts[2] = { seq0[i], ACTION_WAIT };
        StepResult r = env_step_joint(&env, acts, 2);
        replay_record_env(rec, &env, r);
        if (r.done) break;
    }
    assert(env.state.agents[0].alive);   /* the escape worked; agent survived its own bomb */

    int fx = -1, fy = -1, fowner = -2;
    int flame_frame = find_flame_frame(rec, &fx, &fy, &fowner);
    assert(flame_frame >= 0);            /* the detonation actually laid recorded flame */
    assert(fowner == 0);                 /* flame is attributed to the bomb's owner (agent 0) */

    const char* path = "test_replay_flame.bin";
    assert(replay_save(rec, path));
    Replay* ld = (Replay*)calloc(1, sizeof(Replay));
    assert(replay_load(ld, path));       /* version gate accepts our freshly-written v4 */
    assert(ld->frame_count == rec->frame_count);

    /* The whole flame layer of the recorded frame is preserved byte-for-byte. */
    const BomberState* a = &rec->frames[flame_frame].state;
    const BomberState* b = &ld->frames[flame_frame].state;
    assert(memcmp(a->flame_ttl, b->flame_ttl, sizeof(a->flame_ttl)) == 0);
    assert(memcmp(a->flame_owner, b->flame_owner, sizeof(a->flame_owner)) == 0);
    assert(b->flame_ttl[fy][fx] > 0 && b->flame_owner[fy][fx] == 0);
    assert(b->flame_duration == cfg.flame_duration);

    /* The loaded replay re-simulates deterministically (flame is part of the hashed state). */
    uint64_t final_hash = 0;
    assert(replay_validate(ld, &final_hash));

    remove(path);
    free(rec);
    free(ld);
}

static void test_sudden_death_roundtrip(void) {
    BomberConfig cfg;
    config_battle(&cfg);
    cfg.seed = 11;
    cfg.crate_density = 0;
    cfg.sudden_death_start = 4;  /* close the arena early so a wall lands in a few frames */
    cfg.shrink_interval = 2;
    cfg.max_steps = 40;
    config_normalize(&cfg);

    BomberEnv env;
    env_init(&env, &cfg);
    /* Snapshot the opening tiles so we can spot ones the closing arena converts to wall. */
    TileType initial[MAX_HEIGHT][MAX_WIDTH];
    memcpy(initial, env.state.tiles, sizeof(initial));

    Replay* rec = (Replay*)calloc(1, sizeof(Replay));
    replay_init(rec, &cfg, cfg.seed);
    replay_set_policies(rec, "idle", "idle");
    for (int i = 0; i < 20; i++) {
        Action acts[2] = { ACTION_WAIT, ACTION_WAIT };
        StepResult r = env_step_joint(&env, acts, 2);
        replay_record_env(rec, &env, r);
        if (r.done) break;
    }

    /* Locate an interior floor tile that sudden death turned into a solid wall. */
    int wall_frame = -1, wx = -1, wy = -1;
    for (int f = 0; f < rec->frame_count && wall_frame < 0; f++) {
        const BomberState* s = &rec->frames[f].state;
        for (int y = 1; y < s->height - 1 && wall_frame < 0; y++)
            for (int x = 1; x < s->width - 1; x++)
                if (initial[y][x] != TILE_SOLID_WALL && s->tiles[y][x] == TILE_SOLID_WALL) {
                    wall_frame = f; wx = x; wy = y; break;
                }
    }
    assert(wall_frame >= 0);   /* the arena actually walled an interior tile */

    const char* path = "test_replay_sudden_death.bin";
    assert(replay_save(rec, path));
    Replay* ld = (Replay*)calloc(1, sizeof(Replay));
    assert(replay_load(ld, path));
    assert(ld->frames[wall_frame].state.tiles[wy][wx] == TILE_SOLID_WALL);
    uint64_t final_hash = 0;
    assert(replay_validate(ld, &final_hash));

    remove(path);
    free(rec);
    free(ld);
}

int main(void) {
    test_flame_roundtrip();
    test_sudden_death_roundtrip();
    printf("test_replay_flame: ALL PASSED\n");
    return 0;
}
