#include "core/match_history.h"
#include "env/env.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#define RMDIR(path) _rmdir(path)
#else
#include <unistd.h>
#define RMDIR(path) rmdir(path)
#endif

int main(void) {
    remove("test-history/index.csv");
    remove("test-history/index.json");
    remove("test-history/replays/match-000001.bin");
    remove("test-history/replays/match-000002.bin");
    RMDIR("test-history/replays"); RMDIR("test-history");
    MatchHistory history;
    assert(match_history_init(&history, "test-history"));
    BomberConfig config; config_battle(&config); config.agent_count = 2;
    BomberEnv env = {0}; env_init(&env, &config); env_reset(&env, 123);
    Replay* replay = (Replay*)calloc(1, sizeof(Replay));
    Replay* loaded = (Replay*)calloc(1, sizeof(Replay));
    assert(replay && loaded);
    replay_init(replay, &config, 123);
    replay_set_policies(replay, "mcts", "heuristic");
    StepResult result = env_step(&env, ACTION_WAIT);
    replay_record_env(replay, &env, result);
    uint64_t first_hash = replay->frames[0].state_hash;
    assert(match_history_add(&history, replay, result.terminal_reason, 0, 0, 0, 0));
    assert(history.count == 1);
    env_reset(&env, 124);
    replay_init(replay, &config, 124);
    replay_set_policies(replay, "heuristic", "greedy");
    result = env_step(&env, ACTION_WAIT);
    replay_record_env(replay, &env, result);
    assert(match_history_add(&history, replay, result.terminal_reason, 0, 0, 0, 0));
    assert(history.count == 2 && history.entries[1].id > history.entries[0].id);
    FILE* json = fopen("test-history/index.json", "r");
    assert(json); fclose(json);
    MatchHistory restarted;
    assert(match_history_init(&restarted, "test-history"));
    assert(restarted.count == 2 && restarted.selected == 1);
    assert(match_history_load_replay(&history, 0, loaded));
    assert(loaded->frame_count == 1 && loaded->frames[0].state_hash == first_hash);
    uint64_t validated_hash = 0;
    assert(replay_validate(loaded, &validated_hash));
    assert(validated_hash == first_hash);
    free(replay); free(loaded);
    remove("test-history/index.csv");
    remove("test-history/index.json");
    remove("test-history/replays/match-000001.bin");
    remove("test-history/replays/match-000002.bin");
    RMDIR("test-history/replays"); RMDIR("test-history");
    puts("match history tests passed");
    return 0;
}
