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
 * @file    note_router.h
 *
 * @brief   Turns sequencer note events into MIDI messages.
 *
 * The router is the only place that talks MIDI.  It tracks which note is
 * sounding on each channel so that a new note or an explicit note-off
 * releases the previous one, and so that gates that are shorter than a
 * row can be scheduled for release.
 */

#ifndef TRACKER_NOTE_ROUTER_H
#define TRACKER_NOTE_ROUTER_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

#include "tracker_types.h"

/*----- Macros -------------------------------------------------------*/

// MIDI channel offset: tracker channel 0 -> MIDI channel 1.
#define MIDI_CHANNEL_BASE 0

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
    uint8_t note;      // Currently sounding note (NOTE_NONE if idle).
    uint8_t velocity;  // Velocity of the sounding note.
    uint8_t gate_remaining; // Rows remaining before auto release.
    bool active;
} t_router_channel;

typedef struct {
    t_router_channel channel[TRACKER_CHANNELS];
} t_note_router;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void router_init(t_note_router *router);

/**
 * @brief   Start a note on a channel.
 *
 * Releases any note already sounding on that channel first.
 *
 * @param[in]   channel     0..TRACKER_CHANNELS-1.
 * @param[in]   note        MIDI note.
 * @param[in]   velocity    MIDI velocity.
 * @param[in]   gate_rows   Rows the note should last; 0xff = legato.
 */
void router_note_on(t_note_router *router, uint8_t channel, uint8_t note,
                    uint8_t velocity, uint8_t gate_rows);

/**
 * @brief   Release the note on a channel (note off).
 */
void router_note_off(t_note_router *router, uint8_t channel);

/**
 * @brief   Cut the note on a channel (fast note off, velocity 0).
 */
void router_note_cut(t_note_router *router, uint8_t channel);

/**
 * @brief   Count down gates by one row and release expired notes.
 */
void router_advance_row(t_note_router *router);

/**
 * @brief   Release every sounding note (e.g. on stop).
 */
void router_all_notes_off(t_note_router *router);

#ifdef __cplusplus
}
#endif
#endif /* TRACKER_NOTE_ROUTER_H */

/*----- End of file --------------------------------------------------*/
