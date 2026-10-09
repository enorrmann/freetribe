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
 * @file    editor.h
 *
 * @brief   Editing commands operating on the song at the cursor.
 *
 * The editor is the bridge between input events and the data model.  All
 * mutations go through here so that undo snapshots, insert mode and the
 * "last value" channel state stay consistent.
 */

#ifndef TRACKER_EDITOR_H
#define TRACKER_EDITOR_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

#include "clipboard.h"
#include "cursor.h"
#include "song.h"
#include "undo.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/**
 * @brief   Snapshot of the editing session the editor mutates.
 *
 * Bundling the model pointers keeps the editor's signature small and the
 * call-sites readable.
 */
typedef struct {
    t_song *song;
    t_cursor *cursor;
    t_clipboard *clipboard;
    t_undo_stack *undo;
    t_selection *selection;

    bool insert; // Insert mode (off = overwrite).
} t_editor;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void editor_init(t_editor *editor, t_song *song, t_cursor *cursor,
                 t_clipboard *clipboard, t_undo_stack *undo,
                 t_selection *selection);

/**
 * @brief   Write a note into the current cell.
 *
 * Honours the current octave, advances the cursor by the edit step, and in
 * insert mode pushes the rows below down and places a note-off on the
 * following row.
 */
void editor_enter_note(t_editor *editor, uint8_t semitone);

/**
 * @brief   Write NOTE_OFF into the current cell.
 */
void editor_enter_note_off(t_editor *editor);

/**
 * @brief   Write NOTE_CUT into the current cell.
 */
void editor_enter_note_cut(t_editor *editor);

/**
 * @brief   Adjust a numeric field at the cursor by a signed delta.
 */
void editor_adjust(t_editor *editor, int8_t delta);

/**
 * @brief   Clear the current cell.
 */
void editor_clear_cell(t_editor *editor);

/**
 * @brief   Clear the whole channel under the cursor (all rows).
 */
void editor_clear_channel(t_editor *editor);

/**
 * @brief   Set the velocity/instrument of every cell in the current column.
 */
void editor_fill_column(t_editor *editor, uint8_t value);

/**
 * @brief   Delete the row at the cursor, shifting rows up.
 */
void editor_delete_row(t_editor *editor);

/**
 * @brief   Insert a blank row at the cursor, shifting rows down.
 */
void editor_insert_row(t_editor *editor);

/**
 * @brief   Undo the last destructive edit.
 */
bool editor_undo(t_editor *editor);

/**
 * @brief   Copy the selection to the clipboard.
 */
void editor_copy(t_editor *editor);

/**
 * @brief   Cut the selection to the clipboard.
 */
void editor_cut(t_editor *editor);

/**
 * @brief   Paste the clipboard at the cursor.
 */
void editor_paste(t_editor *editor);

/**
 * @brief   Transpose the selection by semitones.
 */
void editor_transpose(t_editor *editor, int8_t semitones);

#ifdef __cplusplus
}
#endif
#endif /* TRACKER_EDITOR_H */

/*----- End of file --------------------------------------------------*/
