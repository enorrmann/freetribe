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
 * @file    cursor.h
 *
 * @brief   Edit cursor and channel scroll position.
 *
 * The cursor is the single source of truth for "where the user is editing".
 * It is independent of rendering so the view can be rebuilt from scratch
 * every frame.
 */

#ifndef TRACKER_CURSOR_H
#define TRACKER_CURSOR_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

#include "tracker_types.h"

/*----- Macros -------------------------------------------------------*/

// Number of visible columns per channel in the tracker grid.
#define TRACKER_COLS_PER_CHANNEL 4 // COL_COUNT: NOTE + INS + VOL + FX
#define TRACKER_VISIBLE_ROWS 4     // Rows drawn in the grid (4 * 8 = 32 px).
#define TRACKER_VISIBLE_CHANNELS 2 // Channels drawn side by side.

/*----- Typedefs -----------------------------------------------------*/

/**
 * @brief   One of the editable fields on a row.
 */
typedef enum {
    COL_NOTE = 0, // Note (whole note value; octave comes from the cursor).
    COL_INS,      // Instrument.
    COL_VOL,      // Volume / velocity.
    COL_FX,       // Effect command.
    COL_COUNT
} e_column;

/**
 * @brief   Cursor position and viewport.
 */
typedef struct {
    uint8_t pattern; // Current pattern index.
    uint8_t row;     // Current row (0..7).
    uint8_t channel; // Focused channel (0..TRACKER_CHANNELS-1).
    uint8_t column;  // Focused column (e_column).

    uint8_t step;    // Edit step (rows advanced on note entry).
    uint8_t octave;  // Current octave for note entry.

    uint8_t chan_scroll; // Leftmost visible channel.
    uint8_t row_scroll;  // Topmost visible row.

    bool insert; // Insert mode (shift rows down on entry).
} t_cursor;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void cursor_init(t_cursor *cursor);

/**
 * @brief   Move the cursor by one row, wrapping within the pattern.
 *
 * @param[in]   rows    Number of rows in the current pattern.
 * @param[in]   delta   Signed row delta.
 */
void cursor_move_row(t_cursor *cursor, int8_t delta, uint8_t rows);

/**
 * @brief   Page the cursor up or down by one visible screen.
 */
void cursor_move_page(t_cursor *cursor, int8_t delta, uint8_t rows);

/**
 * @brief   Move horizontally to the next/previous field.
 */
void cursor_move_column(t_cursor *cursor, int8_t delta);

/**
 * @brief   Move to the next/previous channel.
 */
void cursor_move_channel(t_cursor *cursor, int8_t delta);

/**
 * @brief   Keep the cursor inside the visible window.
 */
void cursor_clamp_scroll(t_cursor *cursor, uint8_t rows);

/**
 * @brief   Advance the cursor after a note is entered, honouring step.
 */
void cursor_advance(t_cursor *cursor, uint8_t rows);

#ifdef __cplusplus
}
#endif
#endif /* TRACKER_CURSOR_H */

/*----- End of file --------------------------------------------------*/
