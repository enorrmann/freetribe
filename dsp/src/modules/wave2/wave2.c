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
 * @file    wave2.c
 *
 * @brief   A wavetable synth module for Freetribe.
 */

/*----- Includes -----------------------------------------------------*/

#include <stdint.h>

#include "module.h"
#include "param_scale.h"
#include "types.h"
#include "utils.h"
#include <math.h>
#include "common/sample.h"
#include "common/parameters.h"
#include "sample_manager.h"

#include "aleph.h"

#include "custom_aleph_monovoice.h"

void module_set_param_voice(uint16_t voice_index, uint16_t param_index,
                            int32_t value);

/*----- Macros -------------------------------------------------------*/

#define MEMPOOL_SIZE (0x2000)


// #define DEFAULT_CUTOFF 0x7f // Index in pitch LUT.
#define DEFAULT_OSC_TYPE 2
#define MAX_VOICES 6
#define PARAM_VOICE_OFFSET(voice, param, paramCount)                           \
    (param + (voice * paramCount))
#define REMOVE_PARAM_OFFSET(param_index_with_offset, paramCount)               \
    ((param_index_with_offset) % paramCount)
#define PARAM_VOICE_NUMBER(param_index_with_offset, paramCount)                \
    ((param_index_with_offset) / paramCount)

/// TODO: Move to common location.
/**
 * @brief   Enumeration of module parameters.
 *
 * Index of each external parameter of module.
 */

/*----- Typedefs -----------------------------------------------------*/


typedef struct {

    Custom_Aleph_MonoVoice voice[MAX_VOICES];
    fract32 amp_level;
    fract32 velocity;
    fract16 morph_amount;
    int active_sample_slot;


} t_module;

/*----- Static variable definitions ----------------------------------*/

__attribute__((section(".l1.data.a")))
__attribute__((aligned(32))) static char g_mempool[MEMPOOL_SIZE];

static t_Aleph g_aleph;
static t_module g_module;
static int recording = 0;

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/
/*----- Extern function implementations ------------------------------*/

#define FOUR_MB (440 * 1024 * 1024)   // 40 Megabytes en bytes
#define FRACT32_BYTES sizeof(fract32) // Tamaño de un fract32 en bytes
#define NUM_FRACT32_ELEMENTS                                                   \
    (FOUR_MB / FRACT32_BYTES) // Cantidad de elementos fract32

/**
 * @brief   Initialise module.
 */
void module_init(void) {

    // generate_soft_sawtooth(0);
    //  generate_soft_square(1);

    // write_to_sdram_4mb();
    // wavtab_big_counter = 0;
    SMPMAN_init();
    Aleph_init(&g_aleph, SAMPLERATE, g_mempool, MEMPOOL_SIZE, NULL);
    g_module.active_sample_slot  = 0;

    int i;
    for (i = 0; i < MAX_VOICES; i++) {
        Custom_Aleph_MonoVoice_init(&g_module.voice[i], &g_aleph);

        module_set_param_voice(i, PARAM_AMP_LEVEL, FR32_MAX);
        module_set_param_voice(i, PARAM_OSC_TYPE, 2);
        module_set_param_voice(i, PARAM_OSC_2_TYPE, 2);
        module_set_param_voice(i, PARAM_FREQ, 220 << 16);
        module_set_param_voice(i, PARAM_TUNE, FIX16_ONE);
        module_set_param_voice(i, PARAM_CUTOFF, 0x326f6abb);
        module_set_param_voice(i, PARAM_RES, FR32_MAX);
    }
}

#include <stdbool.h>
#include <stdint.h>


#define WINDOW_SIZE 4096 // nº de samples por ventana para decidir


void _threshold_test_module_process(fract32 *in, fract32 *out) {

    static uint32_t peak_accum = 0; // almacena máximo absoluto en la ventana
    static int sample_count = 0;    // contador de muestras en ventana
    static bool gate_open = false;  // estado actual de la puerta

    fract32 *outl = out;
    fract32 *outr = out + BLOCK_SIZE;
    fract32 *inl = in;
    fract32 *inr = in + BLOCK_SIZE;

    // Threshold entero 0..1024 convertido a fract32
    uint16_t thr_int = (uint16_t)g_module.voice[0]->playback_rate;
    fract32 threshold_fract =
        (thr_int >= 1024) ? 0x7FFFFFFF
                          : (fract32)(((int64_t)thr_int * 0x7FFFFFFF) >> 10);
    int i;
    for (i = 0; i < BLOCK_SIZE; ++i) {
        // Magnitud absoluta de la muestra izquierda
        uint32_t mag =
            (uint32_t)(inl[i] < 0 ? -(int64_t)inl[i] : (int64_t)inl[i]);

        // Actualizar máximo de la ventana
        if (mag > peak_accum) {
            peak_accum = mag;
        }

        sample_count++;

        // Al final de la ventana decidimos estado del gate
        if (sample_count >= WINDOW_SIZE) {
            gate_open = (peak_accum > (uint32_t)threshold_fract);
            // reset para siguiente ventana
            sample_count = 0;
            peak_accum = 0;
        }

        // Aplicar decisión de la ventana completa
        if (gate_open) {
            outl[i] = inl[i];
            outr[i] = inr[i];
        } else {
            outl[i] = 0;
            outr[i] = 0;
        }
    }
}

/**
 * @brief   Process audio.
 *
 * @param[in]   in  Pointer to input buffer.
 * @param[out]  out Pointer to input buffer.
 */
void module_process(fract32 *in, fract32 *out) {

    fract32 *outl = out;
    fract32 *outr = out + BLOCK_SIZE;

    fract32 *inl = in;
    fract32 *inr = in + BLOCK_SIZE;

     if (recording) {
            int i;
            for (i = 0; i < BLOCK_SIZE; i++) {
                // record the sum of l+r
                //fract32 to_record = add_fr1x32(inl[i]>>1,-1*(inr[i]>>1)); // x -1 to record balanced
                fract32 to_record = inl[i];
                SMPMAN_record(to_record); 
                outl[i] = to_record; // Canal izquierdo
                outr[i] = to_record; // Canal derecho
            }

        return;
    }

    int i;
    for (i = 0; i < BLOCK_SIZE; i++) {

        outl[i] = Custom_Aleph_MonoVoice_next(&g_module.voice[0]);
        int j;
        for (j = 1; j < MAX_VOICES; j++) {
            outl[i] = add_fr1x32(
                outl[i], Custom_Aleph_MonoVoice_next(&g_module.voice[j]));
        }

        outr[i] = outl[i];
    }
}

void module_set_param_voice(uint16_t voice_index, uint16_t param_index,
                            int32_t value) {
    uint16_t paramWithOffset =
        PARAM_VOICE_OFFSET(voice_index, param_index, PARAM_COUNT);
    module_set_param(paramWithOffset, value);
}


void module_set_sample_param(uint16_t param_index_with_offset, int32_t value) {
    int index = param_index_with_offset-SAMPLE_PARAMETER_OFFSET;
    uint16_t param_index = REMOVE_PARAM_OFFSET(index, SAMPLE_PARAM_COUNT);
    int sample_number = PARAM_VOICE_NUMBER(index, SAMPLE_PARAM_COUNT);
    SMPMAN_set_parameter(sample_number, param_index, value);

    

}


/**
 * @brief   Set parameter.
 *
 * @param[in]   param_index Index of parameter to set.
 * @param[in]   value       Value of parameter.
 */
void module_set_param(uint16_t param_index_with_offset, int32_t value) {

    if (param_index_with_offset>=SAMPLE_PARAMETER_OFFSET){
        module_set_sample_param(param_index_with_offset,value);
        return;
    }

    uint16_t param_index =
        REMOVE_PARAM_OFFSET(param_index_with_offset, PARAM_COUNT);
    int voice_number = PARAM_VOICE_NUMBER(param_index_with_offset, PARAM_COUNT);

    switch (param_index) {

    case SAMPLE_RECORD_START:
        recording = 1;
         SMPMAN_record_reset(); // dont reset, keep recording after
/*        if (g_module.active_sample_slot==0){
            wavtab_index = 0;
        }
        else {
            wavtab_index = g_module.samples[g_module.active_sample_slot-1]->end_position;
        }
        if (wavtab_index<0){
            wavtab_index = 0;
        }*/
        //g_module.samples[g_module.active_sample_slot]->start_position = wavtab_index; // move to sample manager

        break;

    case SAMPLE_RECORD_STOP:
        recording = 0;
        // move to sample manager g_module.samples[g_module.active_sample_slot]->end_position = wavtab_index;
        // move to sample manager g_module.active_sample_slot ++;
        break;

    // called from sysex manager and or input sample recorder
    case SAMPLE_LOAD:
        SMPMAN_record(value);
        break;

    case PARAM_AMP:
        Custom_Aleph_MonoVoice_set_amp(&g_module.voice[voice_number], value);
        break;

    case PARAM_FREQ:
        Custom_Aleph_MonoVoice_set_freq(&g_module.voice[voice_number], value);
        break;

    case PARAM_OSC_PHASE:
        Custom_Aleph_MonoVoice_set_phase(&g_module.voice[voice_number], value);
        break;

    case PARAM_MORPH_AMOUNT:
        Custom_Aleph_MonoVoice_set_morph_amount(&g_module.voice[voice_number],
                                                value);
        break;

    case PARAM_AMP_LEVEL:
        g_module.amp_level = value;
        break;

    case PARAM_CUTOFF:
        Custom_Aleph_MonoVoice_set_cutoff(&g_module.voice[voice_number], value);
        break;

    case PARAM_RES:
        Custom_Aleph_MonoVoice_set_res(&g_module.voice[voice_number], value);
        break;

    case PARAM_OSC_TYPE:
        Custom_Aleph_MonoVoice_set_shape(&g_module.voice[voice_number], value);
        break;

    case PARAM_FILTER_TYPE:
        Custom_Aleph_MonoVoice_set_filter_type(&g_module.voice[voice_number],
                                               value);
        break;

    case PARAM_PLAYBACK_RATE:
        Custom_Aleph_MonoVoice_set_playback_rate(&g_module.voice[voice_number],
                                                 value);
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
