#include "core/match_history.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

static void ensure_directories(const char* directory) {
    char replay_dir[MATCH_HISTORY_PATH];
    (void)MKDIR(directory);
    snprintf(replay_dir, sizeof(replay_dir), "%s/replays", directory);
    (void)MKDIR(replay_dir);
}

static int write_json_index(const MatchHistory* history) {
    char path[MATCH_HISTORY_PATH];
    snprintf(path, sizeof(path), "%s/index.json", history->directory);
    FILE* file = fopen(path, "w");
    if (!file) return 0;
    fprintf(file, "{\n  \"schema_version\": 1,\n  \"matches\": [\n");
    for (int i = 0; i < history->count; i++) {
        const MatchHistoryEntry* e = &history->entries[i];
        fprintf(file,
                "    %s{\"id\":%d,\"seed\":%llu,\"agent\":\"%s\",\"opponent\":\"%s\","
                "\"outcome\":%d,\"steps\":%d,\"owned_eliminations\":%d,\"self_kills\":%d,"
                "\"opponent_self_kills\":%d,\"opponent_kills\":%d,\"crate_density\":%d,"
                "\"map_width\":%d,\"map_height\":%d,\"terminal_hash\":%llu,"
                "\"replay_path\":\"%s\"}",
                i ? "," : "", e->id, (unsigned long long)e->seed, e->agent, e->opponent,
                (int)e->outcome, e->steps, e->owned_eliminations, e->self_kills,
                e->opponent_self_kills, e->opponent_kills, e->crate_density, e->map_width, e->map_height,
                (unsigned long long)e->terminal_hash, e->replay_path);
        fputc('\n', file);
    }
    fprintf(file, "  ]\n}\n");
    fclose(file);
    return 1;
}

int match_history_init(MatchHistory* history, const char* directory) {
    if (!history || !directory) return 0;
    memset(history, 0, sizeof(*history));
    snprintf(history->directory, sizeof(history->directory), "%s", directory);
    history->selected = -1;
    ensure_directories(directory);
    char index_path[MATCH_HISTORY_PATH];
    snprintf(index_path, sizeof(index_path), "%s/index.csv", directory);
    FILE* file = fopen(index_path, "r");
    if (!file) return 1;
    char line[1024];
    (void)fgets(line, sizeof(line), file); /* header */
    while (history->count < MATCH_HISTORY_MAX && fgets(line, sizeof(line), file)) {
        MatchHistoryEntry* entry = &history->entries[history->count];
        unsigned long long seed = 0, hash = 0;
        int outcome = 0;
        int parsed = sscanf(line, "%d,%llu,%31[^,],%31[^,],%d,%d,%d,%d,%d,%d,%llu,%d,%d,%d,%259[^\r\n]",
                            &entry->id, &seed, entry->agent, entry->opponent, &outcome,
                            &entry->steps, &entry->owned_eliminations, &entry->self_kills,
                            &entry->opponent_self_kills, &entry->opponent_kills, &hash,
                            &entry->crate_density, &entry->map_width, &entry->map_height,
                            entry->replay_path);
        if (parsed == 15) {
            entry->seed = (uint64_t)seed;
            entry->outcome = (TerminalReason)outcome;
            entry->terminal_hash = (uint64_t)hash;
            history->count++;
        } else {
            memset(entry, 0, sizeof(*entry)); seed = hash = 0; outcome = 0;
            parsed = sscanf(line, "%d,%llu,%31[^,],%31[^,],%d,%d,%d,%d,%d,%d,%llu,%259[^\r\n]",
                            &entry->id, &seed, entry->agent, entry->opponent, &outcome,
                            &entry->steps, &entry->owned_eliminations, &entry->self_kills,
                            &entry->opponent_self_kills, &entry->opponent_kills, &hash,
                            entry->replay_path);
            if (parsed == 12) {
                entry->seed = (uint64_t)seed; entry->outcome = (TerminalReason)outcome;
                entry->terminal_hash = (uint64_t)hash; history->count++;
            }
        }
    }
    fclose(file);
    if (history->count) history->selected = history->count - 1;
    (void)write_json_index(history);
    return 1;
}

int match_history_add(MatchHistory* history, const Replay* replay,
                      TerminalReason outcome, int owned_eliminations,
                      int self_kills, int opponent_self_kills, int opponent_kills) {
    if (!history || !replay || replay->frame_count <= 0 || history->count >= MATCH_HISTORY_MAX) return 0;
    MatchHistoryEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.id = history->count ? history->entries[history->count - 1].id + 1 : 1;
    entry.seed = replay->seed;
    snprintf(entry.agent, sizeof(entry.agent), "%s", replay->agent_name);
    snprintf(entry.opponent, sizeof(entry.opponent), "%s", replay->opponent_name);
    entry.outcome = outcome;
    entry.steps = replay->frames[replay->frame_count - 1].step;
    entry.owned_eliminations = owned_eliminations;
    entry.self_kills = self_kills;
    entry.opponent_self_kills = opponent_self_kills;
    entry.opponent_kills = opponent_kills;
    entry.crate_density = replay->config.crate_density;
    entry.map_width = replay->config.width;
    entry.map_height = replay->config.height;
    entry.terminal_hash = replay->frames[replay->frame_count - 1].state_hash;
    snprintf(entry.replay_path, sizeof(entry.replay_path), "%s/replays/match-%06d.bin", history->directory, entry.id);
    if (!replay_save(replay, entry.replay_path)) return 0;

    char index_path[MATCH_HISTORY_PATH];
    snprintf(index_path, sizeof(index_path), "%s/index.csv", history->directory);
    FILE* file = fopen(index_path, history->count ? "a" : "w");
    if (!file) return 0;
    if (!history->count)
        fprintf(file, "id,seed,agent,opponent,outcome,steps,owned_eliminations,self_kills,opponent_self_kills,opponent_kills,terminal_hash,crate_density,map_width,map_height,replay_path\n");
    fprintf(file, "%d,%llu,%s,%s,%d,%d,%d,%d,%d,%d,%llu,%d,%d,%d,%s\n",
            entry.id, (unsigned long long)entry.seed, entry.agent, entry.opponent,
            (int)entry.outcome, entry.steps, entry.owned_eliminations, entry.self_kills,
            entry.opponent_self_kills, entry.opponent_kills,
            (unsigned long long)entry.terminal_hash, entry.crate_density, entry.map_width,
            entry.map_height, entry.replay_path);
    fclose(file);
    history->entries[history->count++] = entry;
    history->selected = history->count - 1;
    return write_json_index(history);
}

int match_history_load_replay(const MatchHistory* history, int index, Replay* replay) {
    if (!history || !replay || index < 0 || index >= history->count) return 0;
    return replay_load(replay, history->entries[index].replay_path);
}
