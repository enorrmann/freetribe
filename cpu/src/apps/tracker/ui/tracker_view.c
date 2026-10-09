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
 * @file    tracker_view.c
 *
 * @brief   Tracker screen renderer.
 */

/*----- Includes -----------------------------------------------------*/

#include <stdio.h>
#include <string.h>

#include "gui_task.h"
#include "theme.h"
#include "tracker_view.h"

/*----- Macros -------------------------------------------------------*/

#define HEADER1_LEN 21
#define HEADER2_LEN 21
#define STATUS_LEN 21

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

static char g_buf[40];

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _render_header(const t_view_state *view);
static void _render_grid(const t_view_state *view);
static void _render_status(const t_view_state *view);

static void _draw_channel_row(const t_view_state *view, uint8_t row,
                              uint8_t screen_row, uint8_t channel);
static void _draw_field(const t_view_state *view, uint8_t x, uint8_t y,
                        uint8_t channel, uint8_t row, uint8_t column,
                        const char *text);
static uint8_t _channel_x(uint8_t channel, uint8_t scroll);

/*----- Extern function implementations ------------------------------*/

void tracker_view_clear(void) { gui_clear(false); }

void tracker_view_render(const t_view_state *view) {

    if (view == NULL || view->song == NULL || view->cursor == NULL) {
        return;
    }

    gui_clear(false);

    _render_header(view);
    _render_grid(view);
    _render_status(view);
}

/*----- Static function implementations ------------------------------*/

static void _render_header(const t_view_state *view) {

    const t_cursor *cur = view->cursor;

    // Line 1: pattern / song slot / bpm (21 chars).
    snprintf(g_buf, sizeof(g_buf), "PTN:%02X SLT:%02X BPM:%03u", cur->pattern,
             view->slot, (unsigned)(view->song->bpm % 1000u));

    gui_print(0, TH_HEADER1_Y, g_buf);

    // Line 2: octave / step / play state.
    snprintf(g_buf, sizeof(g_buf), "OCT:%u STP:%u %s %s", (unsigned)cur->octave,
             (unsigned)cur->step, view->playing ? ">PLAY" : " STOP",
             view->recording ? "REC" : (view->insert ? "INS" : "OVR"));

    gui_print(0, TH_HEADER2_Y, g_buf);

    gui_draw_line(0, TH_SEP1_Y, GUI_WIDTH - 1, TH_SEP1_Y, true);
}

static void _render_grid(const t_view_state *view) {

    const t_cursor *cur = view->cursor;
    uint8_t i;

    for (i = 0; i < TRACKER_VISIBLE_ROWS; i++) {

        uint8_t row = (uint8_t)(cur->row_scroll + i);
        uint8_t y = (uint8_t)(TH_GRID_Y + (i * TH_ROW_H));
        uint8_t ch;

        if (row >= view->song->rows) {
            break;
        }

        // Row number gutter.
        snprintf(g_buf, 4, "%02X", row);
        gui_print(TH_ROW_NUM_X, y, g_buf);

        for (ch = 0; ch < TRACKER_VISIBLE_CHANNELS; ch++) {
            uint8_t channel = (uint8_t)(cur->chan_scroll + ch);
            if (channel >= TRACKER_CHANNELS) {
                break;
            }
            _draw_channel_row(view, row, y, channel);
        }
    }
}

static void _render_status(const t_view_state *view) {

    const t_cursor *cur = view->cursor;
    const t_cell *cell;

    gui_draw_line(0, TH_SEP2_Y, GUI_WIDTH - 1, TH_SEP2_Y, true);

    if (view->message != NULL && view->message[0] != '\0') {
        snprintf(g_buf, sizeof(g_buf), "%-21.21s", view->message);
        gui_print(0, TH_STATUS_Y, g_buf);
        return;
    }

    cell = song_cell((t_song *)view->song, cur->pattern, cur->channel, cur->row);

    snprintf(g_buf, sizeof(g_buf), "CH:%02X FX:%02X%02X GT:%02X",
             cur->channel, cell->fxx, cell->fxx_data, cell->gate);

    gui_print(0, TH_STATUS_Y, g_buf);
}

static uint8_t _channel_x(uint8_t channel, uint8_t scroll) {

    return (uint8_t)(TH_CHAN_X0 + ((channel - scroll) * TH_CHAN_STRIDE));
}

static void _draw_field(const t_view_state *view, uint8_t x, uint8_t y,
                        uint8_t channel, uint8_t row, uint8_t column,
                        const char *text) {

    const t_cursor *cur = view->cursor;
    bool focused;

    focused = (channel == cur->channel) && (row == cur->row) &&
              (cur->column == column);

    if (focused) {
        // Invert the field to show focus. Clamp the left edge so a field at
        // column 0 cannot underflow the unsigned coordinate.
        uint8_t x0 = (x > 0) ? (uint8_t)(x - 1) : 0;
        gui_fill(x0, y, (uint8_t)(x + ((uint8_t)strlen(text) * TH_CHAR_W)),
                 (uint8_t)(y + TH_CHAR_H - 1), true);
        gui_set_inverted(true);
        gui_print(x, y, text);
        gui_set_inverted(false);
    } else {
        gui_print(x, y, text);
    }
}

static void _draw_channel_row(const t_view_state *view, uint8_t row, uint8_t y,
                              uint8_t channel) {

    const t_cursor *cur = view->cursor;
    const t_cell *cell;
    uint8_t x = _channel_x(channel, cur->chan_scroll);
    char note[4];
    char ins[3];
    char vol[3];
    bool playing_row;

    cell = song_cell((t_song *)view->song, cur->pattern, channel, row);

    // Highlight the row currently being played.
    playing_row = view->playing && (view->play_pattern == cur->pattern) &&
                  (row == view->play_row);

    if (playing_row) {
        gui_fill(x, y, (uint8_t)(x + TH_CHAN_STRIDE - 4),
                 (uint8_t)(y + TH_ROW_H - 1), true);
        gui_set_inverted(true);
    }

    note_format_cell(note, cell->note);
    note[3] = '\0';

    if (cell->instrument == 0xff) {
        snprintf(ins, sizeof(ins), "..");
    } else {
        snprintf(ins, sizeof(ins), "%02X", cell->instrument);
    }

    if (cell->velocity == 0xff) {
        snprintf(vol, sizeof(vol), "..");
    } else {
        snprintf(vol, sizeof(vol), "%02X", cell->velocity);
    }

    if (playing_row) {
        gui_print(x + TH_NOTE_OFF, y, note);
        gui_print(x + TH_INS_OFF, y, ins);
        gui_print(x + TH_VOL_OFF, y, vol);
        gui_set_inverted(false);
        return;
    }

    _draw_field(view, x + TH_NOTE_OFF, y, channel, row, COL_NOTE, note);
    _draw_field(view, x + TH_INS_OFF, y, channel, row, COL_INS, ins);
    _draw_field(view, x + TH_VOL_OFF, y, channel, row, COL_VOL, vol);
}

/*----- End of file --------------------------------------------------*/
