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
 * @file    theme.h
 *
 * @brief   Display layout constants for the tracker view.
 *
 * All pixel positions live here so the layout can be tuned in one place
 * without touching drawing logic.  The 128x64 panel is divided into a
 * two-line header, a scrolling grid, and a status line.
 */

#ifndef TRACKER_THEME_H
#define TRACKER_THEME_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

/*----- Macros -------------------------------------------------------*/

// Character cell (FONT_6X8).
#define TH_CHAR_W 6
#define TH_CHAR_H 8

// Header.
#define TH_HEADER1_Y 0
#define TH_HEADER2_Y 8
#define TH_SEP1_Y 16

// Grid.
#define TH_GRID_Y 17
#define TH_ROW_H 8
#define TH_ROW_NUM_X 0
#define TH_GRID_X 13

// Channel layout within the grid.
#define TH_CHAN_X0 13
#define TH_CHAN_STRIDE 57
#define TH_NOTE_OFF 0   // "C-4"
#define TH_INS_OFF 24   // "01"
#define TH_VOL_OFF 42   // "7F"
#define TH_FX_OFF 42    // FX shares the volume slot when focused.

// Separator / status.
#define TH_SEP2_Y 50
#define TH_STATUS_Y 52

// Layout counts.
#define TH_STATUS_CHARS 21

#ifdef __cplusplus
}
#endif
#endif /* TRACKER_THEME_H */

/*----- End of file --------------------------------------------------*/
