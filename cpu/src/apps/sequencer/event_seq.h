
#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <stdint.h>

#include <stdbool.h>
#include "freetribe.h"

#define LINE_BREAK "\n"
#define MIDI_PPQN 24
#define MAX_STEPS 64
#define STEPS_PER_PAGE 16

typedef void (*t_midi_event_callback)(char chan, char data1, char data2);
typedef void (*t_transport_event_callback)(int beat_index);

// --- Data structures ---
typedef struct MidiEventParams {
    char chan; 
    char data1;
    char data2;
    bool note_on;
} MidiEventParams;

typedef struct SeqEvent
{
    uint32_t timestamp_tick; // Position within loop
    void (*callback)(void);  // Function to trigger
    t_midi_event_callback midi_event_callback;

    struct MidiEventParams midi_params; // MIDI event parameters
    struct SeqEvent *next;
    struct SeqEvent *prev;
    struct SeqEvent *peer_event; // note on/off pairs

} SeqEvent;

typedef struct
{
    SeqEvent *head;             // First event in time order
    SeqEvent *current;          // Next event to check
    uint32_t loop_length_ticks; // Duration of one loop in ticks
    uint32_t current_tick;      // Current tick position
    t_transport_event_callback on_step_callback;  
    t_transport_event_callback on_page_callback;   // after n steps, we call it a page
    t_transport_event_callback on_start_callback;
    t_transport_event_callback on_stop_callback;
    t_transport_event_callback  on_record_toggle_callback;
    t_transport_event_callback on_changed_callback;
    t_transport_event_callback on_clear_callback;
    
    bool playing;
    bool recording;
    uint32_t internal_resolution; 
    uint32_t step_resolution;
    uint8_t step_event_amount[MAX_STEPS];
} Sequencer;

// --- Public API ---

/**
 * Initialize a sequencer with the specified loop length in beats.
 */
void SEQ_init(Sequencer *seq, uint32_t beats);

/**
 * Start or stop playback.
 */
void SEQ_start(Sequencer *seq);
void SEQ_stop(Sequencer *seq);
void SEQ_record_toggle(Sequencer *seq);

/**
 * Advance the sequencer by one tick.
 * Should be called once per PPQN tick (e.g., on each MIDI clock tick).
 */
void SEQ_tick(Sequencer *seq);

/**
 * Insert a new event in the loop, maintaining time order.
 * The timestamp is in ticks relative to the loop start.
 */
void SEQ_add_event(Sequencer *seq, SeqEvent *evt);
void SEQ_add_event_at_timestamp(Sequencer *seq, uint32_t timestamp_tick, SeqEvent *new_event);

/**
 * Remove all events and reset state.
 */
void SEQ_clear(Sequencer *seq);
/**
 * Insert a new event just before the current event in the timeline.
 * This is used for real-time recording, where the new event belongs
 * between the previous tick and the next scheduled event.
 */
void SEQ_insert_before_current(Sequencer *seq, SeqEvent *evt);

void SEQ_set_step_callback(Sequencer *seq, void (*callback)(uint32_t beat_index)) ;
void SEQ_insert_note_off(Sequencer *seq, SeqEvent *new_event) ;
void SEQ_print(Sequencer *seq);
void SEQ_insert_at_step(Sequencer *seq, SeqEvent *new_event, int step);
#endif // SEQUENCER_H
