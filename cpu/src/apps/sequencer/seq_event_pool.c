#include "seq_event_pool.h"

// Initialize the pool safely
void SEQ_POOL_init(SeqEventPool *pool) {
    if (!pool)
        return;

    pool->free_list = &pool->pool[0];

    for (int i = 0; i < MAX_SEQ_EVENTS; i++) {
        pool->pool[i].timestamp_tick = 0;
        pool->pool[i].callback = NULL;
        pool->pool[i].midi_event_callback = NULL;
        pool->pool[i].next = (i < MAX_SEQ_EVENTS - 1) ? &pool->pool[i + 1] : NULL;
        pool->pool[i].prev = NULL;
    }
}

// Get a free event (pop)
SeqEvent *SEQ_POOL_get_event(SeqEventPool *pool) {
    if (!pool || !pool->free_list)
        return NULL; // Pool empty or uninitialized

    SeqEvent *e = pool->free_list;
    pool->free_list = e->next;
    e->next = NULL;
    e->prev = NULL;

    return e;
}

// Return an event to the pool (push)
void SEQ_POOL_release_event(SeqEventPool *pool, SeqEvent *e) {
    if (!pool || !e)
        return;

    // Optional: simple guard to avoid double free
    if (e->next || e->prev) {
        // Already in a list — ignore or handle as error
        return;
    }

    e->callback = NULL;
    e->midi_event_callback = NULL;

    e->next = pool->free_list;
    e->prev = NULL;
    pool->free_list = e;
}
