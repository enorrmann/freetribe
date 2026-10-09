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
 * @file    editor.c
 *
 * @brief   Editing commands implementation.
 */

/*----- Includes -----------------------------------------------------*/

#include <string.h>

#include "editor.h"

/*----- Macros -------------------------------------------------------*/

#define NOTE_MIN 1 // Lowest note; 0 is NOTE_NONE (empty).
#define NOTE_MAX 127

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static t_cell *_current_cell(t_editor *editor);
static void _snapshot(t_editor *editor);
static void _shift_rows_down(t_pattern *pattern, uint8_t channel, uint8_t row,
                             uint8_t rows);
static void _shift_rows_up(t_pattern *pattern, uint8_t channel, uint8_t row,
                           uint8_t rows);
static void _row_clear(t_pattern *pattern, uint8_t channel, uint8_t row);
static bool _note_is_pitch(uint8_t note);

/*----- Extern function implementations ------------------------------*/

void editor_init(t_editor *editor, t_song *song, t_cursor *cursor,
                 t_clipboard *clipboard, t_undo_stack *undo,
                 t_selection *selection) {

    editor->song = song;
    editor->cursor = cursor;
    editor->clipboard = clipboard;
    editor->undo = undo;
    editor->selection = selection;

    editor->insert = false;
}

void editor_enter_note(t_editor *editor, uint8_t semitone) {

    t_cursor *cur = editor->cursor;
    t_cell *cell = _current_cell(editor);
    uint8_t note;
    uint8_t channel;

    if (cell == NULL) {
        return;
    }

    _snapshot(editor);

    // Combine the entered semitone with the current octave.
    note = (uint8_t)((cur->octave * 12) + semitone);

    if (note > NOTE_MAX) {
        note = NOTE_MAX;
    }

    channel = cur->channel;

    if (editor->insert) {
        _shift_rows_down(&editor->song->pattern[cur->pattern], channel,
                         cur->row, editor->song->rows);
    }

    cell->note = note;

    // Inherit last instrument/volume when the cell does not set them.
    if (cell->instrument == 0xff) {
        cell->instrument = editor->song->channel[channel].instrument;
    }

    if (cell->velocity == 0xff) {
        cell->velocity = editor->song->channel[channel].volume;
    }

    // Make the gate explicit: a fresh cell has gate 0, which the sequencer
    // treats as legato; record it so the stored cell matches what plays.
    if (cell->gate == 0) {
        cell->gate = editor->song->channel[channel].gate;
    }

    // Remember for the next entry on this channel.
    editor->song->channel[channel].note = note;
    editor->song->channel[channel].instrument = cell->instrument;
    editor->song->channel[channel].volume = cell->velocity;
    editor->song->channel[channel].gate = cell->gate;

    // Insert a note-off one row below (classic tracker behaviour).
    if (editor->insert && (cur->row + 1) < editor->song->rows) {
        t_cell *next =
            song_cell(editor->song, cur->pattern, channel, cur->row + 1);
        if (next->note == NOTE_NONE) {
            next->note = NOTE_OFF;
        }
    }

    cursor_advance(cur, editor->song->rows);
}

void editor_enter_note_off(t_editor *editor) {

    t_cell *cell = _current_cell(editor);

    if (cell == NULL) {
        return;
    }

    _snapshot(editor);
    cell->note = NOTE_OFF;
    cell->instrument = 0xff;
    cell->velocity = 0xff;
    cell->gate = 0xff;

    cursor_advance(editor->cursor, editor->song->rows);
}

void editor_enter_note_cut(t_editor *editor) {

    t_cell *cell = _current_cell(editor);

    if (cell == NULL) {
        return;
    }

    _snapshot(editor);
    cell->note = NOTE_CUT;
    cell->instrument = 0xff;
    cell->velocity = 0xff;
    cell->gate = 0xff;

    cursor_advance(editor->cursor, editor->song->rows);
}

void editor_adjust(t_editor *editor, int8_t delta) {

    t_cursor *cur = editor->cursor;
    t_cell *cell = _current_cell(editor);
    t_channel_state *state;

    if (cell == NULL) {
        return;
    }

    _snapshot(editor);

    state = &editor->song->channel[cur->channel];

    switch (cur->column) {

    case COL_NOTE: {
        int16_t note;

        if (cell->note == NOTE_NONE || cell->note == NOTE_OFF ||
            cell->note == NOTE_CUT) {
            // Start from the current octave on the first adjustment.
            note = (int16_t)cur->octave * 12;
        } else {
            note = (int16_t)cell->note;
        }

        note += delta;

        // Keep at least note 1: note 0 would be NOTE_NONE, i.e. an empty
        // cell, which is not what "decrement" should produce.
        if (note < 1) {
            note = 1;
        } else if (note > NOTE_MAX) {
            note = NOTE_MAX;
        }

        cell->note = (uint8_t)note;
        state->note = cell->note;
        break;
    }

    case COL_INS: {
        int16_t ins = (cell->instrument == 0xff) ? 0 : cell->instrument;

        ins += delta;

        if (ins < 0) {
            ins = 0;
        } else if (ins > 0x7f) {
            ins = 0x7f;
        }

        cell->instrument = (uint8_t)ins;
        state->instrument = cell->instrument;
        break;
    }

    case COL_VOL: {
        int16_t vol = (cell->velocity == 0xff) ? state->volume : cell->velocity;

        vol += delta;

        if (vol < 0) {
            vol = 0;
        } else if (vol > 0x7f) {
            vol = 0x7f;
        }

        cell->velocity = (uint8_t)vol;
        state->volume = cell->velocity;
        break;
    }

    case COL_FX: {
        uint8_t fx = (uint8_t)((int16_t)cell->fxx + delta);
        cell->fxx = fx;
        break;
    }

    default:
        break;
    }
}

void editor_clear_cell(t_editor *editor) {

    t_cell *cell = _current_cell(editor);

    if (cell == NULL) {
        return;
    }

    _snapshot(editor);
    memset(cell, 0, sizeof(t_cell));
    cell->instrument = 0xff;
    cell->velocity = 0xff;
    cell->gate = 0xff;
}

void editor_clear_channel(t_editor *editor) {

    t_cursor *cur = editor->cursor;
    uint8_t row;

    _snapshot(editor);

    for (row = 0; row < editor->song->rows; row++) {
        t_cell *cell =
            song_cell(editor->song, cur->pattern, cur->channel, row);
        memset(cell, 0, sizeof(t_cell));
        cell->instrument = 0xff;
        cell->velocity = 0xff;
        cell->gate = 0xff;
    }
}

void editor_fill_column(t_editor *editor, uint8_t value) {

    t_cursor *cur = editor->cursor;
    uint8_t row;

    _snapshot(editor);

    for (row = 0; row < editor->song->rows; row++) {

        t_cell *cell =
            song_cell(editor->song, cur->pattern, cur->channel, row);

        switch (cur->column) {

        case COL_INS:
            cell->instrument = value;
            break;

        case COL_VOL:
            cell->velocity = value;
            break;

        case COL_FX:
            cell->fxx = value;
            break;

        default:
            break;
        }
    }
}

void editor_delete_row(t_editor *editor) {

    t_cursor *cur = editor->cursor;
    uint8_t rows = editor->song->rows;

    // Guard against an empty pattern (rows == 0).
    if (rows == 0) {
        return;
    }

    _snapshot(editor);

    _shift_rows_up(&editor->song->pattern[cur->pattern], cur->channel, cur->row,
                   rows);
}

void editor_insert_row(t_editor *editor) {

    t_cursor *cur = editor->cursor;
    uint8_t rows = editor->song->rows;

    if (rows == 0) {
        return;
    }

    _snapshot(editor);

    _shift_rows_down(&editor->song->pattern[cur->pattern], cur->channel,
                     cur->row, rows);
}

bool editor_undo(t_editor *editor) { return undo_pop(editor->undo, editor->song); }

void editor_copy(t_editor *editor) {

    clipboard_copy(editor->clipboard, editor->song, editor->selection);
}

void editor_cut(t_editor *editor) {

    _snapshot(editor);
    clipboard_cut(editor->clipboard, editor->song, editor->selection);
}

void editor_paste(t_editor *editor) {

    t_cursor *cur = editor->cursor;

    _snapshot(editor);
    clipboard_paste(editor->clipboard, editor->song, cur->pattern, cur->channel,
                    cur->row);
}

void editor_transpose(t_editor *editor, int8_t semitones) {

    t_cursor *cur = editor->cursor;
    t_selection sel = *editor->selection;
    uint8_t ch, row;

    selection_normalise(&sel);
    _snapshot(editor);

    for (ch = sel.chan_start; ch <= sel.chan_end; ch++) {
        for (row = sel.row_start; row <= sel.row_end; row++) {

            t_cell *cell = song_cell(editor->song, cur->pattern, ch, row);
            int16_t note;

            if (!_note_is_pitch(cell->note)) {
                continue;
            }

            note = (int16_t)cell->note + semitones;

            if (note < NOTE_MIN) {
                note = NOTE_MIN;
            } else if (note > NOTE_MAX) {
                note = NOTE_MAX;
            }

            cell->note = (uint8_t)note;
        }
    }
}

/*----- Static function implementations ------------------------------*/

static t_cell *_current_cell(t_editor *editor) {

    t_cursor *cur = editor->cursor;
    return song_cell(editor->song, cur->pattern, cur->channel, cur->row);
}

static void _snapshot(t_editor *editor) {

    undo_push(editor->undo, editor->song, editor->cursor->pattern);
}

static bool _note_is_pitch(uint8_t note) {

    return note != NOTE_NONE && note != NOTE_OFF && note != NOTE_CUT;
}

static void _row_clear(t_pattern *pattern, uint8_t channel, uint8_t row) {

    memset(&pattern->cell[channel][row], 0, sizeof(t_cell));
    pattern->cell[channel][row].instrument = 0xff;
    pattern->cell[channel][row].velocity = 0xff;
    pattern->cell[channel][row].gate = 0xff;
}

static void _shift_rows_down(t_pattern *pattern, uint8_t channel, uint8_t row,
                             uint8_t rows) {

    uint8_t r;

    // Move rows [row .. rows-2] down one, then blank the open row.
    if (rows == 0 || row >= rows) {
        return;
    }

    for (r = (uint8_t)(rows - 1); r > row; r--) {
        pattern->cell[channel][r] = pattern->cell[channel][r - 1];
    }

    _row_clear(pattern, channel, row);
}

static void _shift_rows_up(t_pattern *pattern, uint8_t channel, uint8_t row,
                           uint8_t rows) {

    uint8_t r;

    if (rows == 0 || row >= rows) {
        return;
    }

    for (r = row; r < rows - 1; r++) {
        pattern->cell[channel][r] = pattern->cell[channel][r + 1];
    }

    _row_clear(pattern, channel, (uint8_t)(rows - 1));
}

/*----- End of file --------------------------------------------------*/
