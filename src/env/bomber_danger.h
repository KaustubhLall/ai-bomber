#ifndef BOMBER_DANGER_H
#define BOMBER_DANGER_H

#include "env/bomber_state.h"
#include "env/types.h"
#include "core/ring_buffer.h"

typedef struct {
    int current_blast[MAX_HEIGHT][MAX_WIDTH];
    int time_to_blast[MAX_HEIGHT][MAX_WIDTH]; /* -1 = no danger, 0 = exploding now */
    int safe_now[MAX_HEIGHT][MAX_WIDTH];
    int reachable_safe[MAX_HEIGHT][MAX_WIDTH];
    int action_safe[ACTION_COUNT];
} DangerMap;

void danger_compute(DangerMap* dm, const BomberState* state);
void danger_compute_escape(DangerMap* dm, const BomberState* state, int agent_id);
int danger_is_tile_safe(const DangerMap* dm, int x, int y, int ticks_ahead);
int danger_detect_dead_end(const BomberState* state, int x, int y);
int danger_would_trap_agent(const BomberState* state, int agent_id, int bomb_x, int bomb_y);

#endif /* BOMBER_DANGER_H */
