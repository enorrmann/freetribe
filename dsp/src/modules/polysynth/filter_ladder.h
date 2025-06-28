/* filter_ladder.h
   audio library
   aleph

   a digital ladder filter for 32-bit fixed point audio.
*/
#ifndef _ALEPH_AUDIO_FILTER_LADDER_H_
#define _ALEPH_AUDIO_FILTER_LADDER_H_

#include "fix.h"
#include "types.h"
#include "aleph_soft_clip.h"
#include "aleph_oscillator.h"

typedef struct _filter_ladder {
  t_Mempool *mempool;
  fract32 filterStageOuts[4];
  fract32 filterStageLastIns[4];
  fract32 lastOutput;
  fract32 lastInput;
  fract32 alpha;
  fract32 feedback;
  u8 fbShift;
  fract32 clipLevel[4];
  fract32 clipLevelNeg[4];

} filter_ladder;

typedef filter_ladder *FilterLadder;


extern void filter_ladder_init_to_pool (FilterLadder * const f,Mempool *const mempool);
extern void filter_ladder_set_freq (FilterLadder * const f, fract32 freq);
extern void filter_ladder_set_feedback (FilterLadder * const f, fract32 feedback);
extern fract32 filter_ladder_lpf_next (FilterLadder * const f, fract32 in);
extern fract32 filter_ladder_hpf_next (FilterLadder * const f, fract32 in);
extern fract32 filter_ladder_lpf_os_next (FilterLadder * const f, fract32 in);
extern fract32 filter_ladder_lpf_softclip_next (FilterLadder * const f, fract32 in);
extern fract32 filter_ladder_lpf_softclip_os_next (FilterLadder * const f, fract32 in);
extern fract32 filter_ladder_lpf_asym_next (FilterLadder * const f, fract32 in);
extern fract32 filter_ladder_lpf_asym_os_next (FilterLadder * const f, fract32 in);


#endif
