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
 * @file    geon.c
 *
 * @brief   Geonkick percussion synthesizer module for Freetribe.
 *
 * A port of the Geonkick DSP[1] to the Blackfin used by the Electribe 2,
 * following the monosynth example: the CPU owns the envelopes, LFOs and
 * every other parameter, and sends values to this module only when they
 * change.  What lives here is only the state a sample needs.
 *
 * [1] https://github.com/Geonkick-Synthesizer/geonkick
 *     Copyright (C) 2017 Iurie Nistor, GPL-3.0-or-later.
 */

/*----- Includes -----------------------------------------------------*/

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include "fract_math.h"
#include "types.h"

#include "module.h"
#include "utils.h"

#include "geon_filter.h"
#include "geon_instrument.h"
#include "geon_types.h"

/*----- Macros -------------------------------------------------------*/

#define GEON_PARAM_BASE 0x80

/**
 * @brief   Layout of the 32 bit message used to set parameters.
 *
 *   bits 31-28  instrument index, 0...15
 *   bits 27-24  target, oscillator, layer or filter selector
 *   bits 23-00  value
 */
#define GEON_INSTRUMENT_SHIFT 28
#define GEON_INSTRUMENT_MASK 0xF0000000

#define GEON_TARGET_SHIFT 24
#define GEON_TARGET_MASK 0x0F000000

#define GEON_VALUE_MASK 0x00FFFFFF

/**
 * @brief   Instrument output gain, several percussion voices add up
 *          fast.
 */
#define GEON_MASTER_GAIN 0.35f

/*----- Typedefs -----------------------------------------------------*/

/**
 * @brief   Enumeration of module parameters.
 *
 * Index of each external parameter of module.  Every message carries
 * an instrument index in bits 31-28, a target in bits 27-24 and a
 * value in bits 23-0, see geon_interface.h in cpu/src/apps/geon.
 */
typedef enum {

    PARAM_INSTRUMENT_LENGTH = 0, ///< Length, as milliseconds.
    PARAM_INSTRUMENT_AMPLITUDE,  ///< Amplitude, 0...1.
    PARAM_INSTRUMENT_FILTER_ENABLED,          ///< Global filter enable.
    PARAM_INSTRUMENT_FILTER_TYPE,             ///< Global filter type.
    PARAM_INSTRUMENT_FILTER_CUTOFF,           ///< Cutoff, as millihertz.
    PARAM_INSTRUMENT_FILTER_Q,                ///< Q, as milli units.
    PARAM_DISTORTION_ENABLED,                 ///< Distortion enable.
    PARAM_DISTORTION_DRIVE,                   ///< Drive, as milli units.
    PARAM_DISTORTION_VOLUME,                  ///< Volume, 0...1.
    PARAM_OSC_ENABLED,                        ///< Oscillator enable.
    PARAM_OSC_FUNC,                           ///< Oscillator waveform.
    PARAM_OSC_FREQUENCY,                      ///< Frequency, as millihertz.
    PARAM_OSC_AMPLITUDE,                      ///< Amplitude, 0...1.
    PARAM_OSC_NOISE_DENSITY,                  ///< Noise density, 0...1.
    PARAM_OSC_FM,                             ///< Oscillator feeds the next.
    PARAM_OSC_FILTER_ENABLED,                 ///< Oscillator filter enable.
    PARAM_OSC_FILTER_TYPE,                    ///< Oscillator filter type.
    PARAM_OSC_FILTER_CUTOFF,                  ///< Cutoff, as millihertz.
    PARAM_OSC_FILTER_Q,                       ///< Q, as milli units.
    PARAM_NOTE_ON,                            ///< Trigger the instrument.
    PARAM_ALL_NOTES_OFF,                      ///< Stop all instruments.

    PARAM_COUNT
} e_param;

typedef struct {
    t_geon_instrument instruments[GEON_INSTRUMENTS];
    float gain;
} t_module;

/*----- Static variable definitions ----------------------------------*/

static t_module g_module;

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static uint8_t _index(uint32_t message);
static uint8_t _target(uint32_t message);
static float _fix16(uint32_t message);
static uint32_t _uint24(uint32_t message);
static float _milli(uint32_t message);
static uint8_t _byte(uint32_t message);
static e_geon_osc_func _osc_func(uint8_t value);
static e_geon_filter_type _filter_type(uint8_t value);
static t_geon_oscillator *_get_oscillator(t_geon_instrument *instrument,
                                          uint8_t target);
static void _apply_instrument(t_geon_instrument *instrument, uint16_t param,
                              uint32_t message);

/*----- Extern function implementations ------------------------------*/

/**
 * @brief   Initialise module.
 */
void module_init(void) {

    uint8_t i;

    for (i = 0; i < GEON_INSTRUMENTS; i++) {
        geon_instrument_init(&g_module.instruments[i], SAMPLERATE);
    }

    g_module.gain = GEON_MASTER_GAIN;
}

/**
 * @brief   Process audio.
 *
 * @param[in]   in  Pointer to input buffer.
 * @param[out]  out Pointer to input buffer.
 */
void module_process(fract32 *in, fract32 *out) {

    float mix = 0.0f;
    float sample;
    bool active;
    uint8_t i;
    int32_t value;
    t_geon_instrument *instrument;

    (void)in;

    for (i = 0; i < GEON_INSTRUMENTS; i++) {

        instrument = &g_module.instruments[i];

        if (!instrument->enabled) {
            continue;
        }

        active = geon_instrument_process(instrument, &sample);

        if (!active) {
            instrument->enabled = false;
        }

        mix += sample;
    }

    mix *= g_module.gain;

    if (isnan(mix)) {
        mix = 0.0f;

    } else {
        mix = geon_clamp(mix, -1.0f, 1.0f);
    }

    value = (int32_t)(mix * 2147483520.0f);

    out[0] = value;
    out[1] = value;
}

/**
 * @brief   Set parameter.
 *
 * @param[in]   param_index Index of parameter to set.
 * @param[in]   value       Value of parameter.
 */
void module_set_param(uint16_t param_index, int32_t value) {

    uint32_t message = (uint32_t)value;

    uint8_t instrument_index;
    uint16_t param;
    uint8_t i;

    /* Messages below GEON_PARAM_BASE are not ours. */
    if (param_index < GEON_PARAM_BASE) {
        return;
    }

    param = param_index - GEON_PARAM_BASE;

    if (param >= PARAM_COUNT) {
        return;
    }

    /* Global messages, without an instrument index. */
    if (param == PARAM_ALL_NOTES_OFF) {

        for (i = 0; i < GEON_INSTRUMENTS; i++) {
            g_module.instruments[i].enabled = false;
        }

        return;
    }

    instrument_index = _index(message);

    if (instrument_index >= GEON_INSTRUMENTS) {
        return;
    }

    _apply_instrument(&g_module.instruments[instrument_index], param, message);
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

    case PARAM_INSTRUMENT_LENGTH:
        copy_string(text, "Length", MAX_PARAM_NAME_LENGTH);
        break;

    case PARAM_INSTRUMENT_AMPLITUDE:
        copy_string(text, "Amp", MAX_PARAM_NAME_LENGTH);
        break;

    case PARAM_OSC_FUNC:
        copy_string(text, "Wave", MAX_PARAM_NAME_LENGTH);
        break;

    case PARAM_OSC_FREQUENCY:
        copy_string(text, "Freq", MAX_PARAM_NAME_LENGTH);
        break;

    case PARAM_INSTRUMENT_FILTER_CUTOFF:
        copy_string(text, "Cutoff", MAX_PARAM_NAME_LENGTH);
        break;

    case PARAM_INSTRUMENT_FILTER_Q:
        copy_string(text, "Res", MAX_PARAM_NAME_LENGTH);
        break;

    case PARAM_DISTORTION_DRIVE:
        copy_string(text, "Drive", MAX_PARAM_NAME_LENGTH);
        break;

    default:
        copy_string(text, "Unknown", MAX_PARAM_NAME_LENGTH);
        break;
    }
}

/*----- Static function implementations ------------------------------*/

/**
 * @brief   Instrument index, bits 31-28 of the message.
 */
static uint8_t _index(uint32_t message) {

    return (uint8_t)((message & GEON_INSTRUMENT_MASK) >> GEON_INSTRUMENT_SHIFT);
}

/**
 * @brief   Target, bits 27-24 of the message.
 */
static uint8_t _target(uint32_t message) {

    return (uint8_t)((message & GEON_TARGET_MASK) >> GEON_TARGET_SHIFT);
}

/**
 * @brief   Value, bits 23-0, as an unsigned fixed point fraction.
 *
 * The CPU sends a positive value scaled to fill 24 bits, so the full
 * scale is 2^24.
 */
static float _fix16(uint32_t message) {

    return (float)_uint24(message) / 16777216.0f;
}

/**
 * @brief   Value, bits 23-0, as an unsigned integer.
 */
static uint32_t _uint24(uint32_t message) {

    return message & GEON_VALUE_MASK;
}

/**
 * @brief   Value, bits 23-0, as milli units, e.g. millihertz.
 */
static float _milli(uint32_t message) {

    return (float)_uint24(message) / 1000.0f;
}

/**
 * @brief   Value, bits 23-16, as an unsigned byte.
 */
static uint8_t _byte(uint32_t message) {

    return (uint8_t)((message & 0x00FF0000) >> 16);
}

static e_geon_osc_func _osc_func(uint8_t value) {

    if (value >= GEON_OSC_FUNC_COUNT) {
        return GEON_OSC_FUNC_SINE;
    }

    return (e_geon_osc_func)value;
}

static e_geon_filter_type _filter_type(uint8_t value) {

    if (value >= GEON_FILTER_TYPE_COUNT) {
        return GEON_FILTER_LOW_PASS;
    }

    return (e_geon_filter_type)value;
}

/**
 * @brief   Oscillator, from the target field, bit 0.
 */
static t_geon_oscillator *_get_oscillator(t_geon_instrument *instrument,
                                          uint8_t target) {

    uint8_t osc = target & 0x01;

    if (osc >= GEON_OSCILLATORS) {
        return NULL;
    }

    return &instrument->osc[osc];
}

/**
 * @brief   Apply one parameter message to an instrument.
 */
static void _apply_instrument(t_geon_instrument *instrument, uint16_t param,
                              uint32_t message) {

    t_geon_oscillator *osc;
    uint8_t target = _target(message);

    switch (param) {

    case PARAM_INSTRUMENT_LENGTH:
        instrument->length = geon_clamp((float)_uint24(message) / 1000.0f,
                                        0.001f, GEON_MAX_LENGTH);
        break;

    case PARAM_INSTRUMENT_AMPLITUDE:
        instrument->amplitude = geon_clamp(_fix16(message), 0.0f, 1.0f);
        break;

    case PARAM_INSTRUMENT_FILTER_ENABLED:
        instrument->filter_enabled = ((target & 0x01) != 0);
        break;

    case PARAM_INSTRUMENT_FILTER_TYPE:
        geon_filter_set_type(&instrument->filter, _filter_type(target));
        break;

    case PARAM_INSTRUMENT_FILTER_CUTOFF:
        geon_filter_set_cutoff(&instrument->filter,
                               geon_clamp(_milli(message), 20.0f, 20000.0f));
        break;

    case PARAM_INSTRUMENT_FILTER_Q:
        geon_filter_set_factor(&instrument->filter,
                               geon_clamp(_milli(message), 0.5f, 10.0f));
        break;

    case PARAM_DISTORTION_ENABLED:
        instrument->distortion_enabled = ((target & 0x01) != 0);
        break;

    case PARAM_DISTORTION_DRIVE:
        instrument->distortion_drive =
            geon_clamp(_milli(message), 0.0f, 100.0f);
        break;

    case PARAM_DISTORTION_VOLUME:
        instrument->distortion_volume =
            geon_clamp(_fix16(message), 0.0f, 1.0f);
        break;

    case PARAM_OSC_ENABLED:

        osc = _get_oscillator(instrument, target);

        if (osc != NULL) {
            osc->enabled = ((target & 0x08) != 0);
        }
        break;

    case PARAM_OSC_FUNC:

        osc = _get_oscillator(instrument, target);

        if (osc != NULL) {
            osc->func = _osc_func(_byte(message));
        }
        break;

    case PARAM_OSC_FREQUENCY:

        osc = _get_oscillator(instrument, target);

        if (osc != NULL) {
            osc->frequency = geon_clamp(_milli(message), 0.0f, 20000.0f);
        }
        break;

    case PARAM_OSC_AMPLITUDE:

        osc = _get_oscillator(instrument, target);

        if (osc != NULL) {
            osc->amplitude = geon_clamp(_fix16(message), 0.0f, 1.0f);
        }
        break;

    case PARAM_OSC_NOISE_DENSITY:

        osc = _get_oscillator(instrument, target);

        if (osc != NULL) {
            osc->noise_density = geon_clamp(_fix16(message), 0.0f, 1.0f);
        }
        break;

    case PARAM_OSC_FM:

        osc = _get_oscillator(instrument, target);

        if (osc != NULL) {
            osc->is_fm = ((target & 0x08) != 0);
        }
        break;

    case PARAM_OSC_FILTER_ENABLED:

        osc = _get_oscillator(instrument, target);

        if (osc != NULL) {
            osc->filter_enabled = ((target & 0x08) != 0);
        }
        break;

    case PARAM_OSC_FILTER_TYPE:

        osc = _get_oscillator(instrument, target);

        if (osc != NULL) {
            geon_filter_set_type(&osc->filter, _filter_type(_byte(message)));
        }
        break;

    case PARAM_OSC_FILTER_CUTOFF:

        osc = _get_oscillator(instrument, target);

        if (osc != NULL) {
            geon_filter_set_cutoff(&osc->filter,
                                   geon_clamp(_milli(message), 20.0f, 20000.0f));
        }
        break;

    case PARAM_OSC_FILTER_Q:

        osc = _get_oscillator(instrument, target);

        if (osc != NULL) {
            geon_filter_set_factor(&osc->filter,
                                   geon_clamp(_milli(message), 0.5f, 10.0f));
        }
        break;

    case PARAM_NOTE_ON:
        geon_instrument_trigger(instrument);
        instrument->enabled = true;
        break;

    default:
        break;
    }
}

/*----- End of file --------------------------------------------------*/
