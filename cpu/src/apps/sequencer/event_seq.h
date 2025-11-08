
#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <stdint.h>
#include <stdbool.h>

#define MIDI_PPQN 24

typedef void (*t_midi_event_callback)(char chan, char data1, char data2);
typedef void (*t_transport_event_callback)(int beat_index);

// --- Data structures ---
typedef struct MidiEventParams {
    char chan; 
    char data1;
    char data2;
} MidiEventParams;

typedef struct SeqEvent
{
    uint32_t timestamp_tick; // Position within loop
    void (*callback)(void);  // Function to trigger
    //void (*callback)(int , int,int );
    t_midi_event_callback midi_event_callback;

    struct MidiEventParams midi_params; // MIDI event parameters
    struct SeqEvent *next;
    struct SeqEvent *prev;
} SeqEvent;

typedef struct
{
    SeqEvent *head;             // First event in time order
    SeqEvent *current;          // Next event to check
    uint32_t loop_length_ticks; // Duration of one loop in ticks
    uint32_t current_tick;      // Current tick position
    t_transport_event_callback on_beat_callback;  
    t_transport_event_callback on_start_callback;
    t_transport_event_callback on_stop_callback;
    bool playing;
} Sequencer;

// --- Public API ---

/**
 * Initialize a sequencer with the specified loop length in ticks.
 */
void SEQ_init(Sequencer *seq, uint32_t loop_length_ticks);

/**
 * Start or stop playback.
 */
void SEQ_start(Sequencer *seq);
void SEQ_stop(Sequencer *seq);

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

void SEQ_set_beat_callback(Sequencer *seq, void (*callback)(uint32_t beat_index)) ;
#endif // SEQUENCER_H
