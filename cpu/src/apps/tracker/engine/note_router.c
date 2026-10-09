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
 * @file    note_router.c
 *
 * @brief   MIDI note router implementation.
 */

/*----- Includes -----------------------------------------------------*/

#include <string.h>

#include "freetribe.h"

#include "note_router.h"

/*----- Macros -------------------------------------------------------*/

#define DEFAULT_VELOCITY 100
#define GATE_LEGATO 0xff
#define GATE_RELEASE_VEL 0x40

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static uint8_t _midi_channel(uint8_t channel);

/*----- Extern function implementations ------------------------------*/

void router_init(t_note_router *router) {

    memset(router, 0, sizeof(t_note_router));
}

void router_note_on(t_note_router *router, uint8_t channel, uint8_t note,
                    uint8_t velocity, uint8_t gate_rows) {

    t_router_channel *ch;

    channel %= TRACKER_CHANNELS;
    ch = &router->channel[channel];

    // Release anything already sounding on this channel.
    if (ch->active) {
        ft_send_note_off(_midi_channel(channel), ch->note, GATE_RELEASE_VEL);
    }

    if (velocity == 0 || velocity == 0xff) {
        velocity = DEFAULT_VELOCITY;
    }

    ft_send_note_on(_midi_channel(channel), note, velocity);

    ch->note = note;
    ch->velocity = velocity;
    ch->active = true;

    // Legato means "sustain until replaced or stopped".
    ch->gate_remaining = (gate_rows == GATE_LEGATO) ? GATE_LEGATO : gate_rows;
}

void router_note_off(t_note_router *router, uint8_t channel) {

    t_router_channel *ch;

    channel %= TRACKER_CHANNELS;
    ch = &router->channel[channel];

    if (ch->active) {
        ft_send_note_off(_midi_channel(channel), ch->note, GATE_RELEASE_VEL);
        ch->active = false;
        ch->note = NOTE_NONE;
    }
}

void router_note_cut(t_note_router *router, uint8_t channel) {

    t_router_channel *ch;

    channel %= TRACKER_CHANNELS;
    ch = &router->channel[channel];

    if (ch->active) {
        // A cut is a note-off with zero velocity for a hard stop.
        ft_send_note_off(_midi_channel(channel), ch->note, 0);
        ch->active = false;
        ch->note = NOTE_NONE;
    }
}

void router_advance_row(t_note_router *router) {

    uint8_t i;

    for (i = 0; i < TRACKER_CHANNELS; i++) {

        t_router_channel *ch = &router->channel[i];

        if (!ch->active || ch->gate_remaining == GATE_LEGATO) {
            continue;
        }

        if (ch->gate_remaining > 0) {
            ch->gate_remaining--;
        }

        if (ch->gate_remaining == 0) {
            router_note_off(router, i);
        }
    }
}

void router_all_notes_off(t_note_router *router) {

    uint8_t i;

    for (i = 0; i < TRACKER_CHANNELS; i++) {
        router_note_off(router, i);
    }
}

/*----- Static function implementations ------------------------------*/

static uint8_t _midi_channel(uint8_t channel) {

    return (uint8_t)(MIDI_CHANNEL_BASE + channel);
}

/*----- End of file --------------------------------------------------*/
