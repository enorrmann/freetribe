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
 * @file    template_module.c
 *
 * @brief   Template for DSP module source files.
 *
 */

/*----- Includes -----------------------------------------------------*/

#include "types.h"
#include "module.h"
#include "utils.h"

/*----- Macros -------------------------------------------------------*/

#define SDRAM_ADDRESS 0x00000060 // for ipc transfer headers
#define DSP_BUFFER_SIZE_IN_32_BIT_WORDS (48000 * 2) // must be the same on the cpu side

fract32 *sdram_ring_buffer = (fract32 *)SDRAM_ADDRESS;
fract16 *sdram_ring_buffer_16 = (fract16 *)SDRAM_ADDRESS;
uint32_t record_index = 0;


/*----- Typedefs -----------------------------------------------------*/

/**
 * @brief   Enumeration of module parameters.
 *
 * Index of each external parameter of module.
 */
typedef enum {

    PARAM_COUNT /// Should remain last to return number of parameters.
} e_param;

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

//volatile uint32_t *g_record_index_ptr = (volatile uint32_t *)0x0000005C;
 uint32_t g_record_index_ptr = 0;

/**
 * @brief   Initialise module.
 */
void module_init(void) {
    g_record_index_ptr = 0;
}

void store_to_sdram(fract32 left, fract32 right) {
    static fract32 counter = 0;
    counter += 1000000 * 8; 

    if (g_record_index_ptr >= DSP_BUFFER_SIZE_IN_32_BIT_WORDS - 1) {
        g_record_index_ptr = 0;
    }
    
    uint32_t idx = g_record_index_ptr;
    sdram_ring_buffer[idx++] = counter;      // Canal L
    sdram_ring_buffer[idx++] = -counter;     // Canal R (invertido)
    //sdram_ring_buffer[idx++] = left;      // Canal L
    //sdram_ring_buffer[idx++] = right;     // Canal R (invertido)

    g_record_index_ptr = idx;
}

void store_to_sdram_16(fract32 left, fract32 right) {
    static fract32 counter = 0;
    counter += 1000000 * 8; 

    if (g_record_index_ptr >= DSP_BUFFER_SIZE_IN_32_BIT_WORDS - 1) {
        g_record_index_ptr = 0;
    }
    
    uint32_t idx = g_record_index_ptr;
    sdram_ring_buffer_16[idx++] = counter;      // Canal L
    sdram_ring_buffer_16[idx++] = -counter;     // Canal R (invertido)
    //sdram_ring_buffer_16[idx++] = left;      // Canal L
    //sdram_ring_buffer_16[idx++] = right;     // Canal R (invertido)

    g_record_index_ptr = idx;
}

/**
 * @brief   Process audio.
 *
 * @param[in]   in  Pointer to input buffer.
 * @param[out]  out Pointer to input buffer.
 */
void module_process(fract32 *in, fract32 *out) {
    store_to_sdram(in[0], in[1]);
    //store_to_sdram_16(in[0], in[1]);
}

/**
 * @brief   Set parameter.
 *
 * @param[in]   param_index Index of parameter to set.
 * @param[in]   value      Value of parameter.
 */
void module_set_param(uint16_t param_index, int32_t value) {

    switch (param_index) {

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

    return value;
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
