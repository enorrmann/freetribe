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
 * @file    tracker_types.c
 *
 * @brief   Formatting helpers for tracker value types.
 */

/*----- Includes -----------------------------------------------------*/

#include "tracker_types.h"

/*----- Macros -------------------------------------------------------*/

static const char NOTE_NAMES[12][3] = {"C-", "C#", "D-", "D#", "E-", "F-",
                                       "F#", "G-", "G#", "A-", "A#", "B-"};

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

void note_format(char *out, uint8_t note) {
    // MIDI note 0 is C-(-1), so octave = note / 12 - 1.
    uint8_t name = note % 12;
    int8_t octave = (int8_t)(note / 12) - 1;

    out[0] = NOTE_NAMES[name][0];
    out[1] = NOTE_NAMES[name][1];

    if (octave < 0) {
        out[2] = '-';
    } else if (octave > 9) {
        out[2] = '+';
    } else {
        out[2] = (char)('0' + octave);
    }
}

void note_format_cell(char *out, uint8_t note) {

    if (note == NOTE_NONE) {
        out[0] = '-';
        out[1] = '-';
        out[2] = '-';
    } else if (note == NOTE_OFF) {
        out[0] = 'O';
        out[1] = 'F';
        out[2] = 'F';
    } else if (note == NOTE_CUT) {
        out[0] = 'C';
        out[1] = 'U';
        out[2] = 'T';
    } else {
        note_format(out, note);
    }
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
