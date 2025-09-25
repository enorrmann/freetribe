#ifndef WAVE_LOOKUP_H
#define WAVE_LOOKUP_H
#include "custom_aleph_monovoice.h"

fract16 wavetable_lookup_delta(fract32 p, fract32 dp) ;
fract16 sample_playback_delta(fract32 phase, fract32 dp ) ;

#endif
