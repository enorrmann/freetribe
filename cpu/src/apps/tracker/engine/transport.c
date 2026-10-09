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
 * @file    transport.c
 *
 * @brief   Playback transport implementation.
 */

/*----- Includes -----------------------------------------------------*/

#include "transport.h"

/*----- Macros -------------------------------------------------------*/

#define MIN_BPM 20
#define MAX_BPM 400

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

void transport_init(t_transport *transport, uint16_t bpm) {

    transport->state = TRANSPORT_STOPPED;
    transport->order_index = 0;
    transport->row = 0;
    transport->tick_accum = 0;
    transport->rows = SONG_DEFAULT_ROWS;
    transport->wrapped = false;

    transport_set_bpm(transport, bpm);
}

void transport_set_bpm(t_transport *transport, uint16_t bpm) {

    uint32_t rows_per_minute;

    if (bpm < MIN_BPM) {
        bpm = MIN_BPM;
    } else if (bpm > MAX_BPM) {
        bpm = MAX_BPM;
    }

    // Milliseconds per row = 60000 / (bpm * rows_per_beat).
    rows_per_minute = (uint32_t)bpm * TRANSPORT_ROWS_PER_BEAT;
    transport->tick_period = 60000u / rows_per_minute;

    if (transport->tick_period == 0) {
        transport->tick_period = 1;
    }
}

void transport_play(t_transport *transport, uint8_t rows) {

    transport->rows = rows ? rows : 1;
    transport->state = TRANSPORT_PLAYING;
    transport->tick_accum = 0;
}

void transport_stop(t_transport *transport) {

    transport->state = TRANSPORT_STOPPED;
    transport->row = 0;
    transport->order_index = 0;
    transport->tick_accum = 0;
}

void transport_pause(t_transport *transport) {

    if (transport->state == TRANSPORT_PLAYING) {
        transport->state = TRANSPORT_PAUSED;
    } else {
        transport->state = TRANSPORT_PLAYING;
        transport->tick_accum = 0;
    }
}

bool transport_tick(t_transport *transport, uint32_t ms) {

    if (transport->state != TRANSPORT_PLAYING) {
        return false;
    }

    transport->tick_accum += ms;

    if (transport->tick_accum >= transport->tick_period) {
        // Keep the remainder so tempo stays accurate over time.
        transport->tick_accum -= transport->tick_period;
        return true;
    }

    return false;
}

bool transport_next_row(t_transport *transport, uint8_t order_length,
                        const uint8_t *order) {

    bool wrapped = false;

    transport->row++;

    if (transport->row >= transport->rows) {

        transport->row = 0;
        transport->order_index++;

        if (transport->order_index >= order_length) {
            transport->order_index = 0;
            wrapped = true;
        }
    }

    (void)order; // Pattern look-up is done by the sequencer.
    transport->wrapped = wrapped;
    return wrapped;
}

void transport_set_position(t_transport *transport, uint8_t order_index,
                            uint8_t row) {

    transport->order_index = order_index;
    transport->row = row;
    transport->tick_accum = 0;
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
