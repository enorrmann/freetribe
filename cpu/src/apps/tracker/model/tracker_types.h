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
 * @file    tracker_types.h
 *
 * @brief   Shared value types for the tracker data model.
 *
 * This header defines the plain-old-data values that make up a tracker
 * pattern (notes, instruments, velocities, effects) together with the
 * small helpers used to inspect them.  It has no dependency on hardware,
 * the GUI or the sequencer, so it can be reused by any layer.
 */

#ifndef TRACKER_TYPES_H
#define TRACKER_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

/*----- Macros -------------------------------------------------------*/

#define TRACKER_CHANNELS 16
#define TRACKER_ROWS 64
#define TRACKER_PATTERNS 32
#define TRACKER_SONG_LENGTH 128

// Effect command range (hex 00..FF).
#define FX_NONE 0x00

/*----- Typedefs -----------------------------------------------------*/

/**
 * @brief   A single note.
 *
 * NOTE_NONE means "empty row"; NOTE_OFF is an explicit note-off (release);
 * NOTE_CUT is an explicit cut (fast release / stop).  Values in between
 * map linearly to MIDI note numbers, so a cell stores the MIDI note plus
 * two reserved codes above it.
 */
typedef enum {
    NOTE_NONE = 0x00, // Empty cell.
    NOTE_OFF = 0xfe,  // Release / note off.
    NOTE_CUT = 0xff,  // Cut / stop.
} e_note_special;

/**
 * @brief   A pattern cell.
 *
 * The cell is deliberately byte sized so a full pattern fits comfortably
 * in memory and is trivial to copy, clear and serialise.
 */
typedef struct {
    uint8_t note;     // MIDI note, NOTE_NONE / NOTE_OFF / NOTE_CUT.
    uint8_t velocity; // 0x00..0x7f, 0xff = "use default".
    uint8_t gate;     // 0x00..0xff, fraction of a row (0xff = legato).
    uint8_t instrument; // 0x00..0x7f, 0xff = "use last".
    uint8_t fxx;      // Effect command byte (0x00 = none).
    uint8_t fxx_data; // Effect parameter byte.
} t_cell;

/**
 * @brief   A pattern: a 2D array of cells.
 */
typedef struct {
    t_cell cell[TRACKER_CHANNELS][TRACKER_ROWS];
} t_pattern;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

/**
 * @brief   True when the note byte represents an audible pitch.
 */
static inline bool note_is_pitch(uint8_t note) {
    return note != NOTE_NONE && note != NOTE_OFF && note != NOTE_CUT;
}

/**
 * @brief   Format a MIDI note number as a 3 character name, e.g. "C-4".
 *
 * @param[out]  out     Destination, must have room for 3 chars (no NUL).
 * @param[in]   note    MIDI note number.
 */
void note_format(char *out, uint8_t note);

/**
 * @brief   Render a cell note byte in tracker notation.
 *
 * Writes exactly three characters: "---" (empty), "OFF", "CUT" or a
 * note name such as "C-4".
 */
void note_format_cell(char *out, uint8_t note);

#ifdef __cplusplus
}
#endif
#endif /* TRACKER_TYPES_H */

/*----- End of file --------------------------------------------------*/
