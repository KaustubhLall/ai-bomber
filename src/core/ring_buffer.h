#ifndef BOMBER_RING_BUFFER_H
#define BOMBER_RING_BUFFER_H

#include <string.h>

#define RING_BUFFER_CAPACITY 256

typedef struct {
    int data[RING_BUFFER_CAPACITY];
    int head;
    int count;
} IntRingBuffer;

static inline void rb_init(IntRingBuffer* rb) {
    rb->head = 0;
    rb->count = 0;
}

static inline void rb_push(IntRingBuffer* rb, int val) {
    rb->data[rb->head] = val;
    rb->head = (rb->head + 1) % RING_BUFFER_CAPACITY;
    if (rb->count < RING_BUFFER_CAPACITY) rb->count++;
}

static inline int rb_get(const IntRingBuffer* rb, int index_from_oldest) {
    if (index_from_oldest < 0 || index_from_oldest >= rb->count) return -1;
    int start = (rb->head - rb->count + RING_BUFFER_CAPACITY) % RING_BUFFER_CAPACITY;
    return rb->data[(start + index_from_oldest) % RING_BUFFER_CAPACITY];
}

static inline int rb_size(const IntRingBuffer* rb) {
    return rb->count;
}

#endif /* BOMBER_RING_BUFFER_H */
