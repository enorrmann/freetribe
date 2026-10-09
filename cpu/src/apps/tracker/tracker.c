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
 * @file    tracker.c
 *
 * @brief   Tracker application entry point.
 *
 * This file wires the model, the sequencer engine and the view together
 * and translates panel input into editing commands.  It holds the single
 * instance of the app state; everything else is a library of pure-ish
 * modules.
 */

/*----- Includes -----------------------------------------------------*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freetribe.h"

#include "clipboard.h"
#include "cursor.h"
#include "editor.h"
#include "gui_task.h"
#include "led_view.h"
#include "panel_buttons.h"
#include "sequencer.h"
#include "song.h"
#include "storage.h"
#include "tracker_view.h"
#include "transport.h"
#include "undo.h"

/*----- Macros -------------------------------------------------------*/

#define TICK_DIV 0 // User tick every systick (~1 ms).
#define MESSAGE_TICKS 1200 // Status message lifetime (~1.2 s).

// Bar buttons select patterns 0..3.
#define BAR_PATTERN_COUNT 4

/*----- Typedefs -----------------------------------------------------*/

/**
 * @brief   The complete application state.
 */
typedef struct {
    t_song song;
    t_cursor cursor;
    t_clipboard clipboard;
    t_undo_stack undo;
    t_selection selection;
    t_editor editor;
    t_sequencer sequencer;
    t_view_state view;

    bool shift;
    bool record;
    uint8_t slot;

    char message[24];
    uint16_t message_ticks;

    bool needs_redraw;
    uint8_t last_play_row;
} t_tracker;

/*----- Static variable definitions ----------------------------------*/

static t_tracker g_tracker;

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _tick_callback(void);
static void _button_callback(uint8_t index, bool state);
static void _encoder_callback(uint8_t index, int8_t value);
static void _trigger_callback(uint8_t pad, uint8_t vel, bool state);

static void _seq_event(const t_seq_event *event);

static void _draw(void);
static void _render_view(void);

static void _set_message(const char *text);
static void _select_pattern(uint8_t pattern);
static void _cycle_pattern(int8_t delta);
static void _change_octave(int8_t delta);
static void _change_step(int8_t delta);
static void _change_bpm(int16_t delta);
static void _handle_transport(uint8_t button);
static void _handle_navigation(uint8_t button);
static void _handle_edit(uint8_t button);
static void _save_slot(void);
static void _load_slot(void);
static void _trigger_note(uint8_t pad);

/*----- Extern function implementations ------------------------------*/

/**
 * @brief   Initialise the tracker application.
 */
t_status app_init(void) {

    t_status status = ERROR;

    memset(&g_tracker, 0, sizeof(g_tracker));

    song_init(&g_tracker.song);
    cursor_init(&g_tracker.cursor);
    undo_init(&g_tracker.undo);
    editor_init(&g_tracker.editor, &g_tracker.song, &g_tracker.cursor,
                &g_tracker.clipboard, &g_tracker.undo, &g_tracker.selection);

    sequencer_init(&g_tracker.sequencer, &g_tracker.song, g_tracker.song.bpm);
    sequencer_set_listener(&g_tracker.sequencer, _seq_event);

    storage_init();

    g_tracker.cursor.step = g_tracker.song.step;
    g_tracker.slot = 0;
    g_tracker.record = true;
    g_tracker.needs_redraw = true;
    g_tracker.last_play_row = 0xff;

    ft_register_panel_callback(BUTTON_EVENT, _button_callback);
    ft_register_panel_callback(ENCODER_EVENT, _encoder_callback);
    ft_register_panel_callback(TRIGGER_EVENT, _trigger_callback);
    ft_register_tick_callback(TICK_DIV, _tick_callback);

    gui_task();
    tracker_view_clear();

    led_view_update_transport(false, false);
    led_view_show_step(g_tracker.cursor.step);
    led_view_update_channels(&g_tracker.song, g_tracker.cursor.pattern);

    _set_message("TRACKER READY");
    _draw();

    ft_print("Tracker");

    status = SUCCESS;
    return status;
}

/**
 * @brief   Run the tracker application.
 */
void app_run(void) {

    gui_task();

    if (g_tracker.needs_redraw) {
        _draw();
    }
}

/*----- Static function implementations ------------------------------*/

static void _tick_callback(void) {

    // Drive the sequencer with one millisecond of time.
    sequencer_tick(&g_tracker.sequencer, 1);

    // Expire transient messages.
    if (g_tracker.message_ticks > 0) {
        g_tracker.message_ticks--;
        if (g_tracker.message_ticks == 0) {
            g_tracker.message[0] = '\0';
            g_tracker.needs_redraw = true;
        }
    }
}

static void _seq_event(const t_seq_event *event) {

    if (event == NULL) {
        return;
    }

    switch (event->type) {

    case SEQ_EVENT_ROW:
        // Redraw only when the visible playhead moves.
        if (event->row != g_tracker.last_play_row) {
            g_tracker.last_play_row = event->row;
            g_tracker.needs_redraw = true;
            led_view_update_playhead(event->order_index);
        }
        break;

    case SEQ_EVENT_STOP:
        g_tracker.needs_redraw = true;
        break;

    case SEQ_EVENT_NOTE_ON:
    case SEQ_EVENT_NOTE_OFF:
    case SEQ_EVENT_NOTE_CUT:
    default:
        break;
    }
}

static void _button_callback(uint8_t index, bool state) {

    // Shift is a held modifier and must be tracked even on release.
    if (index == BUTTON_SHIFT) {
        g_tracker.shift = state;
        return;
    }

    // Only act on the press edge for the rest.
    if (!state) {
        return;
    }

    switch (index) {

    case BUTTON_PLAY_PAUSE:
    case BUTTON_STOP:
    case BUTTON_RECORD:
        _handle_transport(index);
        break;

    case BUTTON_BACK:
    case BUTTON_FORWARD:
    case BUTTON_LEFT:
    case BUTTON_RIGHT:
        _handle_navigation(index);
        break;

    case BUTTON_ERASE:
    case BUTTON_WRITE:
        _handle_edit(index);
        break;

    case BUTTON_TAP:
        if (g_tracker.shift) {
            // Shift + tap undoes the last edit.
            if (editor_undo(&g_tracker.editor)) {
                _set_message("UNDO");
            } else {
                _set_message("NOTHING TO UNDO");
            }
        } else {
            g_tracker.editor.insert = !g_tracker.editor.insert;
            g_tracker.cursor.insert = g_tracker.editor.insert;
            _set_message(g_tracker.editor.insert ? "INSERT MODE"
                                                 : "OVERWRITE MODE");
        }
        break;

    case BUTTON_PATTERN_SET:
        _cycle_pattern(g_tracker.shift ? -1 : 1);
        break;

    case BUTTON_MENU:
        if (g_tracker.shift) {
            _load_slot();
        } else {
            _save_slot();
        }
        break;

    case BUTTON_STEP_JUMP:
        if (g_tracker.shift) {
            editor_delete_row(&g_tracker.editor);
            _set_message("ROW DELETED");
        } else {
            editor_insert_row(&g_tracker.editor);
            _set_message("ROW INSERTED");
        }
        break;

    case BUTTON_BAR_1:
    case BUTTON_BAR_2:
    case BUTTON_BAR_3:
    case BUTTON_BAR_4:
        if (g_tracker.shift) {
            // Shift + bar sets the edit step 1..4.
            g_tracker.cursor.step = (uint8_t)((index - BUTTON_BAR_1) + 1);
            g_tracker.song.step = g_tracker.cursor.step;
            led_view_show_step(g_tracker.cursor.step);
        } else {
            _select_pattern((uint8_t)(index - BUTTON_BAR_1));
        }
        break;

    case BUTTON_MUTE:
        // Transpose the cell under the cursor (shift = up an octave).
        selection_set_point(&g_tracker.selection, g_tracker.cursor.pattern,
                            g_tracker.cursor.channel, g_tracker.cursor.row);
        if (g_tracker.shift) {
            editor_transpose(&g_tracker.editor, 12);
            _set_message("+12");
        } else {
            editor_transpose(&g_tracker.editor, -1);
            _set_message("-1");
        }
        break;

    case BUTTON_CHORD:
        // CHORD writes a note-off; shifted, it writes a note-cut.
        if (g_tracker.shift) {
            editor_enter_note_cut(&g_tracker.editor);
            _set_message("CUT");
        } else {
            editor_enter_note_off(&g_tracker.editor);
            _set_message("OFF");
        }
        break;

    case BUTTON_EXIT:
        ft_shutdown();
        break;

    default:
        return;
    }

    g_tracker.needs_redraw = true;
}

static void _encoder_callback(uint8_t index, int8_t value) {

    // Detail encoder adjusts the field under the cursor.
    if (index == ENCODER_MAIN || index == ENCODER_OSC) {

        if (g_tracker.shift) {
            // Coarse move by rows.
            cursor_move_row(&g_tracker.cursor, value, g_tracker.song.rows);
            cursor_clamp_scroll(&g_tracker.cursor, g_tracker.song.rows);
        } else {
            editor_adjust(&g_tracker.editor, value);
        }
        g_tracker.needs_redraw = true;
        return;
    }

    switch (index) {

    case ENCODER_CUTOFF:
        _change_octave(value);
        break;

    case ENCODER_MOD:
        _change_step(value);
        break;

    case ENCODER_IFX:
        _change_bpm((int16_t)(value * 5));
        break;

    default:
        break;
    }
}

static void _trigger_callback(uint8_t pad, uint8_t vel, bool state) {

    (void)vel;

    if (!state || pad >= 16) {
        return;
    }

    // Shift turns the pads into octave up/down controls.
    if (g_tracker.shift) {
        _change_octave((pad < 8) ? -1 : 1);
        g_tracker.needs_redraw = true;
        return;
    }

    _trigger_note(pad);
    g_tracker.needs_redraw = true;
}

static void _handle_transport(uint8_t button) {

    switch (button) {

    case BUTTON_PLAY_PAUSE:
        sequencer_toggle_play(&g_tracker.sequencer);
        led_view_update_transport(sequencer_is_playing(&g_tracker.sequencer),
                                  g_tracker.record);
        break;

    case BUTTON_STOP:
        sequencer_stop(&g_tracker.sequencer);
        g_tracker.cursor.row = 0;
        cursor_clamp_scroll(&g_tracker.cursor, g_tracker.song.rows);
        led_view_update_transport(false, g_tracker.record);
        break;

    case BUTTON_RECORD:
        g_tracker.record = !g_tracker.record;
        led_view_update_transport(sequencer_is_playing(&g_tracker.sequencer),
                                  g_tracker.record);
        _set_message(g_tracker.record ? "REC ON" : "REC OFF");
        break;

    default:
        break;
    }
}

static void _handle_navigation(uint8_t button) {

    switch (button) {

    case BUTTON_BACK:
        cursor_move_row(&g_tracker.cursor, -1, g_tracker.song.rows);
        break;

    case BUTTON_FORWARD:
        cursor_move_row(&g_tracker.cursor, 1, g_tracker.song.rows);
        break;

    case BUTTON_LEFT:
        if (g_tracker.shift) {
            cursor_move_channel(&g_tracker.cursor, -1);
        } else {
            cursor_move_column(&g_tracker.cursor, -1);
        }
        break;

    case BUTTON_RIGHT:
        if (g_tracker.shift) {
            cursor_move_channel(&g_tracker.cursor, 1);
        } else {
            cursor_move_column(&g_tracker.cursor, 1);
        }
        break;

    default:
        break;
    }

    cursor_clamp_scroll(&g_tracker.cursor, g_tracker.song.rows);
}

static void _handle_edit(uint8_t button) {

    if (button == BUTTON_ERASE) {

        if (g_tracker.shift) {
            editor_clear_channel(&g_tracker.editor);
            _set_message("CHANNEL CLEARED");
        } else {
            editor_clear_cell(&g_tracker.editor);
        }
        return;
    }

    // BUTTON_WRITE: copy, or paste when shifted.
    if (g_tracker.shift) {
        if (g_tracker.clipboard.has_data) {
            editor_paste(&g_tracker.editor);
            _set_message("PASTED");
        } else {
            _set_message("CLIPBOARD EMPTY");
        }
    } else {
        // A single-cell copy when no block is selected.
        selection_set_point(&g_tracker.selection, g_tracker.cursor.pattern,
                            g_tracker.cursor.channel, g_tracker.cursor.row);
        editor_copy(&g_tracker.editor);
        _set_message("COPIED");
    }
}

static void _select_pattern(uint8_t pattern) {

    g_tracker.cursor.pattern = pattern % TRACKER_PATTERNS;
    g_tracker.cursor.row = 0;
    g_tracker.cursor.row_scroll = 0;
    led_view_update_channels(&g_tracker.song, g_tracker.cursor.pattern);
    _set_message("PATTERN");
}

static void _cycle_pattern(int8_t delta) {

    int16_t pattern =
        (int16_t)g_tracker.cursor.pattern + delta;

    while (pattern < 0) {
        pattern += TRACKER_PATTERNS;
    }

    _select_pattern((uint8_t)(pattern % TRACKER_PATTERNS));
}

static void _change_octave(int8_t delta) {

    int16_t octave = (int16_t)g_tracker.cursor.octave + delta;

    if (octave < 0) {
        octave = 0;
    } else if (octave > 9) {
        octave = 9;
    }

    g_tracker.cursor.octave = (uint8_t)octave;

    char buf[16];
    snprintf(buf, sizeof(buf), "OCTAVE %u", (unsigned)g_tracker.cursor.octave);
    _set_message(buf);
}

static void _change_step(int8_t delta) {

    int16_t step = (int16_t)g_tracker.cursor.step + delta;

    // The blue bar LEDs show the step, so keep it in the 0..4 range they
    // can represent distinctly (0 = no advance).
    if (step < 0) {
        step = 0;
    } else if (step > 4) {
        step = 4;
    }

    g_tracker.cursor.step = (uint8_t)step;
    g_tracker.song.step = (uint8_t)step;

    led_view_show_step(g_tracker.cursor.step);

    char buf[16];
    snprintf(buf, sizeof(buf), "STEP %u", (unsigned)g_tracker.cursor.step);
    _set_message(buf);
}

static void _change_bpm(int16_t delta) {

    int16_t bpm = (int16_t)g_tracker.song.bpm + delta;

    if (bpm < 20) {
        bpm = 20;
    } else if (bpm > 400) {
        bpm = 400;
    }

    g_tracker.song.bpm = (uint16_t)bpm;
    transport_set_bpm(&g_tracker.sequencer.transport, (uint16_t)bpm);

    char buf[16];
    snprintf(buf, sizeof(buf), "BPM %u", (unsigned)bpm);
    _set_message(buf);
}

static void _save_slot(void) {

    e_storage_status status = storage_save(g_tracker.slot, &g_tracker.song);

    char buf[24];
    snprintf(buf, sizeof(buf), "SAVE SLOT %u %s", (unsigned)g_tracker.slot,
             storage_status_text(status));
    _set_message(buf);
}

static void _load_slot(void) {

    e_storage_status status = storage_load(g_tracker.slot, &g_tracker.song);

    if (status == STORAGE_OK) {
        // Keep the cursor valid against the loaded song.
        g_tracker.cursor.pattern = 0;
        g_tracker.cursor.row = 0;
        g_tracker.cursor.row_scroll = 0;
        cursor_clamp_scroll(&g_tracker.cursor, g_tracker.song.rows);
        transport_set_bpm(&g_tracker.sequencer.transport, g_tracker.song.bpm);
        undo_clear(&g_tracker.undo);
        led_view_update_channels(&g_tracker.song, g_tracker.cursor.pattern);
    }

    char buf[24];
    snprintf(buf, sizeof(buf), "LOAD SLOT %u %s", (unsigned)g_tracker.slot,
             storage_status_text(status));
    _set_message(buf);
}

static void _trigger_note(uint8_t pad) {

    // The 16 pads are a chromatic run of 16 semitones from the current
    // octave: pad 0 is the root, pad 12 the root an octave up.
    uint8_t semitone = pad;
    uint8_t channel = g_tracker.cursor.channel;
    uint8_t row = g_tracker.cursor.row;
    uint8_t pattern = g_tracker.cursor.pattern;

    if (g_tracker.record) {
        // Record mode: write the note and audition what was entered (the
        // cursor has already advanced past it).
        editor_enter_note(&g_tracker.editor, semitone);
        sequencer_preview(&g_tracker.sequencer, pattern, channel, row);
        return;
    }

    // Not recording: audition the pitch without altering the pattern.
    {
        t_cell *cell = song_cell(&g_tracker.song, pattern, channel, row);
        t_cell saved = *cell;

        cell->note = (uint8_t)(g_tracker.cursor.octave * 12 + semitone);
        cell->velocity = 100;
        cell->gate = 0xff;
        sequencer_preview(&g_tracker.sequencer, pattern, channel, row);

        *cell = saved;
    }
}

static void _set_message(const char *text) {

    if (text == NULL) {
        return;
    }

    strncpy(g_tracker.message, text, sizeof(g_tracker.message) - 1);
    g_tracker.message[sizeof(g_tracker.message) - 1] = '\0';
    g_tracker.message_ticks = MESSAGE_TICKS;
    g_tracker.needs_redraw = true;
}

static void _render_view(void) {

    g_tracker.view.song = &g_tracker.song;
    g_tracker.view.cursor = &g_tracker.cursor;
    g_tracker.view.message = g_tracker.message;
    g_tracker.view.playing = sequencer_is_playing(&g_tracker.sequencer);
    g_tracker.view.play_row = sequencer_playing_row(&g_tracker.sequencer);
    g_tracker.view.play_pattern = g_tracker.sequencer.playing_pattern;
    g_tracker.view.insert = g_tracker.editor.insert;
    g_tracker.view.recording = g_tracker.record;
    g_tracker.view.slot = g_tracker.slot;
}

static void _draw(void) {

    _render_view();
    tracker_view_render(&g_tracker.view);
    g_tracker.needs_redraw = false;
}

/*----- End of file --------------------------------------------------*/
