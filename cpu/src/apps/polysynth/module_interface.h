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
 * @file    module_interface.h
 *
 * @brief   Public API for CPU interface to DSP module.
 */

#ifndef MODULE_INTERFACE_H
#define MODULE_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdint.h>

#include "leaf.h"
#include "voice_manager.h"
#include "common/params.h"

/*----- Macros -------------------------------------------------------*/

#define DEFAULT_CUTOFF 0x7f // Index in pitch LUT.

#define PARAM_MAX (PARAM_COUNT - 1)
#define FILTER_TYPE_MAX (FILTER_TYPE_COUNT - 1)
#define MOD_TYPE_MAX (MOD_TYPE_COUNT - 1)
#define OSC_TYPE_MAX (OSC_TYPE_COUNT - 1)

/*----- Typedefs -----------------------------------------------------*/


typedef enum {
    OSC_TYPE_SINE,
    OSC_TYPE_TRI,
    OSC_TYPE_SAW,
    OSC_TYPE_SQUARE,
    OSC_TYPE_NONE,

    OSC_TYPE_COUNT
} e_osc_type;

typedef enum {
    FILTER_TYPE_LPF,
    FILTER_TYPE_HPF,
    FILTER_TYPE_BPF,

    FILTER_TYPE_COUNT
} e_filter_type;

typedef enum {
    MOD_AMP_LFO,
    MOD_FILTER_LFO,
    MOD_PITCH_LFO,

    MOD_TYPE_COUNT
} e_mod_type;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

/// TODO: Make LEAF optional dependency.
//
void module_init(LEAF *leaf);
void module_process(void);
void module_set_param_voice(uint16_t voice_index,uint16_t param_index, float value);
void module_set_param_all_voices(uint16_t param_index, float value);
float module_get_param(uint16_t param_index);

#ifdef __cplusplus
}
#endif
#endif

/*----- End of file --------------------------------------------------*/
