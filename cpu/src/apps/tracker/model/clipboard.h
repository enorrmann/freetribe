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
 * @file    clipboard.h
 *
 * @brief   Copy / cut / paste buffer for rectangular block selections.
 *
 * A selection is a rectangle in (channel, row) space.  Copying captures
 * the rectangle into a compact buffer; pasting writes the buffer back at
 * the cursor position, wrapping where needed.
 */

#ifndef TRACKER_CLIPBOARD_H
#define TRACKER_CLIPBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

#include "song.h"
#include "tracker_types.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/**
 * @brief   A rectangular selection in the pattern grid.
 */
typedef struct {
    uint8_t pattern;
    uint8_t chan_start;
    uint8_t chan_end; // Inclusive.
    uint8_t row_start;
    uint8_t row_end; // Inclusive.
    bool active;
} t_selection;

/**
 * @brief   The clipboard contents.
 */
typedef struct {
    t_cell cell[TRACKER_CHANNELS][TRACKER_ROWS];
    uint8_t chans;
    uint8_t rows;
    bool has_data;
} t_clipboard;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

/**
 * @brief   Bookmark the current cursor cell as a single-cell selection.
 */
void selection_set_point(t_selection *sel, uint8_t pattern, uint8_t channel,
                         uint8_t row);

/**
 * @brief   Extend the selection to a new anchor point.
 */
void selection_extend(t_selection *sel, uint8_t channel, uint8_t row);

/**
 * @brief   Normalise the selection so start <= end on both axes.
 */
void selection_normalise(t_selection *sel);

/**
 * @brief   Copy the selected block from the song into the clipboard.
 */
void clipboard_copy(t_clipboard *clip, const t_song *song,
                    const t_selection *sel);

/**
 * @brief   Cut the selected block: copy then clear the source cells.
 */
void clipboard_cut(t_clipboard *clip, t_song *song, const t_selection *sel);

/**
 * @brief   Paste the clipboard at the given pattern/channel/row.
 *
 * Data wraps modulo the pattern size so pastes never leave the pattern.
 */
void clipboard_paste(const t_clipboard *clip, t_song *song, uint8_t pattern,
                     uint8_t channel, uint8_t row);

#ifdef __cplusplus
}
#endif
#endif /* TRACKER_CLIPBOARD_H */

/*----- End of file --------------------------------------------------*/
