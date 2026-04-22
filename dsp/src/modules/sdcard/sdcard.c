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

                       Copyright bangcorrupt 2023

----------------------------------------------------------------------*/

/**
 * @file    sdcard.c
 *
 * @brief   Template for DSP module source files.
 *
 */

/*----- Includes -----------------------------------------------------*/

#include "fract_math.h"
#include "types.h"

#include "module.h"

#include "utils.h"
#include "parameters.h"
#include "dev_cpu_ipc.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/



/*----- Static variable definitions ----------------------------------*/

// #define SDRAM_ADDRESS 0x00000000
#define SDRAM_ADDRESS 0x00000060 // for ipc transfer headers

fract32 *data_sdram = (fract32 *)SDRAM_ADDRESS;
uint32_t record_index = 0;
uint32_t play_index = 0;
uint32_t total_samples=0;
uint32_t cpu_callback_function_address = 666;
uint32_t cpu_ipc_receive_buffer_address = 666;
int response = 666;


#define MIN_IPC_TRANSFER_SIZE 32
uint32_t dsp_ipc_send_buffer [MIN_IPC_TRANSFER_SIZE] ;
uint32_t to_receive [MIN_IPC_TRANSFER_SIZE] ;


#define MAX_SIZE 48000 *10

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/



void ipc_callback(void *ctx, t_ipc_status status) ;

void ipc_callback(void *ctx, t_ipc_status status) {
    // this is never called, becasue response is still 666 after this
    //response = 16; // just to test that callback is being called at all
}
void ipc_callback_send(void *ctx, t_ipc_status status) {
    // this is never called, becasue response is still 666 after this
    response = 16; // just to test that callback is being called at all
}

/**
 * @brief   Initialise module.
 */
void module_init(void) {
    // initialize data_sdram to 0
    int i;
    for (i = 0; i < MAX_SIZE; i++) {
        data_sdram[i] = 0;
    }
    
    
}

/**
 * @brief   Process audio.
 *
 * @param[in]   in  Pointer to input buffer.
 * @param[out]  out Pointer to input buffer.
 */
void module_process(fract32 *in, fract32 *out) {
   
    if (play_index < total_samples) {
        fract32 output = data_sdram[play_index];
        play_index++; 
        out[0] = output;
        out[1] = output;
        
    }
}

/**
 * @brief   Set parameter.
 *
 * @param[in]   param_index Index of parameter to set.
 * @param[in]   value      Value of parameter.
 */
void module_set_param(uint16_t param_index, int32_t value) {
    switch (param_index) {
        case PARAM_TRANSMISSION_START:
            record_index = 0; // reset record_index index at start of transmission
            play_index = total_samples+1; //stop playing
            break;
        case PARAM_SAMPLE_LOAD:
            data_sdram[record_index] = value; // load sample into SDRAM at current play index
            record_index++;
            break;
            case PARAM_TRANSMISSION_END:
            total_samples = value; 
            play_index = 0;
            break;
            case PARAM_SAMPLE_COUNT_UPDATE:
            total_samples = value; 
            break;

            case PARAM_SET_CPU_RECEIVE_BUFFER_ADDRESS:
                cpu_ipc_receive_buffer_address = value;
            break;
            case PARAM_SET_CPU_CALLBACK_FUNCTION_ADDRESS:
                cpu_callback_function_address = value;
            break;
            
            case PARAM_TRANSFER_MEMORY:{
                // address 0 arbitrary       
                // reads the value of address 0 and stores it in to_receive
                 dev_cpu_ipc_request_data(
                        value, to_receive, MIN_IPC_TRANSFER_SIZE, ipc_callback,
                        (void *)0x23AC1D23 // arbitrary user context value for testing
                    );
                }
                dsp_ipc_send_buffer[0] = cpu_callback_function_address; 
                dev_cpu_ipc_transfer(
                    cpu_ipc_receive_buffer_address, dsp_ipc_send_buffer, MIN_IPC_TRANSFER_SIZE, ipc_callback_send,
                    (void *)0x23AC1D23 // arbitrary user context value for testing
                );
                break;
    default:
        break;
    }
}

/**
 * @brief   Get parameter.
 *
 * @param[in]   param_index Index of parameter to get.
 *
 * @return      value       Value of parameter.
 */
int32_t module_get_param(uint16_t param_index) {

    int32_t value = 0;

    switch (param_index) {

    default:
        break;
    }

    return cpu_ipc_receive_buffer_address; // just to test that value is being updated at all
}

/**
 * @brief   Get number of parameters.
 *
 * @return  Number of parameters
 */
uint32_t module_get_param_count(void) { return PARAM_COUNT; }

/**
 * @brief   Get name of parameter at index.
 *
 * @param[in]   param_index     Index pf parameter.
 * @param[out]  text            Buffer to store string.
 *                              Must provide 'MAX_PARAM_NAME_LENGTH'
 *                              bytes of storage.
 */
void module_get_param_name(uint16_t param_index, char *text) {

    switch (param_index) {

    default:
        copy_string(text, "Unknown", MAX_PARAM_NAME_LENGTH);
        break;
    }
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
