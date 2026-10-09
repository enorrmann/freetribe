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
 * @file    song.h
 *
 * @brief   Song data model: patterns, order list and global settings.
 *
 * The song owns all pattern data.  It exposes accessors rather than a raw
 * struct pointer where possible so that invariants (row bounds, pattern
 * bounds, per-channel state) live in one place.
 */

#ifndef TRACKER_SONG_H
#define TRACKER_SONG_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdint.h>

#include "tracker_types.h"

/*----- Macros -------------------------------------------------------*/

#define SONG_DEFAULT_BPM 120
#define SONG_DEFAULT_ROWS 16 // Default pattern length (rows).
#define SONG_DEFAULT_STEP 1  // Default edit step.

/*----- Typedefs -----------------------------------------------------*/

/**
 * @brief   Per-channel runtime state kept by the song.
 *
 * This mirrors classic trackers: each channel remembers the last
 * instrument, volume and the note currently sounding so that empty fields
 * inherit sensible values.
 */
typedef struct {
    uint8_t instrument; // Last instrument used on this channel.
    uint8_t volume;     // Last volume/velocity (0..127).
    uint8_t note;       // Last note played (for note-off tracking).
    uint8_t gate;       // Last gate value.
} t_channel_state;

/**
 * @brief   The complete song.
 */
typedef struct {
    t_pattern pattern[TRACKER_PATTERNS];

    // Order list: sequence of pattern indices to play.
    uint8_t order[TRACKER_SONG_LENGTH];
    uint8_t order_length;

    uint16_t bpm;   // Beats per minute.
    uint8_t rows;   // Rows per pattern (1..TRACKER_ROWS).
    uint8_t step;   // Cursor advance after note entry.

    t_channel_state channel[TRACKER_CHANNELS];
} t_song;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

/**
 * @brief   Initialise a song to a sane default (empty patterns, order 0).
 */
void song_init(t_song *song);

/**
 * @brief   Clear all patterns, the order list and channel state.
 */
void song_clear(t_song *song);

/**
 * @brief   Clear one pattern.
 */
void pattern_clear(t_pattern *pattern);

/**
 * @brief   Access a cell, clamping pattern and row indices.
 */
t_cell *song_cell(t_song *song, uint8_t pattern, uint8_t channel, uint8_t row);

/**
 * @brief   Set one cell.
 */
void song_set_cell(t_song *song, uint8_t pattern, uint8_t channel, uint8_t row,
                   const t_cell *cell);

/**
 * @brief   Copy one pattern to another.
 */
void song_copy_pattern(t_song *song, uint8_t dest, uint8_t src);

/**
 * @brief   Clear one pattern by index.
 */
void song_clear_pattern(t_song *song, uint8_t pattern);

/**
 * @brief   Insert a new order entry at `index`, shifting the rest.
 */
void song_order_insert(t_song *song, uint8_t index, uint8_t pattern);

/**
 * @brief   Delete the order entry at `index`.
 */
void song_order_delete(t_song *song, uint8_t index);

#ifdef __cplusplus
}
#endif
#endif /* TRACKER_SONG_H */

/*----- End of file --------------------------------------------------*/
