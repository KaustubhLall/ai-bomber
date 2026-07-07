#ifndef VIZ_SESSION_H
#define VIZ_SESSION_H

#include "env/env.h"
#include "env/bomber_observation.h"
#include "agents/agent.h"
#include "viz/dashboard.h"
#include "core/replay.h"
#include "core/match_history.h"

#define MAX_VIZ_AGENTS 8
#define MAX_VIZ_EPOCHS 2000

typedef enum {
    VIEW_ARENA = 0,
    VIEW_COMPARE,
    VIEW_GRAPHS,
    VIEW_DEBUG,
    VIEW_HISTORY
} ViewMode;

typedef struct {
    Agent agent;
    Agent opponent;
    int has_opponent_policy;
    BomberEnv env;
    BomberConfig config;
    char name[32];
    AgentType type;
    AgentType opponent_type;
    char opponent_name[32];

    /* Per-epoch metrics (lightweight, just numbers) */
    float epoch_rewards[MAX_VIZ_EPOCHS];
    float epoch_avg_rewards[MAX_VIZ_EPOCHS]; /* running average */
    int epoch_wins[MAX_VIZ_EPOCHS];
    int epoch_deaths[MAX_VIZ_EPOCHS];
    int epoch_lengths[MAX_VIZ_EPOCHS];
    int epoch_crates[MAX_VIZ_EPOCHS];
    int epoch_count;

    /* Cumulative stats */
    float total_reward;
    int total_wins;
    int total_deaths;
    int total_crates;

    /* Current epoch live state */
    float current_reward;
    int current_step;
    int episode_done;
    int epoch_recorded;
    TerminalReason outcome;
    Replay* replay;
    int history_recorded;

    /* Dashboard for this agent */
    DashboardState dashboard;
} AgentSession;

typedef struct {
    AgentSession sessions[MAX_VIZ_AGENTS];
    int session_count;
    int active_session;
    int max_epochs;
    int paused;
    int simulation_hz;
    int target_fps;
    int show_help;
    int step_once;
    int auto_advance_epoch;
    ViewMode view_mode;
    uint64_t base_seed;
    int show_danger;
    int show_obs;
    int show_local_obs;
    int show_legend;
    int show_observation_window;
    int show_grid;
    MatchHistory history;
    Replay* history_replay;
    int history_frame;
    int history_playing;
    AgentType matchup_blue;
    AgentType matchup_red;
    uint64_t matchup_seed;
    int matchup_map_preset; /* 0=open, 1=standard, 2=dense */
    int matchup_matches;
    int show_matchup;
} VizSession;

void viz_session_init(VizSession* vs, int max_epochs, uint64_t base_seed);
int viz_session_add_agent(VizSession* vs, AgentType type, const char* name,
                          AgentType opponent_type, const char* opponent_name,
                          int has_opponent_policy, const BomberConfig* config);
void viz_session_step(VizSession* vs);
void viz_session_reset_epoch(VizSession* vs, int session_idx);
void viz_session_reset_all(VizSession* vs);
void viz_session_switch_agent(VizSession* vs, int idx);
void viz_session_switch_view(VizSession* vs, ViewMode mode);

/* Get the active session's current state for rendering */
AgentSession* viz_session_active(VizSession* vs);
int viz_session_start_match(VizSession* vs, AgentType blue, AgentType red, uint64_t seed);
int viz_session_load_history(VizSession* vs, int index);
void viz_session_history_step(VizSession* vs, int delta);
void viz_session_shutdown(VizSession* vs);

#endif /* VIZ_SESSION_H */
