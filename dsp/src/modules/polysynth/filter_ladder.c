/* filter_ladder.c
   audio
   aleph

   digital ladder filter for fract32 audio.
   optional oversampling for better frequency range.
 */

#include "module.h"
#include "fix.h"
#include "fract_math.h"
//#include "sin_fr32.h"
//#include "table.h"
#include "filter_ladder.h"
//#include "ricks_tricks.h"
#define TWO_PI_16_16 411775

// this ladder filter was designed on difference equation page 17 of
// Paul Daly's Masters thesis.  We use a different softclip algorithm
// to the tanh proposed there, which is more suited to blackfin
// primitives...
// http://www.acoustics.ed.ac.uk/wp-content/uploads/AMT_MSc_FinalProjects/2012__Daly__AMT_MSc_FinalProject_MoogVCF.pdf

static inline fract32 lpf_freq_calc(fract32 freq) {
    // 1.0 / ((1 / 2.0 * pi * dt * fc) + 1.0)
    fract32 temp = mult_fr1x32x32(TWO_PI_16_16, freq);
    return ((temp << 12) / ((1 << 16) + temp)) << 19;
}

extern void filter_ladder_init_to_pool (FilterLadder * const filter, Mempool *const mempool) {

    t_Mempool *mp = *mempool;

    filter_ladder *f = *filter =
        (filter_ladder *)mpool_alloc(sizeof(filter_ladder), mp);

    f->mempool = mp;

  int i;

  for(i=0; i < 4; i++) {
    f->filterStageOuts[i] = 0;
    f->filterStageLastIns[i] = 0;
  }
  f->clipLevel[0] = FR32_MAX >> 1;
  f->clipLevel[1] = FR32_MAX >> 2;
  f->clipLevel[2] = FR32_MAX - (FR32_MAX >> 2);
  f->clipLevel[3] = FR32_MAX >> 1;

  f->clipLevel[0] = FR32_MAX >> 2;
  f->clipLevel[1] = FR32_MAX >> 4;
  f->clipLevel[2] = FR32_MAX >> 2;
  f->clipLevel[3] = FR32_MAX >> 4;

  f->lastOutput = 0;
  f->alpha = lpf_freq_calc(1000 << 16);
  f->feedback = FR32_MAX >> 2;
  f->fbShift = 0;

}

// LTI version of 4-stage ladder topology
extern fract32 filter_ladder_lpf_next (FilterLadder * const filter, fract32 in) {
  filter_ladder *f = *filter;
  int i;
  fract32 outDel = add_fr1x32(shr_fr1x32(f->filterStageOuts[3], 1),
			      shr_fr1x32(f->lastOutput, 1));
  outDel = shl_fr1x32(mult_fr1x32x32(f->feedback, outDel),
		      2 - f->fbShift);;
  f->filterStageOuts[0]
    = add_fr1x32(f->filterStageOuts[0],
		 mult_fr1x32x32(f->alpha,
				sub_fr1x32(in,
					   add_fr1x32(outDel,
						      f->filterStageOuts[0]))));
  f->lastOutput = f->filterStageOuts[3];
  for(i=1; i < 4; i++) {
    f->filterStageOuts[i]
      = add_fr1x32(f->filterStageOuts[i],
		   mult_fr1x32x32(f->alpha,
				  sub_fr1x32(f->filterStageOuts[i-1],
					     f->filterStageOuts[i])));
  }
  return f->filterStageOuts[3];
}

// LTI version of 4-stage hpf ladder topology
// NOTE: not sure this actually works, leaving it here anyway...
extern fract32 filter_ladder_hpf_next (FilterLadder * const filter, fract32 in) {
  fract32 lpf_out = filter_ladder_lpf_next(filter, in);
  return sub_fr1x32(in, lpf_out); 
}

// oversampling version of the 4-stage LTI ladder
extern fract32 filter_ladder_lpf_os_next (FilterLadder * const filter, fract32 in) {
  filter_ladder *f = *filter;
  fract32 lastIn_os = add_fr1x32(shr_fr1x32(in, 1),
				 shr_fr1x32(f->lastInput, 1));

  fract32 out = shr_fr1x32(filter_ladder_lpf_next(&f, lastIn_os), 1);
  out = add_fr1x32(out, shr_fr1x32(filter_ladder_lpf_next(&f, in), 1));
  f->lastInput = in;
  return out;
}

// symettrical softclipping version of 4-stage ladder topology
extern fract32 filter_ladder_lpf_softclip_next (FilterLadder * const filter, fract32 in) {
  int i;
filter_ladder *f = *filter;
  fract32 outDel = add_fr1x32(shr_fr1x32(f->filterStageOuts[3], 1),
			      shr_fr1x32(f->lastOutput, 1));
  outDel = shl_fr1x32(mult_fr1x32x32(f->feedback, outDel),
		      2 - f->fbShift);;
  f->filterStageOuts[0]
    = add_fr1x32(f->filterStageOuts[0],
		 mult_fr1x32x32(f->alpha,
				sub_fr1x32(in,
					   add_fr1x32(outDel,
						      f->filterStageOuts[0]))));
  f->filterStageOuts[0] = soft_clip (f->clipLevel[0], f->filterStageOuts[0]);
  f->lastOutput = f->filterStageOuts[3];
  for(i=1; i < 4; i++) {
    f->filterStageOuts[i]
      = add_fr1x32(f->filterStageOuts[i],
		   mult_fr1x32x32(f->alpha,
				  sub_fr1x32(f->filterStageOuts[i-1],
					     f->filterStageOuts[i])));
    f->filterStageOuts[i] = soft_clip (f->clipLevel[i], f->filterStageOuts[i]);
  }
  return f->filterStageOuts[3];
}

// oversampling version of the 4-stage symmetrical softclipping ladder
extern fract32 filter_ladder_lpf_softclip_os_next (FilterLadder * const filter, fract32 in) {
filter_ladder *f = *filter;
  fract32 lastIn_os = add_fr1x32(shr_fr1x32(in, 1),
				 shr_fr1x32(f->lastInput, 1));

  fract32 out = shr_fr1x32(filter_ladder_lpf_softclip_next(&f, lastIn_os), 1);
  out = add_fr1x32(out, shr_fr1x32(filter_ladder_lpf_softclip_next(&f, in), 1));
  f->lastInput = in;
  return out;
}

// symettrical softclipping version of 4-stage ladder topology
extern fract32 filter_ladder_lpf_asym_next (FilterLadder * const filter, fract32 in) {
filter_ladder *f = *filter;
  int i;
  fract32 outDel = add_fr1x32(shr_fr1x32(f->filterStageOuts[3], 1),
			      shr_fr1x32(f->lastOutput, 1));
  outDel = shl_fr1x32(mult_fr1x32x32(f->feedback, outDel),
		      2 - f->fbShift);;
  f->filterStageOuts[0]
    = add_fr1x32(f->filterStageOuts[0],
		 mult_fr1x32x32(f->alpha,
				sub_fr1x32(in,
					   add_fr1x32(outDel,
						      f->filterStageOuts[0]))));
  f->filterStageOuts[0] = soft_clip_asym (f->clipLevel[0], f->clipLevelNeg[0],
					  f->filterStageOuts[0]);
  f->lastOutput = f->filterStageOuts[3];
  for(i=1; i < 4; i++) {
    f->filterStageOuts[i]
      = add_fr1x32(f->filterStageOuts[i],
		   mult_fr1x32x32(f->alpha,
				  sub_fr1x32(f->filterStageOuts[i-1],
					     f->filterStageOuts[i])));
    f->filterStageOuts[i] = soft_clip_asym (f->clipLevel[i], f->clipLevelNeg[i],
					    f->filterStageOuts[i]);
  }
  return f->filterStageOuts[3];
}

// oversampling version of the 4-stage symmetrical softclipping ladder
extern fract32 filter_ladder_lpf_asym_os_next (FilterLadder * const filter, fract32 in) {
filter_ladder *f = *filter;
  fract32 lastIn_os = add_fr1x32(shr_fr1x32(in, 1),
				 shr_fr1x32(f->lastInput, 1));

  fract32 out = shr_fr1x32(filter_ladder_lpf_asym_next(&f, lastIn_os), 1);
  out = add_fr1x32(out, shr_fr1x32(filter_ladder_lpf_softclip_next(&f, in), 1));
  f->lastInput = in;
  return out;
}

extern void filter_ladder_set_freq(FilterLadder * const filter, fract32 cv_value) {
  filter_ladder *f = *filter;
    // Convertir CV a frecuencia normalizada para el filtro.
    // El objetivo es mapear el CV (0.0 a 1.0) a un rango de frecuencias de corte musicalmente útil.
    // Usaremos un rango de ~50 Hz a 12 kHz, asumiendo una frecuencia de muestreo de 48 kHz.

    // --- CONSTANTES CORREGIDAS ---
    fract32 min_freq = 0x00222222;   // CORREGIDO: Representa ~50Hz @ 48kHz
    fract32 max_freq = 0x20000000;   // CORRECTO: Representa 12kHz @ 48kHz

    // Asegurar que el valor de CV esté en el rango válido [0.0, 1.0)
    if (cv_value > 0x7FFFFFFF) cv_value = 0x7FFFFFFF;
    // Aunque fract32 es con signo, para CV se usa el rango positivo.
    if (cv_value < 0x00000000) cv_value = 0x00000000;

    fract32 freq_range = sub_fr1x32(max_freq, min_freq);
    fract32 freq = add_fr1x32(min_freq, mult_fr1x32x32(cv_value, freq_range));

    if (freq > 0x3FFFFFFF) {  // limit ~0.499 (~23.9 kHz)
        freq = 0x3FFFFFFF;
    }
    if (freq < 0x00000001) {  
        freq = 0x00000001;
    }

    f->alpha = shl_fr1x32(freq, 1);

    if (f->alpha > 0x60000000) {  // Limita alpha a ~0.75
        f->alpha = 0x60000000;
    }
}

extern void filter_ladder_set_feedback (FilterLadder * const filter, fract32 feedback){
 filter_ladder *f = *filter;
 f->feedback = feedback; 
}