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
 * @file    sdcard.c
 *
 * @brief   Example app showing how to use the SD card.
 */

/*----- Includes -----------------------------------------------------*/

#include "per_gpio.h"

#include "freetribe.h"
// BEGIN FF
#include "ff.h"
#include "macros.h"
// END FF
#include "dev_dsp_ipc.h"
void _ipc_callback(void *ctx, t_ipc_status status) ;

void _trigger_callback(uint8_t pad, uint8_t vel, bool state);

void _ipc_callback(void *ctx, t_ipc_status status) {
    ft_printf("IPC callback called with user context");
    ft_printf("IPC transfer status from test: %i", (int)status);
}

// must be outside function
// MUST BE MULTIPLE OF 32 !!
#define DATA_SIZE 32
uint32_t to_send [DATA_SIZE];

// Bank 7 pin 15.
#define GPIO_POWER_BUTTON 128
void ipc_simple_send(uint32_t value) {

    uint32_t base_address = 0x00000060;
             to_send [0] = value;
        int status = dev_dsp_ipc_transfer(
            base_address, to_send, DATA_SIZE, _ipc_callback,
             (void*)0x23AC1D23 // arbitrary user context value for testing
        );


         ft_printf("simple sent: %u, status %i", value, status);
             ft_get_module_param(0, 0);


}


void _trigger_callback(uint8_t pad, uint8_t vel, bool state) {
    if (state) {
        ipc_simple_send(pad); // for testing
        
    }
}
void param_callback(uint16_t module_id, uint16_t param_index, int32_t param_value){
    ft_printf("Received param callback! module_id: %u, param_index: %u, param_value: %d", module_id, param_index, param_value);

}
/**
 * @brief   Initialise application.
 *
 * @return status   Status code indicating success:
 *                  - SUCCESS
 *                  - WARNING
 *                  - ERROR
 */
t_status app_init(void) {

    ft_register_panel_callback(TRIGGER_EVENT, _trigger_callback);
ft_register_dsp_callback(MSG_TYPE_MODULE, MODULE_PARAM_VALUE, param_callback);

    t_status status = ERROR;


    status = SUCCESS;
    return status;
}

/**
 * @brief   Run application.
 *
 * Test if GPIO index 128 (bank 7 pin 15) is low, if so, shutdown.
 */
void app_run(void) {

    if (per_gpio_get_indexed(GPIO_POWER_BUTTON) == 0) {

        ft_shutdown();

        // Should never reach here.
    }
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
