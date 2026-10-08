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
 * @file    geon_interface.c
 *
 * @brief   CPU interface to the Geonkick DSP module.
 */

/*----- Includes -----------------------------------------------------*/

#include <math.h>
#include <stdint.h>

#include "freetribe.h"

#include "geon_interface.h"

/*----- Macros -------------------------------------------------------*/

#define MODULE_ID 0

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static uint32_t _fix16(float value);
static uint32_t _milli(float value);

/*----- Extern function implementations ------------------------------*/

/**
 * @brief   Send a raw message to the DSP module.
 */
void geon_send(uint8_t instrument, e_param param, uint8_t target,
               uint32_t value) {

    ft_set_module_param(MODULE_ID, GEON_PARAM_BASE + param,
                        (int32_t)GEON_MESSAGE(instrument, target, value));
}

void geon_note_on(uint8_t instrument) {

    geon_send(instrument, PARAM_NOTE_ON, 0, 0);
}

void geon_all_notes_off(void) {

    ft_set_module_param(MODULE_ID, GEON_PARAM_BASE + PARAM_ALL_NOTES_OFF, 0);
}

void geon_set_length(uint8_t instrument, uint16_t milliseconds) {

    geon_send(instrument, PARAM_INSTRUMENT_LENGTH, 0, milliseconds);
}

void geon_set_amplitude(uint8_t instrument, float amplitude) {

    geon_send(instrument, PARAM_INSTRUMENT_AMPLITUDE, 0, _fix16(amplitude));
}

void geon_set_osc_enabled(uint8_t instrument, uint8_t osc, bool enable) {

    geon_send(instrument, PARAM_OSC_ENABLED, osc | (enable ? 0x08 : 0x00), 0);
}

void geon_set_osc_func(uint8_t instrument, uint8_t osc,
                       e_geon_osc_func func) {

    geon_send(instrument, PARAM_OSC_FUNC, osc, (uint32_t)func << 16);
}

void geon_set_osc_frequency(uint8_t instrument, uint8_t osc, float hz) {

    geon_send(instrument, PARAM_OSC_FREQUENCY, osc, _milli(hz));
}

void geon_set_osc_amplitude(uint8_t instrument, uint8_t osc, float amplitude) {

    geon_send(instrument, PARAM_OSC_AMPLITUDE, osc, _fix16(amplitude));
}

void geon_set_osc_noise_density(uint8_t instrument, uint8_t osc, float density) {

    geon_send(instrument, PARAM_OSC_NOISE_DENSITY, osc, _fix16(density));
}

void geon_set_osc_fm(uint8_t instrument, uint8_t osc, bool is_fm) {

    geon_send(instrument, PARAM_OSC_FM, osc | (is_fm ? 0x08 : 0x00), 0);
}

void geon_set_osc_filter_enabled(uint8_t instrument, uint8_t osc, bool enable) {

    geon_send(instrument, PARAM_OSC_FILTER_ENABLED,
              osc | (enable ? 0x08 : 0x00), 0);
}

void geon_set_osc_filter_type(uint8_t instrument, uint8_t osc,
                              e_geon_filter_type type) {

    geon_send(instrument, PARAM_OSC_FILTER_TYPE, osc, (uint32_t)type << 16);
}

void geon_set_osc_filter_cutoff(uint8_t instrument, uint8_t osc, float hz) {

    geon_send(instrument, PARAM_OSC_FILTER_CUTOFF, osc, _milli(hz));
}

void geon_set_osc_filter_q(uint8_t instrument, uint8_t osc, float q) {

    geon_send(instrument, PARAM_OSC_FILTER_Q, osc, _milli(q));
}

void geon_set_filter_enabled(uint8_t instrument, bool enable) {

    geon_send(instrument, PARAM_INSTRUMENT_FILTER_ENABLED, enable ? 1 : 0, 0);
}

void geon_set_filter_type(uint8_t instrument, e_geon_filter_type type) {

    geon_send(instrument, PARAM_INSTRUMENT_FILTER_TYPE, (uint8_t)type, 0);
}

void geon_set_filter_cutoff(uint8_t instrument, float hz) {

    geon_send(instrument, PARAM_INSTRUMENT_FILTER_CUTOFF, 0, _milli(hz));
}

void geon_set_filter_q(uint8_t instrument, float q) {

    geon_send(instrument, PARAM_INSTRUMENT_FILTER_Q, 0, _milli(q));
}

void geon_set_distortion_enabled(uint8_t instrument, bool enable) {

    geon_send(instrument, PARAM_DISTORTION_ENABLED, enable ? 1 : 0, 0);
}

void geon_set_distortion_drive(uint8_t instrument, float drive) {

    geon_send(instrument, PARAM_DISTORTION_DRIVE, 0, _milli(drive));
}

void geon_set_distortion_volume(uint8_t instrument, float volume) {

    geon_send(instrument, PARAM_DISTORTION_VOLUME, 0, _fix16(volume));
}

/*----- Static function implementations ------------------------------*/

/**
 * @brief   Value scaled to fill the 24 bit value field.
 *
 * The DSP reads the field as value / 2^24, so this must scale by 2^24.
 * Values are clamped to the field range and never negative, the DSP
 * stores all of them in float members.
 */
static uint32_t _fix16(float value) {

    if (value < 0.0f) {
        value = 0.0f;

    } else if (value > 1.0f) {
        value = 1.0f;
    }

    return (uint32_t)(value * 16777215.0f) & 0x00FFFFFF;
}

/**
 * @brief   Value as milli units in the low 24 bits of the message.
 *
 * Clamped so that the value cannot wrap the 24 bit field.
 */
static uint32_t _milli(float value) {

    if (value < 0.0f) {
        value = 0.0f;
    }

    if (value > 16000.0f) {
        value = 16000.0f;
    }

    return (uint32_t)(value * 1000.0f) & 0x00FFFFFF;
}

/*----- End of file --------------------------------------------------*/
