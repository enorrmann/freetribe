/*----------------------------------------------------------------------

                     This file is part of Freetribe

                https://github.com/bangcorrupt/freetribe

                                License

                   GNU AFFERO GENERAL PUBLIC LICENSE
                      Version 3, 19 November 2007

                           AGPL-3.0-or-later

 Freetribe is free software: you can redistribute it and/or modify it
under the terms of the GNU Affero General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
                  (at your option) any later version.

     Freetribe is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty
        of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
          See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see <https://www.gnu.org/licenses/>.

                       Copyright bangcorrupt 2024

----------------------------------------------------------------------*/

/**
 * @file    sequencer.c
 *
 * @brief   Event-based sequencer implementation.
 */

/*----- Includes -----------------------------------------------------*/

#include <string.h>

#include "sequencer.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _emit(t_sequencer *seq, e_seq_event_type type, uint8_t channel,
                  uint8_t value);
static void _process_row(t_sequencer *seq);
static void _process_channel(t_sequencer *seq, uint8_t channel,
                             const t_cell *cell);
static uint8_t _pattern_at(t_sequencer *seq, uint8_t order_index);

/*----- Extern function implementations ------------------------------*/

void sequencer_init(t_sequencer *seq, t_song *song, uint16_t bpm) {

    memset(seq, 0, sizeof(t_sequencer));

    seq->song = song;
    seq->listener = NULL;
    seq->playing = false;

    transport_init(&seq->transport, bpm);
    router_init(&seq->router);
}

void sequencer_set_listener(t_sequencer *seq, t_seq_listener listener) {

    seq->listener = listener;
}

void sequencer_tick(t_sequencer *seq, uint32_t ms) {

    if (!transport_tick(&seq->transport, ms)) {
        return;
    }

    // Release notes whose gate expired on the previous row before starting
    // the new row, so a gate of 1 row lasts exactly one row.
    router_advance_row(&seq->router);

    _process_row(seq);

    transport_next_row(&seq->transport, seq->song->order_length,
                       seq->song->order);
}

void sequencer_play(t_sequencer *seq) {

    uint8_t rows = seq->song->rows;

    seq->playing = true;
    transport_play(&seq->transport, rows);
}

void sequencer_stop(t_sequencer *seq) {

    seq->playing = false;
    router_all_notes_off(&seq->router);
    transport_stop(&seq->transport);

    _emit(seq, SEQ_EVENT_STOP, 0, 0);
}

void sequencer_toggle_play(t_sequencer *seq) {

    if (seq->playing) {
        sequencer_stop(seq);
    } else {
        sequencer_play(seq);
    }
}

void sequencer_preview(t_sequencer *seq, uint8_t pattern, uint8_t channel,
                       uint8_t row) {

    t_cell *cell = song_cell(seq->song, pattern, channel, row);

    if (!note_is_pitch(cell->note)) {
        return;
    }

    router_note_on(&seq->router, channel, cell->note,
                   cell->velocity ? cell->velocity : 100, 0xff);
}

bool sequencer_is_playing(const t_sequencer *seq) { return seq->playing; }

uint8_t sequencer_playing_row(const t_sequencer *seq) {

    return seq->playing_row;
}

uint8_t sequencer_playing_order(const t_sequencer *seq) {

    return seq->playing_order;
}

/*----- Static function implementations ------------------------------*/

static void _emit(t_sequencer *seq, e_seq_event_type type, uint8_t channel,
                  uint8_t value) {

    t_seq_event event;

    if (seq->listener == NULL) {
        return;
    }

    event.type = type;
    event.order_index = seq->playing_order;
    event.pattern = seq->playing_pattern;
    event.row = seq->playing_row;
    event.channel = channel;
    event.value = value;

    seq->listener(&event);
}

static uint8_t _pattern_at(t_sequencer *seq, uint8_t order_index) {

    if (order_index >= seq->song->order_length) {
        order_index = 0;
    }

    return seq->song->order[order_index] % TRACKER_PATTERNS;
}

static void _process_row(t_sequencer *seq) {

    uint8_t ch;

    seq->playing_order = seq->transport.order_index;
    seq->playing_pattern = _pattern_at(seq, seq->playing_order);
    seq->playing_row = seq->transport.row;

    _emit(seq, SEQ_EVENT_ROW, 0, seq->playing_row);

    for (ch = 0; ch < TRACKER_CHANNELS; ch++) {

        t_cell *cell = song_cell(seq->song, seq->playing_pattern, ch,
                                 seq->playing_row);

        _process_channel(seq, ch, cell);
    }
}

static void _process_channel(t_sequencer *seq, uint8_t channel,
                             const t_cell *cell) {

    t_channel_state *state = &seq->song->channel[channel];

    if (cell->note == NOTE_OFF) {
        router_note_off(&seq->router, channel);
        _emit(seq, SEQ_EVENT_NOTE_OFF, channel, 0);
        return;
    }

    if (cell->note == NOTE_CUT) {
        router_note_cut(&seq->router, channel);
        _emit(seq, SEQ_EVENT_NOTE_CUT, channel, 0);
        return;
    }

    if (!note_is_pitch(cell->note)) {
        return;
    }

    // Inherit instrument/volume from the channel when not specified.
    if (cell->instrument != 0xff) {
        state->instrument = cell->instrument;
    }

    if (cell->velocity != 0xff && cell->velocity != 0) {
        state->volume = cell->velocity;
    }

    state->note = cell->note;
    state->gate = cell->gate;

    // Treat gate 0 and 0xff alike as "legato": a fresh cell (gate 0) should
    // sustain just like a cleared cell (gate 0xff).
    uint8_t gate = (cell->gate == 0) ? 0xff : cell->gate;

    router_note_on(&seq->router, channel, cell->note, state->volume, gate);
    _emit(seq, SEQ_EVENT_NOTE_ON, channel, cell->note);
}

/*----- End of file --------------------------------------------------*/
