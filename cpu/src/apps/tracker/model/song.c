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
 * @file    song.c
 *
 * @brief   Song data model implementation.
 */

/*----- Includes -----------------------------------------------------*/

#include <string.h>

#include "song.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

void pattern_clear(t_pattern *pattern) {
    memset(pattern, 0, sizeof(t_pattern));
}

void song_clear(t_song *song) {

    uint8_t i;

    for (i = 0; i < TRACKER_PATTERNS; i++) {
        pattern_clear(&song->pattern[i]);
    }

    memset(song->order, 0, sizeof(song->order));
    song->order_length = 1;
    memset(song->channel, 0, sizeof(song->channel));

    for (i = 0; i < TRACKER_CHANNELS; i++) {
        song->channel[i].volume = 100;
        song->channel[i].gate = 0xff;
    }
}

void song_init(t_song *song) {

    memset(song, 0, sizeof(t_song));

    song->bpm = SONG_DEFAULT_BPM;
    song->rows = SONG_DEFAULT_ROWS;
    song->step = SONG_DEFAULT_STEP;

    song_clear(song);
}

t_cell *song_cell(t_song *song, uint8_t pattern, uint8_t channel, uint8_t row) {

    pattern %= TRACKER_PATTERNS;
    channel %= TRACKER_CHANNELS;
    row %= TRACKER_ROWS;

    return &song->pattern[pattern].cell[channel][row];
}

void song_set_cell(t_song *song, uint8_t pattern, uint8_t channel, uint8_t row,
                   const t_cell *cell) {

    *song_cell(song, pattern, channel, row) = *cell;
}

void song_copy_pattern(t_song *song, uint8_t dest, uint8_t src) {

    dest %= TRACKER_PATTERNS;
    src %= TRACKER_PATTERNS;

    if (dest == src) {
        return;
    }

    song->pattern[dest] = song->pattern[src];
}

void song_clear_pattern(t_song *song, uint8_t pattern) {

    pattern_clear(&song->pattern[pattern % TRACKER_PATTERNS]);
}

void song_order_insert(t_song *song, uint8_t index, uint8_t pattern) {

    uint8_t i;

    if (song->order_length >= TRACKER_SONG_LENGTH) {
        return;
    }

    if (index > song->order_length) {
        index = song->order_length;
    }

    for (i = song->order_length; i > index; i--) {
        song->order[i] = song->order[i - 1];
    }

    song->order[index] = pattern % TRACKER_PATTERNS;
    song->order_length++;
}

void song_order_delete(t_song *song, uint8_t index) {

    uint8_t i;

    if (index >= song->order_length || song->order_length <= 1) {
        return;
    }

    for (i = index; i < song->order_length - 1; i++) {
        song->order[i] = song->order[i + 1];
    }

    song->order_length--;
    song->order[song->order_length] = 0;
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
