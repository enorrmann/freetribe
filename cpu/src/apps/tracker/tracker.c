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

/*----- Includes -----------------------------------------------------*/

#include "freetribe.h"
#include "gui_task.h"

#define BUTTON_EXIT 0x0d
#define LINE_BREAK "\n"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

void _button_callback(uint8_t index, bool state);
void _pad_callback(uint32_t x_val, uint32_t y_val);

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

/**
 * @brief   Initialise application.
 *
 * Initialise a delay.
 * Status is assumed successful.
 *
 * @return status   Status code indicating success:
 *                  - SUCCESS
 *                  - WARNING
 *                  - ERROR
 */
t_status app_init(void) {

    ft_register_panel_callback(BUTTON_EVENT, _button_callback);
    ft_register_panel_callback(XY_PAD_EVENT, _pad_callback);

    gui_task();
    ft_print("tracker");
    //gui_post_label("buenas");
    //gui_post_label_xy("1:00mC 1 0:43 10456", 0, 00);
    //gui_post_label_xy("1:00mC 1 0:43 1044", 0, 10);
    gui_post_label_xy("XX", 0, 20);

    return SUCCESS;
}

/**
 * @brief   Run application.
 *
 * Test if 0.5 seconds have passed  since we started the delay.
 * If so, toggle an LED and restart delay.
 */
void app_run(void) {
    gui_task();
    //  process_tempo_tick();
}

/*----- Static function implementations ------------------------------*/

/**
 * @brief   Callback triggered by panel button events.
 *
 * @param[in]   index   Index of button.
 * @param[in]   state   State of button.
 */
void _button_callback(uint8_t index, bool state) {

    switch (index) {

    case BUTTON_EXIT:
        if (state == 1) {
            ft_shutdown();
        }
        break;

    default:
        break;
    }
}

// FROM 0 to 1023
void _pad_callback(uint32_t x, uint32_t y) {
    // g_device.y_dim 64
    int rel_y = (y * 63) / 1023;
    int rel_x = (x * 127) / 1023;
    if (rel_y <= 20)
        rel_y = 20;
    if (rel_y >= 50)
        rel_y = 50;
    if (rel_x <= 20)
        rel_x = 20;
    if (rel_x >= 110)
        rel_x = 110;
     /*ft_print(int_to_char(rel_x));
      ft_print(" , ");
     ft_print(int_to_char(rel_y));
     ft_print(LINE_BREAK);*/
    //gui_post_label_xy(".", rel_x, 64-rel_y);
}

/*----- End of file --------------------------------------------------*/
