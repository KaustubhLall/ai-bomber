#ifndef BOMBER_MATCH_HISTORY_H
#define BOMBER_MATCH_HISTORY_H

#include "core/replay.h"

#define MATCH_HISTORY_MAX 256
#define MATCH_HISTORY_PATH 260

typedef struct {
    int id;
    uint64_t seed;
    char agent[32];
    char opponent[32];
    TerminalReason outcome;
    int steps;
    int owned_eliminations;
    int self_kills;
    int opponent_self_kills;
    int opponent_kills;
    int crate_density;
    int map_width;
    int map_height;
    uint64_t terminal_hash;
    char replay_path[MATCH_HISTORY_PATH];
} MatchHistoryEntry;

typedef struct {
    char directory[MATCH_HISTORY_PATH];
    MatchHistoryEntry entries[MATCH_HISTORY_MAX];
    int count;
    int selected;
} MatchHistory;

int match_history_init(MatchHistory* history, const char* directory);
int match_history_add(MatchHistory* history, const Replay* replay,
                      TerminalReason outcome, int owned_eliminations,
                      int self_kills, int opponent_self_kills, int opponent_kills);
int match_history_load_replay(const MatchHistory* history, int index, Replay* replay);

#endif
