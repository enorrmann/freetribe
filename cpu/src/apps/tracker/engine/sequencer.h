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
 * @file    sequencer.h
 *
 * @brief   Event-based sequencer.
 *
 * The sequencer walks the song row by row on the transport clock.  Each
 * row is decoded into a stream of events (note on / off / cut / effect)
 * which are delivered to a listener.  Splitting decoding from rendering
 * means the UI can display exactly what the engine is doing without the
 * engine knowing anything about the display.
 */

#ifndef TRACKER_SEQUENCER_H
#define TRACKER_SEQUENCER_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

#include "note_router.h"
#include "song.h"
#include "tracker_types.h"
#include "transport.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

typedef enum {
    SEQ_EVENT_ROW,      // A new row became active.
    SEQ_EVENT_NOTE_ON,  // A note started.
    SEQ_EVENT_NOTE_OFF, // A note was released.
    SEQ_EVENT_NOTE_CUT, // A note was cut.
    SEQ_EVENT_STOP,     // Playback stopped.
    SEQ_EVENT_COUNT
} e_seq_event_type;

typedef struct {
    e_seq_event_type type;
    uint8_t order_index; // Position in the song order list.
    uint8_t pattern;     // Pattern at that order position.
    uint8_t row;         // Row within the pattern.
    uint8_t channel;     // Channel that produced the event.
    uint8_t value;       // Note or effect value, event dependent.
} t_seq_event;

typedef void (*t_seq_listener)(const t_seq_event *event);

typedef struct {
    t_song *song;
    t_transport transport;
    t_note_router router;
    t_seq_listener listener;

    // Row currently being played (mirrors transport, cached for the UI).
    uint8_t playing_order;
    uint8_t playing_pattern;
    uint8_t playing_row;
    bool playing;
} t_sequencer;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void sequencer_init(t_sequencer *seq, t_song *song, uint16_t bpm);

/**
 * @brief   Register the event listener.
 */
void sequencer_set_listener(t_sequencer *seq, t_seq_listener listener);

/**
 * @brief   Feed the sequencer a millisecond tick.
 *
 * When a row is due the sequencer decodes it and emits events.
 *
 * @param[in]   ms  Milliseconds elapsed since the last call.
 */
void sequencer_tick(t_sequencer *seq, uint32_t ms);

void sequencer_play(t_sequencer *seq);
void sequencer_stop(t_sequencer *seq);
void sequencer_toggle_play(t_sequencer *seq);

/**
 * @brief   Preview a single cell immediately (audition while editing).
 *
 * @param[in]   pattern Pattern index.
 * @param[in]   channel Channel index.
 * @param[in]   row     Row index.
 */
void sequencer_preview(t_sequencer *seq, uint8_t pattern, uint8_t channel,
                       uint8_t row);

/**
 * @brief   True while the transport is running.
 */
bool sequencer_is_playing(const t_sequencer *seq);

/**
 * @brief   Current playing row (for the playhead indicator) or 0 when idle.
 */
uint8_t sequencer_playing_row(const t_sequencer *seq);

/**
 * @brief   Current playing order index (for the playhead indicator).
 */
uint8_t sequencer_playing_order(const t_sequencer *seq);

#ifdef __cplusplus
}
#endif
#endif /* TRACKER_SEQUENCER_H */

/*----- End of file --------------------------------------------------*/
