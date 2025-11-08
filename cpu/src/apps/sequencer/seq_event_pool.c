
#include "seq_event_pool.h"

// Initialize the pool
void SEQ_POOL_init(SeqEventPool *pool) {
    pool->free_list = &pool->pool[0];
    for (int i = 0; i < MAX_SEQ_EVENTS - 1; i++) {
        pool->pool[i].next = &pool->pool[i + 1];
        pool->pool[i].prev = NULL;
    }
    pool->pool[MAX_SEQ_EVENTS - 1].next = NULL;
    pool->pool[MAX_SEQ_EVENTS - 1].prev = NULL;
}

// Get a free event (pop)
SeqEvent *SEQ_POOL_get_event(SeqEventPool *pool) {
    if (!pool->free_list)
        return NULL; // No free events
    SeqEvent *e = pool->free_list;
    pool->free_list = e->next;
    e->next = NULL;
    e->prev = NULL;
    return e;
}

// Return an event to the pool (push)
void SEQ_POOL_release_event(SeqEventPool *pool, SeqEvent *e) {
    if (!e)
        return;
    e->next = pool->free_list;
    e->prev = NULL;
    pool->free_list = e;
}
