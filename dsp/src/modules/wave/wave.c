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
 * @file    monosynth.c
 *
 * @brief   A monophonic synth module for Freetribe.
 */

/*----- Includes -----------------------------------------------------*/

#include <stdint.h>

#include "module.h"
#include "types.h"
#include "utils.h"

#include "aleph.h"
#include "osc_waves.h"
#include "wave_params.h"
#include "custom_aleph_monovoice.h"

void module_set_param_voice(uint16_t voice_index, uint16_t param_index,
                            int32_t value);

/*----- Macros -------------------------------------------------------*/

#define MEMPOOL_SIZE (0x2000)
#define FRACT32_MAX ((fract32)0x7fffffff) /* max value of a fract32 */
#define SDRAM_ADDRESS 0x00000000

static const fract32 wavtab[WAVE_SHAPE_NUM][WAVE_TAB_SIZE] = {
#include "wavtab_data_inc.data"
};
//   fract32 (*wavtab)[WAVE_TAB_SIZE] ;//= (fract32 (*)[WAVE_TAB_SIZE])SDRAM_ADDRESS;


/// TODO: Struct for parameter type.
///         scaler,
///         range,
///         default,
///         display,
///         etc...,

// #define DEFAULT_CUTOFF 0x7f // Index in pitch LUT.
#define DEFAULT_OSC_TYPE 2
#define MAX_VOICES 1
#define PARAM_VOICE_OFFSET(voice, param, paramCount)                           \
    (param + (voice * paramCount))
#define REMOVE_PARAM_OFFSET(param_index_with_offset, paramCount)               \
    ((param_index_with_offset) % paramCount)
#define PARAM_VOICE_NUMBER(param_index_with_offset, paramCount)                \
    ((param_index_with_offset) / paramCount)

#define WAVES_PM_DEL_SAMPS 0x400

/// TODO: Move to common location.
/**
 * @brief   Enumeration of module parameters.
 *
 * Index of each external parameter of module.
 */

/*----- Typedefs -----------------------------------------------------*/
#include "wave_typedefs.h"

/*----- Static variable definitions ----------------------------------*/

__attribute__((section(".l2.data.a")))
__attribute__((aligned(32))) static char g_mempool[MEMPOOL_SIZE];

//static t_Aleph g_aleph;
static t_module g_module;

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

static fract32 trash;
// additional busses
static fract32 voiceOut[MAX_VOICES] = { 0, 0, };
wavesVoice voice[MAX_VOICES];

static volatile fract32* patch_adc_dac[4][4];
static volatile fract32* patch_osc_dac[2][4];
#define IN_PORTS 4
#define OUT_PORTS 4
fract32 in[IN_PORTS];
fract32 out[OUT_PORTS];


// set adc_dac patch point
static inline void param_set_adc_patch(int i, int o, ParamValue v) {
  if(v > 0) { 
    patch_adc_dac[i][o] = &(out[o]);
  } else {
    patch_adc_dac[i][o] = &trash;
  }
}

static inline void param_set_osc_patch(int i, int o, ParamValue v) {
  if(v > 0) { 
    patch_osc_dac[i][o] = &(out[o]);
  } else {
    patch_osc_dac[i][o] = &trash;
  }
}

/**
 * @brief   Initialise module.
 */
void module_init(void) {
    int i;

    for (i = 0; i < MAX_VOICES; i++) {
        fract32 tmp = FRACT32_MAX >> 2;
        osc_init(&(g_module.voice[i].osc), &wavtab, SAMPLERATE);
        // filter_svf_init( &(g_module.voice[i].svf) );
        g_module.voice[i].amp = tmp;

        slew_init((g_module.voice[i].ampSlew), 0, 0, 0);
        slew_init((g_module.voice[i].cutSlew), 0, 0, 0);
        slew_init((g_module.voice[i].rqSlew), 0, 0, 0);

        slew_init((g_module.voice[i].wetSlew), 0, 0, 0);
        slew_init((g_module.voice[i].drySlew), 0, 0, 0);

        g_module.voice[i].modDelWrIdx = 0;
        g_module.voice[i].modDelRdIdx = 0;
    }

    // set parameters to defaults
    /// slew first
    module_set_param(eParamHz1Slew, PARAM_SLEW_DEFAULT);
    module_set_param(eParamHz0Slew, PARAM_SLEW_DEFAULT);
    module_set_param(eParamPm10Slew, PARAM_SLEW_DEFAULT);
    module_set_param(eParamPm01Slew, PARAM_SLEW_DEFAULT);
    module_set_param(eParamWm10Slew, PARAM_SLEW_DEFAULT);
    module_set_param(eParamWm01Slew, PARAM_SLEW_DEFAULT);
    module_set_param(eParamWave1Slew, PARAM_SLEW_DEFAULT);
    module_set_param(eParamWave0Slew, PARAM_SLEW_DEFAULT);
    module_set_param(eParamAmp1Slew, PARAM_SLEW_DEFAULT);
    module_set_param(eParamAmp0Slew, PARAM_SLEW_DEFAULT);

    module_set_param(eParamCut0Slew, PARAM_SLEW_DEFAULT);
    module_set_param(eParamCut1Slew, PARAM_SLEW_DEFAULT);
    module_set_param(eParamRq0Slew, PARAM_SLEW_DEFAULT);
    module_set_param(eParamRq1Slew, PARAM_SLEW_DEFAULT);

    module_set_param(eParamWet0Slew, PARAM_SLEW_DEFAULT);
    module_set_param(eParamWet1Slew, PARAM_SLEW_DEFAULT);
    module_set_param(eParamDry0Slew, PARAM_SLEW_DEFAULT);
    module_set_param(eParamDry1Slew, PARAM_SLEW_DEFAULT);
    module_set_param(eParamHz1, 220 << 16);
    module_set_param(eParamHz0, 330 << 16);
    module_set_param(eParamTune1, FIX16_ONE);
    module_set_param(eParamTune0, FIX16_ONE);
    module_set_param(eParamWave1, 0);
    module_set_param(eParamWave0, 0);
    module_set_param(eParamAmp1, PARAM_AMP_6);
    module_set_param(eParamAmp0, PARAM_AMP_6);

    module_set_param(eParamPm10, 0);
    module_set_param(eParamPm01, 0);
    module_set_param(eParamWm10, 0);
    module_set_param(eParamWm01, 0);
    /* module_set_param(  eParamBl1,  	0 ); */
    /* module_set_param(  eParamBl0,  	0 ); */

    module_set_param(eParam_cut1, PARAM_CUT_DEFAULT);
    module_set_param(eParam_rq1, PARAM_RQ_DEFAULT);
    module_set_param(eParam_mode1, 0);

    module_set_param(eParam_fwet1, PARAM_AMP_6);
    module_set_param(eParam_fdry1, PARAM_AMP_6);

    module_set_param(eParam_cut0, PARAM_CUT_DEFAULT);
    module_set_param(eParam_rq0, PARAM_RQ_DEFAULT);
    module_set_param(eParam_mode0, 0);

    module_set_param(eParam_fwet0, PARAM_AMP_6);
    module_set_param(eParam_fdry0, PARAM_AMP_6);

    module_set_param(eParam_adc0_dac0, 1);
    module_set_param(eParam_adc1_dac1, 1);
    module_set_param(eParam_adc2_dac2, 1);
    module_set_param(eParam_adc3_dac3, 1);

    module_set_param(eParam_osc0_dac0, 1);
    module_set_param(eParam_osc0_dac1, 1);
    module_set_param(eParam_osc0_dac2, 1);
    module_set_param(eParam_osc0_dac3, 1);
    module_set_param(eParam_osc1_dac0, 1);
    module_set_param(eParam_osc1_dac1, 1);
    module_set_param(eParam_osc1_dac2, 1);
    module_set_param(eParam_osc1_dac3, 1);

    module_set_param(eParam_cvVal0, FRACT32_MAX >> 1);
    module_set_param(eParam_cvVal1, FRACT32_MAX >> 1);
    module_set_param(eParam_cvVal2, FRACT32_MAX >> 1);
    module_set_param(eParam_cvVal3, FRACT32_MAX >> 1);
    module_set_param(eParam_cvSlew0, PARAM_SLEW_DEFAULT);
    module_set_param(eParam_cvSlew1, PARAM_SLEW_DEFAULT);
    module_set_param(eParam_cvSlew2, PARAM_SLEW_DEFAULT);
    module_set_param(eParam_cvSlew3, PARAM_SLEW_DEFAULT);
}

/**
 * @brief   Process audio.
 *
 * @param[in]   in  Pointer to input buffer.
 * @param[out]  out Pointer to input buffer.
 */
void module_process(fract32 *in, fract32 *out) {

    fract32 output;
    fract32 amp_level_scaled = g_module.amp_level / MAX_VOICES;

    // output = mult_fr1x32x32(Custom_Aleph_MonoVoice_next(&g_module.voice[0]),
    // amp_level_scaled);

    // Scale amplitude by level.
    // output = mult_fr1x32x32(output, g_module.amp_level);

    // Set output.
    out[0] = output;
    out[1] = output;
}

static inline void mix_voice (void) {
  /// can't handle the mix! use pointers for patch points right now
  int i, j;
  volatile fract32** pout = &(patch_osc_dac[0][0]);
  fract32* pin = voiceOut;
  for(i=0; i < 2; i++) {    
    for(j=0; j < 4; j++) {
      **pout = add_fr1x32(**pout, *pin);
      pout++;
    }
    pin++;
  }
}

static inline void mix_adc (void) {
  /// can't handle the mix! use pointers for patch points right now
  int i, j;
  volatile fract32** pout = &(patch_adc_dac[0][0]);
  fract32* pin = in;
  for(i=0; i < 4; i++) {    
    for(j=0; j < 4; j++) {
      **pout = add_fr1x32(**pout, *pin);
      pout++;
    }
    pin++;
  }
}
// frame calculation
static void calc_frame(void) {
  int i;
  wavesVoice* v = voice;
  fract32* vout = voiceOut;

  for(i=0; i<MAX_VOICES; i++) {
    // oscillator class includes hz and mod integrators
    v->oscOut = shr_fr1x32( osc_next( &(v->osc) ), 2);

    // set filter params
    /*slew32_calc(v->cutSlew);
    slew32_calc(v->rqSlew);
    filter_svf_set_coeff( &(v->svf), v->cutSlew.y );
    filter_svf_set_rq( &(v->svf), v->rqSlew.y );

    // process filter
    switch(svf_mode[i]) {
    case 0 :
      v->svfOut = filter_svf_lpf_next( &(v->svf), shr_fr1x32(v->oscOut, 1) );
      break;
    case 1 :
      v->svfOut = filter_svf_hpf_next( &(v->svf), shr_fr1x32(v->oscOut, 1) );
      break;
    case 2 :
      v->svfOut = filter_svf_bpf_next( &(v->svf), shr_fr1x32(v->oscOut, 1) );
      break;
    default :
      v->svfOut = filter_svf_lpf_next( &(v->svf), shr_fr1x32(v->oscOut, 1) );
      break;
    }*/
    // no filter 
    v->svfOut = v->oscOut; 

    // process amp/mix smoothing
    slew32_calc(v->ampSlew);
    slew16_calc(v->drySlew);
    slew16_calc(v->wetSlew);

    // mix dry/filter and apply amp
    *vout = mult_fr1x32x32(
				 v->ampSlew.y,
				 add_fr1x32(
					    mult_fr1x32( 
							trunc_fr1x32(v->oscOut), 
							v->drySlew.y
							 ),
					    mult_fr1x32( 
							trunc_fr1x32(v->svfOut), 
							v->wetSlew.y
							 )
					    )
				 );

    
    // advance phase del indices
    v->modDelWrIdx = (v->modDelWrIdx + 1) & WAVES_PM_DEL_SAMPS_1;
    v->modDelRdIdx = (v->modDelRdIdx + 1) & WAVES_PM_DEL_SAMPS_1;
    // set pm input from delay
    osc_pm_in(&(v->osc), v->modDelBuf[v->modDelRdIdx]);
    // no tricky modulation routing here!
    osc_wm_in(&(v->osc), v->modDelBuf[v->modDelRdIdx]);
    // advance pointers
    vout++;
    v++;
  } // end voice loop

  // // simple cross-patch modulation
  // add delay, before filter
  voice[0].modDelBuf[voice[0].modDelWrIdx] = voice[1].oscOut;
  voice[1].modDelBuf[voice[1].modDelWrIdx] = voice[0].oscOut;
  /* voice[0].pmIn = voice[1].oscOut; */
  /* voice[1].pmIn = voice[0].oscOut; */

  // zero the outputs
  out[0] = out[1] = out[2] = out[3] = 0;
  
  // patch filtered oscs outputs
  mix_voice();
  
  // patch adc
  mix_adc();
}

void module_set_param_voice(uint16_t voice_index, uint16_t param_index,
                            int32_t value) {
    uint16_t paramWithOffset =
        PARAM_VOICE_OFFSET(voice_index, param_index, PARAM_COUNT);
    module_set_param(paramWithOffset, value);
}

/**
 * @brief   Set parameter.
 *
 * @param[in]   param_index Index of parameter to set.
 * @param[in]   value       Value of parameter.
 */
void module_set_param(uint16_t param_index_with_offset, int32_t value) {

    uint16_t param_index =
        REMOVE_PARAM_OFFSET(param_index_with_offset, PARAM_COUNT);
    int voice_number = PARAM_VOICE_NUMBER(param_index_with_offset, PARAM_COUNT);
    // voice_number = 1;
    
    ParamValue v = value;

    switch (param_index) {

    case eParamHz1:
        osc_set_hz(&(g_module.voice[1].osc), v);
        break;
    case eParamHz0:
        osc_set_hz(&(g_module.voice[0].osc), v);
        break;

    case eParamTune1:
        osc_set_tune(&(g_module.voice[1].osc), v);
        break;
    case eParamTune0:
        osc_set_tune(&(g_module.voice[0].osc), v);
        break;

    case eParamWave1:
        g_module.voice[1].osc.shapeSlew.x = param_unit_to_fr16(v);
        break;
    case eParamWave0:
        g_module.voice[0].osc.shapeSlew.x = param_unit_to_fr16(v);
        break;

    case eParamPm10:
        osc_set_pm(&(g_module.voice[0].osc), param_unit_to_fr16(v));
        break;
    case eParamPm01:
        osc_set_pm(&(g_module.voice[1].osc), param_unit_to_fr16(v));
        break;

    case eParamFmDel0:
        param_set_pm_del(0, v);
        break;
    case eParamFmDel1:
        param_set_pm_del(1, v);
        break;

    case eParamWm10:
        osc_set_wm(&(g_module.voice[0].osc), param_unit_to_fr16(v));
        break;
    case eParamWm01:
        osc_set_wm(&(g_module.voice[1].osc), param_unit_to_fr16(v));
        break;

    case eParamAmp1:
        g_module.voice[1].ampSlew.x = v;
        break;
    case eParamAmp0:
        g_module.voice[0].ampSlew.x = v;
        break;

        //// filter params:
    case eParam_cut1:
        g_module.voice[1].cutSlew.x = v;
        break;
    case eParam_cut0:
        g_module.voice[0].cutSlew.x = v;
        break;
    case eParam_rq1:
        g_module.voice[1].rqSlew.x = v << 14;
        break;
    case eParam_rq0:
        g_module.voice[0].rqSlew.x = v << 14;
        break;
        /* case eParam_low1 : */
        /*   filter_svf_set_low(&(g_module.voice[1].svf), trunc_fr1x32(v)); */
        /*   break; */
        /* case eParam_low0 : */
        /*   filter_svf_set_low(&(g_module.voice[0].svf), trunc_fr1x32(v)); */
        /*   break; */
        /* case eParam_high1 : */
        /*   filter_svf_set_high(&(g_module.voice[1].svf), trunc_fr1x32(v)); */
        /*   break; */
        /* case eParam_high0 : */
        /*   filter_svf_set_high(&(g_module.voice[0].svf), trunc_fr1x32(v)); */
        /*   break; */
        /* case eParam_band1 : */
        /*   filter_svf_set_band(&(g_module.voice[1].svf), trunc_fr1x32(v)); */
        /*   break; */
        /* case eParam_band0 : */
        /*   filter_svf_set_band(&(g_module.voice[0].svf), trunc_fr1x32(v)); */
        /*   break; */
        /* case eParam_notch1 : */
        /*   filter_svf_set_notch(&(g_module.voice[1].svf), trunc_fr1x32(v)); */
        /*   break; */
        /* case eParam_notch0 : */
        /*   filter_svf_set_notch(&(g_module.voice[0].svf), trunc_fr1x32(v)); */
        /*   break; */

/*    case eParam_mode0:
        svf_mode[0] = v >> 16;
        break;
    case eParam_mode1:
        svf_mode[1] = v >> 16;
        break;*/

        // filter balance
    case eParam_fwet0:
        g_module.voice[0].wetSlew.x = trunc_fr1x32(v);
        break;

    case eParam_fwet1:
        g_module.voice[1].wetSlew.x = trunc_fr1x32(v);
        break;

    case eParam_fdry0:
        g_module.voice[0].drySlew.x = trunc_fr1x32(v);
        break;

    case eParam_fdry1:
        g_module.voice[1].drySlew.x = trunc_fr1x32(v);
        break;

        //----- slew parameters

        // oscillator param slew
    case eParamHz1Slew:
        g_module.voice[1].osc.incSlew.c = v;
        break;
    case eParamHz0Slew:
        g_module.voice[0].osc.incSlew.c = v;
        break;

    case eParamPm01Slew:
        g_module.voice[1].osc.pmSlew.c = param_fract_to_slew16(v);
        break;
    case eParamPm10Slew:
        g_module.voice[0].osc.pmSlew.c = param_fract_to_slew16(v);
        break;

    case eParamWm01Slew:
        g_module.voice[1].osc.wmSlew.c = param_fract_to_slew16(v);
        break;
    case eParamWm10Slew:
        g_module.voice[0].osc.wmSlew.c = param_fract_to_slew16(v);
        break;

    case eParamWave1Slew:
        g_module.voice[1].osc.shapeSlew.c = param_fract_to_slew16(v);
        break;
    case eParamWave0Slew:
        g_module.voice[0].osc.shapeSlew.c = param_fract_to_slew16(v);
        break;
    case eParamAmp1Slew:
        g_module.voice[1].ampSlew.c = v;
        break;
    case eParamAmp0Slew:
        g_module.voice[0].ampSlew.c = v;
        break;

        // other param slew
    case eParamCut0Slew:
        g_module.voice[0].cutSlew.c = v;
        break;
    case eParamCut1Slew:
        g_module.voice[1].cutSlew.c = v;
        break;

    case eParamRq0Slew:
        g_module.voice[0].rqSlew.c = v;
        break;
    case eParamRq1Slew:
        g_module.voice[1].rqSlew.c = v;
        break;

    case eParamWet0Slew:
        g_module.voice[0].wetSlew.c = param_fract_to_slew16(v);
        break;
    case eParamWet1Slew:
        g_module.voice[1].wetSlew.c = param_fract_to_slew16(v);
        break;

    case eParamDry0Slew:
        g_module.voice[0].drySlew.c = param_fract_to_slew16(v);
        break;
    case eParamDry1Slew:
        g_module.voice[1].rqSlew.c = param_fract_to_slew16(v);
        break;

        // cv values
    /*case eParam_cvVal0:
        filter_1p_lo_in(&(cvSlew[0]), v);
        break;
    case eParam_cvVal1:
        filter_1p_lo_in(&(cvSlew[1]), v);
        break;
    case eParam_cvVal2:
        filter_1p_lo_in(&(cvSlew[2]), v);
        break;
    case eParam_cvVal3:
        filter_1p_lo_in(&(cvSlew[3]), v);
        break;

    case eParam_cvSlew0:
        cvSlew[0].c = v;
        break;
    case eParam_cvSlew1:
        cvSlew[1].c = v;
        break;
    case eParam_cvSlew2:
        cvSlew[2].c = v;
        break;
    case eParam_cvSlew3:
        cvSlew[3].c = v;
        break;
*/
        // i/o mix:
    case eParam_adc0_dac0:
        param_set_adc_patch(0, 0, v);
        break;
    case eParam_adc0_dac1:
        param_set_adc_patch(0, 1, v);
        break;
    case eParam_adc0_dac2:
        param_set_adc_patch(0, 2, v);
        break;
    case eParam_adc0_dac3:
        param_set_adc_patch(0, 3, v);
        break;
    case eParam_adc1_dac0:
        param_set_adc_patch(1, 0, v);
        break;
    case eParam_adc1_dac1:
        param_set_adc_patch(1, 1, v);
        break;
    case eParam_adc1_dac2:
        param_set_adc_patch(1, 2, v);
        break;
    case eParam_adc1_dac3:
        param_set_adc_patch(1, 3, v);
        break;
    case eParam_adc2_dac0:
        param_set_adc_patch(2, 0, v);
        break;
    case eParam_adc2_dac1:
        param_set_adc_patch(2, 1, v);
        break;
    case eParam_adc2_dac2:
        param_set_adc_patch(2, 2, v);
        break;
    case eParam_adc2_dac3:
        param_set_adc_patch(2, 3, v);
        break;
    case eParam_adc3_dac0:
        param_set_adc_patch(3, 0, v);
        break;
    case eParam_adc3_dac1:
        param_set_adc_patch(3, 1, v);
        break;
    case eParam_adc3_dac2:
        param_set_adc_patch(3, 2, v);
        break;
    case eParam_adc3_dac3:
        param_set_adc_patch(3, 3, v);
        break;

        // osc mix:
    case eParam_osc0_dac0:
        param_set_osc_patch(0, 0, v);
        break;
    case eParam_osc0_dac1:
        param_set_osc_patch(0, 1, v);
        break;
    case eParam_osc0_dac2:
        param_set_osc_patch(0, 2, v);
        break;
    case eParam_osc0_dac3:
        param_set_osc_patch(0, 3, v);
        break;
    case eParam_osc1_dac0:
        param_set_osc_patch(1, 0, v);
        break;
    case eParam_osc1_dac1:
        param_set_osc_patch(1, 1, v);
        break;
    case eParam_osc1_dac2:
        param_set_osc_patch(1, 2, v);
        break;
    case eParam_osc1_dac3:
        param_set_osc_patch(1, 3, v);
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
