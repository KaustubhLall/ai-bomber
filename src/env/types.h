#ifndef BOMBER_TYPES_H
#define BOMBER_TYPES_H

typedef enum {
    ACTION_UP = 0,
    ACTION_DOWN,
    ACTION_LEFT,
    ACTION_RIGHT,
    ACTION_PLACE_BOMB,
    ACTION_WAIT,
    ACTION_COUNT
} Action;

typedef enum {
    TERMINAL_NONE = 0,
    TERMINAL_AGENT_DEAD,
    TERMINAL_ENEMY_DEAD,
    TERMINAL_WIN,
    TERMINAL_LOSS,
    TERMINAL_DRAW,
    TERMINAL_TIMEOUT
} TerminalReason;

typedef struct {
    float reward;
    int done;
    TerminalReason terminal_reason;
} StepResult;

#endif /* BOMBER_TYPES_H */
