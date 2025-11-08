

#include "event_seq.h"
#include <stdlib.h>


// --- Initialization ---

void SEQ_init(Sequencer *seq, uint32_t loop_length_ticks) {
    seq->head = NULL;
    seq->current = NULL;
    seq->loop_length_ticks = loop_length_ticks;
    seq->current_tick = 0;
    seq->playing = false;
}

// --- Control ---

void SEQ_start(Sequencer *seq) {
    if (seq->on_beat_callback) {
        seq->on_beat_callback(0);
    }
    if (seq->on_start_callback) {
        seq->on_start_callback(0);
    }
    seq->playing = true;
    seq->current_tick = 0;
    seq->current = seq->head;
}

void SEQ_stop(Sequencer *seq) {
    seq->playing = false;
    if (seq->on_stop_callback) {
        seq->on_stop_callback(0);
    }
}

void SEQ_add_event(Sequencer *seq, SeqEvent *new_event) {
    if (!seq)
        return;

    new_event->timestamp_tick = seq->current_tick;
    // new_event->callback = callback;

    if (!seq->head) {
        // First event in sequence
        new_event->next = new_event;
        new_event->prev = new_event;
        seq->head = new_event;
        seq->current = new_event;
        return;
    }

    // Insert keeping chronological order
    SeqEvent *cur = seq->head;
    do {
        if (new_event->timestamp_tick < cur->timestamp_tick)
            break;
        cur = cur->next;
    } while (cur != seq->head);

    new_event->next = cur;
    new_event->prev = cur->prev;
    cur->prev->next = new_event;
    cur->prev = new_event;

    // Only update head if the new event has an earlier timestamp
    if (new_event->timestamp_tick < seq->head->timestamp_tick)
        seq->head = new_event;
}

void SEQ_add_event_at_timestamp(Sequencer *seq, uint32_t timestamp_tick,
                                SeqEvent *new_event) {

    new_event->timestamp_tick = timestamp_tick % seq->loop_length_ticks;
    // new_event->callback = callback;

    if (!seq->head) {
        // First event in the list
        seq->head = new_event;
        new_event->next = new_event->prev = new_event; // circular list
        seq->current = new_event;
        return;
    }

    SeqEvent *cur = seq->head;
    do {
        if (timestamp_tick < cur->timestamp_tick)
            break;
        cur = cur->next;
    } while (cur != seq->head);

    new_event->next = cur;
    new_event->prev = cur->prev;
    cur->prev->next = new_event;
    cur->prev = new_event;

    // Only update head if the new event has an earlier timestamp
    if (new_event->timestamp_tick < seq->head->timestamp_tick)
        seq->head = new_event;
}

void SEQ_tick(Sequencer *seq) {
    if (!seq->playing)
        return;
    if (!seq->head)
        return;

    SeqEvent *current_event = seq->current;

    // Loop while there are events whose timestamp matches current tick
    while (current_event &&
           current_event->timestamp_tick == seq->current_tick) {
        if (current_event->midi_event_callback) {

            current_event->midi_event_callback(
                current_event->midi_params.chan,
                current_event->midi_params.data1,
                current_event->midi_params.data2);
        }
        if (current_event->callback) {
            current_event->callback();
        }

        current_event = current_event->next;

        // Si llegamos al final del loop (lista circular)
        if (current_event == seq->head) {
            break;
        } else {
        }
    }

    // Actualizar el "current" al próximo evento
    seq->current = current_event;

    // call on_beat_callback if set and on beat
    int ppqn = MIDI_PPQN / 4;
    if (seq->on_beat_callback && (seq->current_tick % ppqn == 0)) {
        uint32_t beat_index = seq->current_tick / ppqn;
        seq->on_beat_callback(beat_index);
    }
    seq->current_tick++;
    if (seq->current_tick >= seq->loop_length_ticks) {
        seq->current_tick = 0;
        seq->current = seq->head;
    }
}

// --- Clear all events ---

void SEQ_clear(Sequencer *seq) {
    if (!seq->head)
        return;

    SeqEvent *cur = seq->head;
    do {
        SeqEvent *next = cur->next;
        free(cur);
        cur = next;
    } while (cur != seq->head);

    seq->head = NULL;
    seq->current = NULL;
    seq->current_tick = 0;
}

void SEQ_insert_before_current(Sequencer *seq, SeqEvent *new_event) {
    if (!seq->head) {
        // If list is empty, fallback to normal insert
        SEQ_add_event(seq, new_event);
        return;
    }

    new_event->timestamp_tick = seq->current_tick;
    // new_event->callback = callback;

    // Insert before current
    SeqEvent *current_event = seq->current ? seq->current : seq->head;
    new_event->next = current_event;
    new_event->prev = current_event->prev;
    current_event->prev->next = new_event;
    current_event->prev = new_event;

    // Only update head if the new event has an earlier timestamp
    if (new_event->timestamp_tick < seq->head->timestamp_tick)
        seq->head = new_event;
}

// Setter
void SEQ_set_beat_callback(Sequencer *seq,
                           void (*callback)(uint32_t beat_index)) {
    seq->on_beat_callback = callback;
}