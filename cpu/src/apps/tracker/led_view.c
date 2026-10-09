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
 * @file    led_view.c
 *
 * @brief   Panel LED feedback implementation.
 */

/*----- Includes -----------------------------------------------------*/

#include "freetribe.h"

#include "led_view.h"

/*----- Macros -------------------------------------------------------*/

// Bar LEDs (red channel) used as a playhead / position indicator.
static const t_led_index BAR_LEDS[4] = {LED_BAR_0_RED, LED_BAR_1_RED,
                                        LED_BAR_2_RED, LED_BAR_3_RED};

// The first eight pad LEDs (red) map to channels 0..7.
static const t_led_index PAD_LEDS[8] = {
    LED_PAD_0_RED, LED_PAD_1_RED, LED_PAD_2_RED, LED_PAD_3_RED,
    LED_PAD_4_RED, LED_PAD_5_RED, LED_PAD_6_RED, LED_PAD_7_RED};

// Bar LEDs (blue channel) used to show the edit step.
static const t_led_index BAR_BLUE[4] = {LED_BAR_0_BLUE, LED_BAR_1_BLUE,
                                        LED_BAR_2_BLUE, LED_BAR_3_BLUE};

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

void led_view_update_transport(bool playing, bool recording) {

    ft_set_led(LED_PLAY, playing ? 0xff : 0x00);
    ft_set_led(LED_STOP, playing ? 0x00 : 0xff);
    ft_set_led(LED_REC, recording ? 0xff : 0x00);
}

void led_view_update_playhead(uint8_t order_index) {

    uint8_t i;

    for (i = 0; i < 4; i++) {
        ft_set_led(BAR_LEDS[i], (i == (order_index & 0x03)) ? 0xff : 0x00);
    }
}

void led_view_update_channels(const t_song *song, uint8_t pattern) {

    uint8_t ch;
    uint8_t row;

    for (ch = 0; ch < 8; ch++) {

        bool active = false;

        for (row = 0; row < song->rows && !active; row++) {
            const t_cell *cell = song_cell((t_song *)song, pattern, ch, row);
            if (cell->note != NOTE_NONE) {
                active = true;
            }
        }

        ft_set_led(PAD_LEDS[ch], active ? 0xff : 0x00);
    }
}

void led_view_show_step(uint8_t step) {

    uint8_t i;

    if (step == 0) {
        step = 1;
    }

    for (i = 0; i < 4; i++) {
        // Encode step 1..4 as a growing blue bar.
        ft_set_led(BAR_BLUE[i], (i < step) ? 0xff : 0x00);
    }
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
