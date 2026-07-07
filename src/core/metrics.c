#include "core/metrics.h"
#include <stdio.h>
#include <string.h>
#include "env/bomber_state.h"

void metrics_init(Metrics* m) {
    memset(m, 0, sizeof(Metrics));
}

void metrics_update(Metrics* m, Action action, StepResult result,
                    int crates_destroyed, int powerups_collected) {
    m->total_steps++;
    m->total_reward += result.reward;
    m->crates_destroyed += crates_destroyed;
    m->powerups_collected += powerups_collected;

    if (action >= 0 && action < 6) {
        m->action_counts[action]++;
        if (action == ACTION_PLACE_BOMB) m->bombs_placed++;
    }

    if (result.done) {
        m->episodes++;
        switch (result.terminal_reason) {
            case TERMINAL_WIN:           m->wins++; break;
            case TERMINAL_LOSS:          m->losses++; break;
            case TERMINAL_DRAW:          m->draws++; break;
            case TERMINAL_TIMEOUT:       m->timeouts++; break;
            case TERMINAL_AGENT_DEAD:    m->deaths++; break;
            default: break;
        }
    }
}

void metrics_record_elimination_causes(Metrics* m, const BomberState* state) {
    if (!m || !state) return;
    if (!state->agents[0].alive) {
        if (state->death_owner[0] == 0) m->self_kills++;
        else if (state->death_owner[0] > 0) m->opponent_kills++;
    }
    for (int a = 1; a < state->agent_count; a++) if (!state->agents[a].alive) {
        if (state->death_owner[a] == 0) m->enemies_killed++;
        else if (state->death_owner[a] == a) m->opponent_self_kills++;
    }
}

void metrics_print(const Metrics* m) {
    printf("=== Metrics ===\n");
    printf("Episodes:          %d\n", m->episodes);
    printf("Total steps:       %d\n", m->total_steps);
    printf("Total reward:      %.2f\n", m->total_reward);
    printf("Avg reward/ep:     %.4f\n", m->episodes > 0 ? m->total_reward / m->episodes : 0.0f);
    printf("Wins:              %d\n", m->wins);
    printf("Losses:            %d\n", m->losses);
    printf("Draws:             %d\n", m->draws);
    printf("Timeouts:          %d\n", m->timeouts);
    printf("Deaths:            %d\n", m->deaths);
    printf("Crates destroyed:  %d\n", m->crates_destroyed);
    printf("Powerups collected:%d\n", m->powerups_collected);
    printf("Owned eliminations: %d\n", m->enemies_killed);
    printf("Own self-kills:     %d\n", m->self_kills);
    printf("Opponent self-kills:%d\n", m->opponent_self_kills);
    printf("Opponent kills:     %d\n", m->opponent_kills);

    const char* action_names[] = {"UP", "DOWN", "LEFT", "RIGHT", "BOMB", "WAIT"};
    printf("Action distribution:\n");
    for (int i = 0; i < 6; i++) {
        printf("  %-6s: %d (%.1f%%)\n", action_names[i], m->action_counts[i],
               m->total_steps > 0 ? 100.0f * (float)m->action_counts[i] / m->total_steps : 0.0f);
    }

    if (m->total_time_ms > 0) {
        double eps_per_sec = (double)m->episodes / (m->total_time_ms / 1000.0);
        double steps_per_sec = (double)m->total_steps / (m->total_time_ms / 1000.0);
        printf("Episodes/sec:      %.1f\n", eps_per_sec);
        printf("Steps/sec:         %.1f\n", steps_per_sec);
        printf("Avg episode len:   %.1f\n", m->episodes > 0 ? (float)m->total_steps / m->episodes : 0.0f);
    }
}

void metrics_print_compact(const Metrics* m) {
    double avg_reward = m->episodes > 0 ? m->total_reward / m->episodes : 0.0f;
    double death_rate = m->episodes > 0 ? (double)m->deaths / m->episodes : 0.0f;
    double win_rate = m->episodes > 0 ? (double)m->wins / m->episodes : 0.0f;
    double timeout_rate = m->episodes > 0 ? (double)m->timeouts / m->episodes : 0.0f;
    double steps_per_sec = m->total_time_ms > 0 ? (double)m->total_steps / (m->total_time_ms / 1000.0) : 0.0f;

    printf("eps=%d steps=%d avg_r=%.4f win=%.2f death=%.2f timeout=%.2f sps=%.0f\n",
           m->episodes, m->total_steps, avg_reward, win_rate, death_rate,
           timeout_rate, steps_per_sec);
}
