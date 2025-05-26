/*----------------------------------------------------------------------

                     This file is part of Aleph DSP

                https://github.com/bangcorrupt/aleph-dsp

         Aleph DSP is based on monome/aleph and spiricom/LEAF.

                              MIT License

            Aleph dedicated to the public domain by monome.

                LEAF Copyright Jeff Snyder et. al. 2020

                       Copyright bangcorrupt 2024

----------------------------------------------------------------------*/

/**
 * @file    Aleph_MonoVoice_single_osc.h
 *
 * @brief   Public API for monovoice module.
 */

#ifndef Aleph_MonoVoice_single_osc_H
#define Aleph_MonoVoice_single_osc_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include "aleph.h"

#include "aleph_filter.h"
#include "aleph_filter_svf.h"
#include "aleph_lpf_one_pole.h"
#include "aleph_waveform.h"

/*----- Macros -------------------------------------------------------*/

#define Aleph_MonoVoice_single_osc_DEFAULT_AMP (0)
#define Aleph_MonoVoice_single_osc_DEFAULT_FREQ (220 << 16)
#define Aleph_MonoVoice_single_osc_DEFAULT_FREQ_OFFSET (0)

#define Aleph_MonoVoice_single_osc_DEFAULT_CUTOFF (20000 << 16)
#define Aleph_MonoVoice_single_osc_DEFAULT_RES (FR32_MAX)
#define Aleph_MonoVoice_single_osc_DEFAULT_FILTER_TYPE ALEPH_FILTERSVF_TYPE_LPF

/*----- Typedefs -----------------------------------------------------*/

typedef struct {

    Mempool mempool;

    Aleph_Waveform waveform;
    fract32 freq_offset;

    Aleph_FilterSVF filter;
    e_Aleph_FilterSVF_type filter_type;

    Aleph_LPFOnePole amp_slew;
    Aleph_LPFOnePole freq_slew;
    Aleph_LPFOnePole cutoff_slew;

    Aleph_HPF dc_block;

} t_Aleph_MonoVoice_single_osc;

typedef t_Aleph_MonoVoice_single_osc *Aleph_MonoVoice_single_osc;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void Aleph_MonoVoice_single_osc_init(Aleph_MonoVoice_single_osc *const synth, t_Aleph *const aleph);
void Aleph_MonoVoice_single_osc_init_to_pool(Aleph_MonoVoice_single_osc *const synth,
                                  Mempool *const mempool);
void Aleph_MonoVoice_single_osc_free(Aleph_MonoVoice_single_osc *const synth);

fract32 Aleph_MonoVoice_single_osc_next(Aleph_MonoVoice_single_osc *const synth);

void Aleph_MonoVoice_single_osc_set_shape(Aleph_MonoVoice_single_osc *const synth,
                               e_Aleph_Waveform_shape shape);

void Aleph_MonoVoice_single_osc_set_amp(Aleph_MonoVoice_single_osc *const synth, fract32 amp);
void Aleph_MonoVoice_single_osc_set_phase(Aleph_MonoVoice_single_osc *const synth, fract32 phase);

void Aleph_MonoVoice_single_osc_set_freq(Aleph_MonoVoice_single_osc *const synth, fract32 freq);
void Aleph_MonoVoice_single_osc_set_freq_offset(Aleph_MonoVoice_single_osc *const synth,
                                     fract32 freq_offset);

void Aleph_MonoVoice_single_osc_set_filter_type(Aleph_MonoVoice_single_osc *const synth,
                                     e_Aleph_FilterSVF_type type);

void Aleph_MonoVoice_single_osc_set_cutoff(Aleph_MonoVoice_single_osc *const synth, fract32 cutoff);
void Aleph_MonoVoice_single_osc_set_res(Aleph_MonoVoice_single_osc *const synth, fract32 res);

void Aleph_MonoVoice_single_osc_set_amp_slew(Aleph_MonoVoice_single_osc *const synth,
                                  fract32 amp_slew);

void Aleph_MonoVoice_single_osc_set_freq_slew(Aleph_MonoVoice_single_osc *const synth,
                                   fract32 freq_slew);

void Aleph_MonoVoice_single_osc_set_cutoff_slew(Aleph_MonoVoice_single_osc *const synth,
                                     fract32 cutoff_slew);

#ifdef __cplusplus
}
#endif
#endif

/*----- End of file --------------------------------------------------*/
