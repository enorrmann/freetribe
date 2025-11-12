

#include "event_seq.h"
#include <stdlib.h>

static SeqEvent *_SEQ_find_matching_note_on(SeqEvent *evt, uint8_t chan,
                                            uint8_t note);
void _changed(Sequencer *seq, SeqEvent *new_event);

static char *int_to_char(int32_t value) {
    // buffer estático para almacenar el resultado (-2147483648 = 11 chars +
    // '\0')
    static char str_buf[12];

    // conversión usando itoa, base 10 (decimal)
    itoa(value, str_buf, 10);

    // devolver puntero al buffer resultante
    return str_buf;
}

static inline uint32_t SEQ_quantize_tick(Sequencer *seq, uint32_t tick) {
    //return tick;

    // uint32_t quant_ticks = seq->internal_resolution / 2; // default 1/8
    uint32_t quant_ticks = seq->internal_resolution / 4; // default 1/16
     //uint32_t quant_ticks = seq->internal_resolution / 8; // default 1/32

    // Redondear al múltiplo más cercano
    uint32_t lower = (tick / quant_ticks) * quant_ticks;
    uint32_t upper = lower + quant_ticks;
    uint32_t quant_tick = 0;
    if ((tick - lower) >= (quant_ticks / 2)) {
        quant_tick = upper;

    } else {
        quant_tick = lower;
    }
    return quant_tick;
}

// --- Initialization ---

void SEQ_init(Sequencer *seq, uint32_t beats) {

    seq->head = NULL;
    seq->current = NULL;
    seq->current_tick = 0;
    seq->playing = false;
    seq->recording = false;
    seq->internal_resolution = MIDI_PPQN ;
    seq->step_resolution = seq->internal_resolution / 4; // 6 en este caso
    seq->loop_length_ticks = seq->internal_resolution * beats;
    int i;
    for (i = 0; i < MAX_STEPS; i++) {
        seq->step_event_amount[i] = 0;
    }
}

// --- Control ---

void SEQ_start(Sequencer *seq) {
    if (seq->on_step_callback) {
        seq->on_step_callback(0);
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

void SEQ_record_toggle(Sequencer *seq) {
    seq->recording = !seq->recording;
    if (seq->on_record_toggle_callback) {
        seq->on_record_toggle_callback(seq->recording);
    }
}

void _changed(Sequencer *seq, SeqEvent *new_event) {
    if (!new_event->midi_params.note_on) return; // only save note on as registered steps
    uint32_t step_index = new_event->timestamp_tick / seq->step_resolution;
    seq->step_event_amount[step_index]++;
    if (seq->on_changed_callback) {
        on_changed_callback(0);
    }
}

void SEQ_add_event(Sequencer *seq, SeqEvent *new_event) {

    if (!seq || !seq->recording) {
        return;
    }

    new_event->timestamp_tick = seq->current_tick;

    _changed(seq, new_event);

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
    if (!seq || !seq->recording) {
        return;
    }

    new_event->timestamp_tick = timestamp_tick % seq->loop_length_ticks;
    _changed(seq,new_event);
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

    // if (!seq->head) return; run even with no events to advance ticks and call
    // callbacks

    SeqEvent *current_event = seq->current;

    // Loop while there are events whose timestamp matches current tick
    while (current_event && current_event->timestamp_tick == seq->current_tick) {
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
        }
    }

    // Actualizar el "current" al próximo evento
    seq->current = current_event;

    // call on_step_callback if set and on beat

    if (seq->on_step_callback &&
        (seq->current_tick % seq->step_resolution == 0)) {

        uint32_t step_index = seq->current_tick / seq->step_resolution;
        /*ft_print("On Step callback index");
        ft_print(int_to_char(step_index));
        ft_print("\n");*/

        seq->on_step_callback(step_index);
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
        // free(cur); dont
        cur = next;
    } while (cur != seq->head);

    seq->head = NULL;
    seq->current = NULL;
    seq->current_tick = 0;

    int i;
    for (i = 0; i < MAX_STEPS; i++) {
        seq->step_event_amount[i] = 0;
    }

    if (seq->on_changed_callback) {
        seq->on_changed_callback(0);
    }
}

void SEQ_insert_note_off(Sequencer *seq, SeqEvent *new_event) {
    uint8_t stopped_record_gate = 0;
    if (!seq->playing) {
        stopped_record_gate = 6;
    }

    // adjust timing for NOTE_OFF events
    if (new_event->midi_params.note_on == false) {
        new_event->timestamp_tick = seq->current_tick;

        // SeqEvent *prev =_SEQ_find_matching_note_on(seq->head,
        // new_event->midi_params.chan, new_event->midi_params.data1);
        SeqEvent *prev_note_on = _SEQ_find_matching_note_on(
            seq->current, new_event->midi_params.chan,
            new_event->midi_params.data1); // test try from current

        if (prev_note_on) {
            new_event->peer_event = prev_note_on;
            prev_note_on->peer_event = new_event;
            uint32_t gate = new_event->timestamp_tick - prev_note_on->timestamp_tick; // original gate time without quantisation
            if (gate==0){
                gate++;
            }
            
            new_event->timestamp_tick = prev_note_on->timestamp_tick + gate + stopped_record_gate;

            
            SEQ_add_event_at_timestamp(seq, new_event->timestamp_tick,new_event);
        } else {
            ft_print("note not found prev"); // bug hunt
        }
    }
}

/*void SEQ_insert_before_current(Sequencer *seq, SeqEvent *new_event) {
    if (!seq || !seq->recording)
        return;

    if (!seq->head) {
        SEQ_add_event(seq, new_event);
        return;
    }

    // Quantize if needed
    new_event->timestamp_tick = seq->current_tick;
    if (new_event->midi_params.note_on)
        new_event->quantised_timestamp_tick =
            SEQ_quantize_tick(seq, new_event->timestamp_tick);

    SeqEvent *current_event = seq->current ? seq->current : seq->head;
    SeqEvent *insert_pos = current_event;

    // --- Find correct insertion point chronologically ---
    // Move backwards until we find an event with <= timestamp
    while (insert_pos->prev != seq->head->prev &&
           insert_pos->prev->timestamp_tick > new_event->timestamp_tick) {
        insert_pos = insert_pos->prev;
    }

    // --- Insert new_event before insert_pos ---
    new_event->next = insert_pos;
    new_event->prev = insert_pos->prev;
    insert_pos->prev->next = new_event;
    insert_pos->prev = new_event;

    // --- Update head if necessary ---
    if (new_event->timestamp_tick < seq->head->timestamp_tick)
        seq->head = new_event;
}
*/
// Setter
void SEQ_set_step_callback(Sequencer *seq,
                           void (*callback)(uint32_t beat_index)) {
    seq->on_step_callback = callback;
}

static SeqEvent *_SEQ_find_matching_note_on(SeqEvent *evt, uint8_t chan,
                                            uint8_t note) {
    if (!evt || !evt->prev) {

        return NULL;
    }

    SeqEvent *start = evt;
    if (start->midi_params.chan == chan && start->midi_params.data1 == note &&
        start->midi_params.note_on && start->peer_event == NULL) {
        return start; // Found valid NOTE_ON
    }

    SeqEvent *search = evt->prev;

    if (start->prev == start) {
        return start;
    }

    while (search != start) { // caso especial no la encuentra en el primero

        if (search->midi_params.chan == chan &&
            search->midi_params.data1 == note && search->midi_params.note_on &&
            search->peer_event == NULL) {
            return search; // Found valid NOTE_ON
        }

        search = search->prev;
    }

    return NULL; // Not found after full loop
}

void evt_print(SeqEvent *evt) {
    ft_print(int_to_char(evt->timestamp_tick));
    ft_print(",");
    if (evt->midi_params.note_on) {
        ft_print("ON ");
    } else {
        ft_print("OFF ");
    }
    ft_print(",");
    ft_print(int_to_char(evt->midi_params.data1));
    ft_print(",");
}

void SEQ_print(Sequencer *seq) {

    if (!seq || !seq->head) {
        ft_print("empty list \n");
        return;
    }
    SeqEvent *start = seq->head;
    SeqEvent *cur = seq->head;

    ft_print(
        "TICK,TYPE,NOTE, PEER_TICK, PEER_TYPE,PEER_NOTE \n");
    do {
        evt_print(cur);
        evt_print(cur->peer_event);
        ft_print("\n");

        cur = cur->next;
    } while (cur && cur != start);
}

void SEQ_insert_at_step(Sequencer *seq, SeqEvent *new_event, int step) {
    bool prev_recording_state = seq->recording;
    // set it to recording for sequencer mode insertions
    if (!seq->recording){
        seq->recording = true;
    }
    SEQ_add_event_at_timestamp(seq, step * seq->step_resolution, new_event);
    seq->recording = prev_recording_state;
}