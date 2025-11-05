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
#include "seq.h"

#define BUTTON_EXIT 0x0d

void _button_callback(uint8_t index, bool state);
static void _tick_callback(void) ;
void foo_callback();


/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/


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
    
    ft_register_tick_callback(0, _tick_callback);

    SEQ_init(120.0f, 4, 4, 8, 16); // 15 BPM, 4/4, 1 note steps, 16 steps
    SEQ_fill_every_n(4);            // Activate every 4 steps
    SEQ_set_update_mode(SEQ_MODE_IMMEDIATE);
    SEQ_start();
    SEQ_set_step_callback(0, foo_callback);
    ft_print("sequencer");

    return SUCCESS;
}

void foo_callback(){
    ft_print("Step triggered!");
}

/**
 * @brief   Run application.
 *
 * Test if 0.5 seconds have passed  since we started the delay.
 * If so, toggle an LED and restart delay.
 */
void app_run(void) {
   
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


 static void _tick_callback(void) {
 SEQ_tick();


}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
