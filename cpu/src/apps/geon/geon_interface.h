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
 * @file    geon_interface.h
 *
 * @brief   Public API for CPU interface to Geonkick DSP module.
 *
 * Follows the monosynth example: the CPU owns the envelopes, LFOs and
 * all control state, and sends the DSP the values it needs only when
 * they change.  This header is the single definition of the protocol
 * between the two, the parameter indices must match e_param in
 * dsp/src/modules/geon/geon.c.
 */

#ifndef GEON_INTERFACE_H
#define GEON_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

/*----- Macros -------------------------------------------------------*/

/**
 * @brief   DSP module parameter base.
 *
 * Messages below this index are not handled by the Geonkick module.
 */
#define GEON_PARAM_BASE 0x80

/**
 * @brief   Layout of the 32 bit message sent as a parameter value.
 *
 *   bits 31-28  instrument index, 0...15
 *   bits 27-24  target, oscillator or filter selector
 *   bits 23-00  value
 *
 * Values are a fraction in the range 0...1 scaled to fill bits 23-0, an
 * unsigned value in bits 23-0, or a small enumerator in bits 23-16.
 * See doc/protocol.md for the full encoding.
 */
#define GEON_MESSAGE(instrument, target, value)                            \
    ((((uint32_t)(instrument)&0x0F) << 28) |                               \
     (((uint32_t)(target)&0x0F) << 24) | ((uint32_t)(value)&0x00FFFFFF))

#define GEON_INSTRUMENTS 16
#define GEON_OSCILLATORS 2

/**
 * @brief   Maximum instrument length in milliseconds.
 */
#define GEON_MAX_LENGTH_MS 2000

/**
 * @brief   Oscillator waveform.
 */
typedef enum {
    GEON_OSC_FUNC_SINE = 0,
    GEON_OSC_FUNC_SQUARE,
    GEON_OSC_FUNC_TRIANGLE,
    GEON_OSC_FUNC_SAWTOOTH,
    GEON_OSC_FUNC_NOISE_WHITE,
    GEON_OSC_FUNC_NOISE_BROWNIAN,

    GEON_OSC_FUNC_COUNT
} e_geon_osc_func;

/**
 * @brief   Filter type.
 */
typedef enum {
    GEON_FILTER_LOW_PASS = 0,
    GEON_FILTER_HIGH_PASS,
    GEON_FILTER_BAND_PASS,

    GEON_FILTER_TYPE_COUNT
} e_geon_filter_type;

/**
 * @brief   Per oscillator envelope, evaluated on the CPU.
 */
#define GEON_ENV_AMPLITUDE 0
#define GEON_ENV_FREQUENCY 1
#define GEON_ENV_NOISE_DENSITY 2
#define GEON_ENV_COUNT 3

/**
 * @brief   Parameter indices, must match e_param in the DSP module.
 */
typedef enum {
    PARAM_INSTRUMENT_LENGTH = 0,
    PARAM_INSTRUMENT_AMPLITUDE,
    PARAM_INSTRUMENT_FILTER_ENABLED,
    PARAM_INSTRUMENT_FILTER_TYPE,
    PARAM_INSTRUMENT_FILTER_CUTOFF,
    PARAM_INSTRUMENT_FILTER_Q,
    PARAM_DISTORTION_ENABLED,
    PARAM_DISTORTION_DRIVE,
    PARAM_DISTORTION_VOLUME,
    PARAM_OSC_ENABLED,
    PARAM_OSC_FUNC,
    PARAM_OSC_FREQUENCY,
    PARAM_OSC_AMPLITUDE,
    PARAM_OSC_NOISE_DENSITY,
    PARAM_OSC_FM,
    PARAM_OSC_FILTER_ENABLED,
    PARAM_OSC_FILTER_TYPE,
    PARAM_OSC_FILTER_CUTOFF,
    PARAM_OSC_FILTER_Q,
    PARAM_NOTE_ON,
    PARAM_ALL_NOTES_OFF,

    PARAM_COUNT
} e_param;

/*----- Typedefs -----------------------------------------------------*/

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void geon_send(uint8_t instrument, e_param param, uint8_t target,
               uint32_t value);

void geon_note_on(uint8_t instrument);
void geon_all_notes_off(void);

void geon_set_length(uint8_t instrument, uint16_t milliseconds);
void geon_set_amplitude(uint8_t instrument, float amplitude);

void geon_set_osc_enabled(uint8_t instrument, uint8_t osc, bool enable);
void geon_set_osc_func(uint8_t instrument, uint8_t osc,
                       e_geon_osc_func func);
void geon_set_osc_frequency(uint8_t instrument, uint8_t osc, float hz);
void geon_set_osc_amplitude(uint8_t instrument, uint8_t osc, float amplitude);
void geon_set_osc_noise_density(uint8_t instrument, uint8_t osc, float density);
void geon_set_osc_fm(uint8_t instrument, uint8_t osc, bool is_fm);

void geon_set_osc_filter_enabled(uint8_t instrument, uint8_t osc, bool enable);
void geon_set_osc_filter_type(uint8_t instrument, uint8_t osc,
                              e_geon_filter_type type);
void geon_set_osc_filter_cutoff(uint8_t instrument, uint8_t osc, float hz);
void geon_set_osc_filter_q(uint8_t instrument, uint8_t osc, float q);

void geon_set_filter_enabled(uint8_t instrument, bool enable);
void geon_set_filter_type(uint8_t instrument, e_geon_filter_type type);
void geon_set_filter_cutoff(uint8_t instrument, float hz);
void geon_set_filter_q(uint8_t instrument, float q);

void geon_set_distortion_enabled(uint8_t instrument, bool enable);
void geon_set_distortion_drive(uint8_t instrument, float drive);
void geon_set_distortion_volume(uint8_t instrument, float volume);

#ifdef __cplusplus
}
#endif
#endif /* GEON_INTERFACE_H */

/*----- End of file --------------------------------------------------*/
