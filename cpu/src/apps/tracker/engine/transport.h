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
 * @file    transport.h
 *
 * @brief   Playback transport: play / stop / pause and position.
 *
 * The transport owns *time*.  It is fed a millisecond tick from the engine
 * and converts the song BPM into row advances.  It knows nothing about
 * notes or MIDI: it simply reports "a new row is due" and where we are.
 */

#ifndef TRACKER_TRANSPORT_H
#define TRACKER_TRANSPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

#include "song.h"

/*----- Macros -------------------------------------------------------*/

// Sixteenth note grid, four rows per beat.
#define TRANSPORT_ROWS_PER_BEAT 4

/*----- Typedefs -----------------------------------------------------*/

typedef enum {
    TRANSPORT_STOPPED = 0,
    TRANSPORT_PLAYING,
    TRANSPORT_PAUSED,
} e_transport_state;

typedef struct {
    e_transport_state state;

    uint8_t order_index; // Position in the song order list.
    uint8_t row;         // Current row in the current pattern.

    // Accumulated milliseconds towards the next row.
    uint32_t tick_accum;
    uint32_t tick_period; // Milliseconds per row, derived from BPM.

    uint8_t rows;   // Rows in the current pattern.
    bool wrapped;   // Set when the current pattern/order just wrapped.
} t_transport;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void transport_init(t_transport *transport, uint16_t bpm);

/**
 * @brief   Recompute tick_period from a BPM value.
 */
void transport_set_bpm(t_transport *transport, uint16_t bpm);

/**
 * @brief   Start playback from the current position.
 */
void transport_play(t_transport *transport, uint8_t rows);

/**
 * @brief   Stop playback and reset the position.
 */
void transport_stop(t_transport *transport);

/**
 * @brief   Toggle between playing and paused.
 */
void transport_pause(t_transport *transport);

/**
 * @brief   Advance the clock by `ms` milliseconds.
 *
 * @return  True when a new row is due (caller should process it), false
 *          otherwise.  At most one row is emitted per call.
 */
bool transport_tick(t_transport *transport, uint32_t ms);

/**
 * @brief   Move to the next row, wrapping the pattern and the order list.
 *
 * @param[in]   order_length    Number of entries in the song order list.
 * @return  True when playback reached the end and wrapped to the start.
 */
bool transport_next_row(t_transport *transport, uint8_t order_length,
                        const uint8_t *order);

/**
 * @brief   Jump the playhead to an order position.
 */
void transport_set_position(t_transport *transport, uint8_t order_index,
                            uint8_t row);

#ifdef __cplusplus
}
#endif
#endif /* TRACKER_TRANSPORT_H */

/*----- End of file --------------------------------------------------*/
