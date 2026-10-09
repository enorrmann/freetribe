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
      but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
          See the GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see <https://www.gnu.org/licenses/>.

                       Copyright bangcorrupt 2024

----------------------------------------------------------------------*/

/**
 * @file    clipboard.c
 *
 * @brief   Copy / cut / paste implementation.
 */

/*----- Includes -----------------------------------------------------*/

#include <string.h>

#include "clipboard.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

void selection_set_point(t_selection *sel, uint8_t pattern, uint8_t channel,
                         uint8_t row) {

    sel->pattern = pattern % TRACKER_PATTERNS;
    sel->chan_start = channel % TRACKER_CHANNELS;
    sel->chan_end = sel->chan_start;
    sel->row_start = row % TRACKER_ROWS;
    sel->row_end = sel->row_start;
    sel->active = true;
}

void selection_extend(t_selection *sel, uint8_t channel, uint8_t row) {

    sel->chan_end = channel % TRACKER_CHANNELS;
    sel->row_end = row % TRACKER_ROWS;
    sel->active = true;
}

void selection_normalise(t_selection *sel) {

    uint8_t tmp;

    if (sel->chan_start > sel->chan_end) {
        tmp = sel->chan_start;
        sel->chan_start = sel->chan_end;
        sel->chan_end = tmp;
    }

    if (sel->row_start > sel->row_end) {
        tmp = sel->row_start;
        sel->row_start = sel->row_end;
        sel->row_end = tmp;
    }
}

void clipboard_copy(t_clipboard *clip, const t_song *song,
                    const t_selection *sel) {

    uint8_t ch, row;

    t_selection src = *sel;
    selection_normalise(&src);

    clip->chans = src.chan_end - src.chan_start + 1;
    clip->rows = src.row_end - src.row_start + 1;

    memset(clip->cell, 0, sizeof(clip->cell));

    for (ch = 0; ch < clip->chans; ch++) {
        for (row = 0; row < clip->rows; row++) {
            clip->cell[ch][row] =
                *song_cell((t_song *)song, src.pattern, src.chan_start + ch,
                           src.row_start + row);
        }
    }

    clip->has_data = true;
}

void clipboard_cut(t_clipboard *clip, t_song *song, const t_selection *sel) {

    uint8_t ch, row;
    t_cell empty = {0};
    t_selection src = *sel;

    // Canonical empty cell: inherit sentinels, matching editor_clear_cell.
    empty.instrument = 0xff;
    empty.velocity = 0xff;
    empty.gate = 0xff;

    selection_normalise(&src);

    clipboard_copy(clip, song, &src);

    for (ch = 0; ch < clip->chans; ch++) {
        for (row = 0; row < clip->rows; row++) {
            song_set_cell(song, src.pattern, src.chan_start + ch,
                          src.row_start + row, &empty);
        }
    }
}

void clipboard_paste(const t_clipboard *clip, t_song *song, uint8_t pattern,
                     uint8_t channel, uint8_t row) {

    uint8_t ch, r;

    if (!clip->has_data) {
        return;
    }

    for (ch = 0; ch < clip->chans; ch++) {
        for (r = 0; r < clip->rows; r++) {
            // Wrapping handled by song_set_cell modulo arithmetic.
            song_set_cell(song, pattern, channel + ch, row + r,
                          &clip->cell[ch][r]);
        }
    }
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
