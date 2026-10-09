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
 * @file    cursor.c
 *
 * @brief   Edit cursor implementation.
 */

/*----- Includes -----------------------------------------------------*/

#include "cursor.h"

/*----- Macros -------------------------------------------------------*/

// Columns visible at once given the tracker column layout.
#define VISIBLE_CHANNELS TRACKER_VISIBLE_CHANNELS

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

void cursor_init(t_cursor *cursor) {

    cursor->pattern = 0;
    cursor->row = 0;
    cursor->channel = 0;
    cursor->column = COL_NOTE;

    cursor->step = 1;
    cursor->octave = 4;

    cursor->chan_scroll = 0;
    cursor->row_scroll = 0;

    cursor->insert = false;
}

void cursor_move_row(t_cursor *cursor, int8_t delta, uint8_t rows) {

    int16_t row;

    if (rows == 0) {
        rows = 1;
    }

    row = (int16_t)cursor->row + delta;

    while (row < 0) {
        row += rows;
    }

    cursor->row = (uint8_t)(row % rows);
}

void cursor_move_page(t_cursor *cursor, int8_t delta, uint8_t rows) {

    cursor_move_row(cursor, delta * TRACKER_VISIBLE_ROWS, rows);
}

void cursor_move_column(t_cursor *cursor, int8_t delta) {

    int16_t col = (int16_t)cursor->column + delta;

    if (col < 0) {
        col = 0;
    } else if (col >= COL_COUNT) {
        col = COL_COUNT - 1;
    }

    cursor->column = (uint8_t)col;
}

void cursor_move_channel(t_cursor *cursor, int8_t delta) {

    int16_t chan = (int16_t)cursor->channel + delta;

    if (chan < 0) {
        chan = 0;
    } else if (chan >= TRACKER_CHANNELS) {
        chan = TRACKER_CHANNELS - 1;
    }

    cursor->channel = (uint8_t)chan;
}

void cursor_clamp_scroll(t_cursor *cursor, uint8_t rows) {

    // Horizontal: keep the focused channel inside the window.
    if (cursor->channel < cursor->chan_scroll) {
        cursor->chan_scroll = cursor->channel;
    }

    if (cursor->channel >= cursor->chan_scroll + VISIBLE_CHANNELS) {
        cursor->chan_scroll = cursor->channel - (VISIBLE_CHANNELS - 1);
    }

    // Vertical: keep the focused row inside the window.
    if (cursor->row < cursor->row_scroll) {
        cursor->row_scroll = cursor->row;
    }

    if (cursor->row >= cursor->row_scroll + TRACKER_VISIBLE_ROWS) {
        cursor->row_scroll = cursor->row - (TRACKER_VISIBLE_ROWS - 1);
    }

    // Do not scroll past the end of a short pattern.
    if (rows > TRACKER_VISIBLE_ROWS &&
        cursor->row_scroll > rows - TRACKER_VISIBLE_ROWS) {
        cursor->row_scroll = rows - TRACKER_VISIBLE_ROWS;
    }
}

void cursor_advance(t_cursor *cursor, uint8_t rows) {

    // An edit step of 0 keeps the cursor on the same row.
    if (cursor->step == 0) {
        return;
    }

    cursor_move_row(cursor, cursor->step, rows);
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
