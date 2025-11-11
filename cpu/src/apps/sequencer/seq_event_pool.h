
#ifndef SEQ_EVENT_POOL_H
#define SEQ_EVENT_POOL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "event_seq.h"



// --- Static pool of free events ---
#define MAX_SEQ_EVENTS 256

typedef struct {
    SeqEvent pool[MAX_SEQ_EVENTS];
    SeqEvent *free_list;
} SeqEventPool;

// --- Public API ---
void SEQ_POOL_init(SeqEventPool *pool);
SeqEvent *SEQ_POOL_get_event(SeqEventPool *pool);
void SEQ_POOL_release_event(SeqEventPool *pool, SeqEvent *e);

#endif // SEQ_EVENT_POOL_H
