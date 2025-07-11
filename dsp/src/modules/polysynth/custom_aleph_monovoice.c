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
 * @file    Custom_Aleph_MonoVoice.c
 *
 * @brief   Monophonic synth voice module.
 */

/*----- Includes -----------------------------------------------------*/

#include "aleph.h"

#include "aleph_env_adsr.h"
#include "aleph_filter.h"
#include "aleph_filter_svf.h"
#include "aleph_lpf_one_pole.h"
#include "aleph_oscillator.h"
#include "aleph_waveform.h"

#include "custom_aleph_monovoice.h"
#include "common/config.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

void Custom_Aleph_MonoVoice_init(Custom_Aleph_MonoVoice *const synth, t_Aleph *const aleph) {

    Custom_Aleph_MonoVoice_init_to_pool(synth, &aleph->mempool);
}

void Custom_Aleph_MonoVoice_init_to_pool(Custom_Aleph_MonoVoice *const synth,
                                  Mempool *const mempool) {

    t_Mempool *mp = *mempool;

    t_Custom_Aleph_MonoVoice *syn = *synth =
        (t_Custom_Aleph_MonoVoice *)mpool_alloc(sizeof(t_Custom_Aleph_MonoVoice), mp);

    syn->mempool = mp;

    syn->freq_offset = Custom_Aleph_MonoVoice_DEFAULT_FREQ_OFFSET;
    syn->filter_type = Custom_Aleph_MonoVoice_DEFAULT_FILTER_TYPE;
    syn->filter_function = &Aleph_FilterSVF_lpf_next;

    int i;
    for (i = 0; i < MAX_UNISON_VOICES; i++) {
        Aleph_Waveform_init_to_pool(&syn->waveformSingle[i], mempool);
    }

    Aleph_FilterSVF_init_to_pool(&syn->filter, mempool);

    Aleph_HPF_init_to_pool(&syn->dc_block, mempool);

    Aleph_LPFOnePole_init_to_pool(&syn->freq_slew, mempool);
    Aleph_LPFOnePole_set_output(&syn->freq_slew, Custom_Aleph_MonoVoice_DEFAULT_FREQ);

    Aleph_LPFOnePole_init_to_pool(&syn->cutoff_slew, mempool);
    Aleph_LPFOnePole_set_output(&syn->cutoff_slew,
                                Custom_Aleph_MonoVoice_DEFAULT_CUTOFF);

    Aleph_LPFOnePole_init_to_pool(&syn->amp_slew, mempool);
    Aleph_LPFOnePole_set_output(&syn->amp_slew, Custom_Aleph_MonoVoice_DEFAULT_AMP);
}

void Custom_Aleph_MonoVoice_free(Custom_Aleph_MonoVoice *const synth) {

    t_Custom_Aleph_MonoVoice *syn = *synth;
int i;
        for (i = 0; i < MAX_UNISON_VOICES; i++) {
        Aleph_Waveform_free(&syn->waveformSingle[i]);
    }

    Aleph_FilterSVF_free(&syn->filter);

    Aleph_HPF_free(&syn->dc_block);

    Aleph_LPFOnePole_free(&syn->freq_slew);
    Aleph_LPFOnePole_free(&syn->cutoff_slew);

    mpool_free((char *)syn, syn->mempool);
}

fract32 Custom_Aleph_MonoVoice_next(Custom_Aleph_MonoVoice *const synth) {

    t_Custom_Aleph_MonoVoice *syn = *synth;
    fract32 output = 0;
    fract32 amp;
    fract32 freq;

    // Get slewed frequency.
    freq = Aleph_LPFOnePole_next(&syn->freq_slew);
    fract32 freq_with_offset = freq; // no offset for first voice

    int i;
    for (i = 0; i < MAX_UNISON_VOICES; i++) {
        // Set oscillator frequency.
        Aleph_Waveform_set_freq(&syn->waveformSingle[i], freq_with_offset);
        
        fract32 next = Aleph_Waveform_next(&syn->waveformSingle[i]);
        output = add_fr1x32(output, next);

        freq_with_offset = fix16_mul_fract(freq_with_offset, syn->freq_offset);
    }


    
    
    
    // a frequency in the middle of original and offset
    //fract32 freq_with_offset_2 = add_fr1x32(freq_with_offset>>1 , freq>>1);
    

//    Aleph_Waveform_set_freq(&syn->waveformSingle[1], freq_with_offset_2);
  //  Aleph_Waveform_set_freq(&syn->waveformSingle[2], freq_with_offset);

    
   // output = add_fr1x32(next_0, next_1);
    //output = next_a + next_b; // ring modulation? 
    


    // Shift right to prevent clipping.
    output = shr_fr1x32(output, 4);

    // Get slewed amplitude.
    amp = Aleph_LPFOnePole_next(&syn->amp_slew);
    // Apply amp modulation.
    output = mult_fr1x32x32(output, amp);

    // Apply filter to the generated signal
    #ifdef VOICE_MODE_POLYPHONIC
    output = Custom_Aleph_MonoVoice_apply_filter(synth, output);
    #endif

    // Block DC.
    // output = Aleph_HPF_dc_block(&syn->dc_block, output); // ??

    return output;
}

fract32 Custom_Aleph_MonoVoice_apply_filter(Custom_Aleph_MonoVoice *const synth, fract32 input_signal) {
    t_Custom_Aleph_MonoVoice *syn = *synth;

    fract32 output;
    fract32 cutoff;
    
    // Get slewed cutoff.
    cutoff = Aleph_LPFOnePole_next(&syn->cutoff_slew);

    // Set filter cutoff.
    Aleph_FilterSVF_set_coeff(&syn->filter, cutoff);

    output = syn->filter_function(&syn->filter, input_signal);
    
    return output;
}

void Custom_Aleph_MonoVoice_set_shape_a(Custom_Aleph_MonoVoice *const synth, e_Aleph_Waveform_shape shape) {
    t_Custom_Aleph_MonoVoice *syn = *synth;
    int i;
    for (i = 0; i < MAX_UNISON_VOICES; i++) {
        Aleph_Waveform_set_shape(&syn->waveformSingle[i], shape);
    }
}

void Custom_Aleph_MonoVoice_set_shape_b(Custom_Aleph_MonoVoice *const synth, e_Aleph_Waveform_shape shape) {
    t_Custom_Aleph_MonoVoice *syn = *synth;
    Aleph_Waveform_set_shape(&syn->waveformSingle[1], shape);
}


void Custom_Aleph_MonoVoice_set_amp(Custom_Aleph_MonoVoice *const synth, fract32 amp) {

    t_Custom_Aleph_MonoVoice *syn = *synth;

    Aleph_LPFOnePole_set_target(&syn->amp_slew, amp);
}

void Custom_Aleph_MonoVoice_set_phase(Custom_Aleph_MonoVoice *const synth, fract32 phase) {

    t_Custom_Aleph_MonoVoice *syn = *synth;
    int i;
    for (i = 0; i < MAX_UNISON_VOICES; i++) {
        Aleph_Waveform_set_phase(&syn->waveformSingle[i], phase);
    }

}

void Custom_Aleph_MonoVoice_set_freq(Custom_Aleph_MonoVoice *const synth, fract32 freq) {

    t_Custom_Aleph_MonoVoice *syn = *synth;

    Aleph_LPFOnePole_set_target(&syn->freq_slew, freq);
}

void Custom_Aleph_MonoVoice_set_freq_offset(Custom_Aleph_MonoVoice *const synth,
                                     fract32 freq_offset) {

    t_Custom_Aleph_MonoVoice *syn = *synth;

    syn->freq_offset = freq_offset;
}

void Custom_Aleph_MonoVoice_set_filter_type(Custom_Aleph_MonoVoice *const synth,
                                     e_Aleph_FilterSVF_type type) {

    t_Custom_Aleph_MonoVoice *syn = *synth;

    syn->filter_type = type; //filter function pointer
    switch (syn->filter_type) {

    case ALEPH_FILTERSVF_TYPE_LPF:
        syn->filter_function = &Aleph_FilterSVF_lpf_next;
        break;

    case ALEPH_FILTERSVF_TYPE_BPF:
        syn->filter_function = &Aleph_FilterSVF_bpf_next;
        break;

    case ALEPH_FILTERSVF_TYPE_HPF:
        syn->filter_function = &Aleph_FilterSVF_hpf_next;
        break;

    default:
        // Default to LPF.
        syn->filter_function = &Aleph_FilterSVF_lpf_next;
        break;
    }    


}

void Custom_Aleph_MonoVoice_set_cutoff(Custom_Aleph_MonoVoice *const synth, fract32 cutoff) {

    t_Custom_Aleph_MonoVoice *syn = *synth;

    Aleph_LPFOnePole_set_target(&syn->cutoff_slew, cutoff);
}

void Custom_Aleph_MonoVoice_set_res(Custom_Aleph_MonoVoice *const synth, fract32 res) {

    t_Custom_Aleph_MonoVoice *syn = *synth;

    Aleph_FilterSVF_set_rq(&syn->filter, res);
}




/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
