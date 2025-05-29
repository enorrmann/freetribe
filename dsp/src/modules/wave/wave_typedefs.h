#ifndef WAVE_TYPEDEFS_H
#define WAVE_TYPEDEFS_H

#include "osc_waves.h"
#include "wave_params.h"


// single "voice" structure
typedef struct _waveVoice {
    // oscillator
    osc osc;
    // filter
    ////Aleph_FilterSVF svf;
    // osc amp
    fract32 amp;
    // osc output bus
    fract32 oscOut;
    // filter output bus
    fract32 svfOut;

    // amp smoother
    //  filter_1p_lo ampSlew;
    Slew32 ampSlew;
    // cutoff smoother
    //  filter_1p_lo cutSlew;
    Slew32 cutSlew;
    // rq smoother
    //  filter_1p_lo rqSlew;
    Slew32 rqSlew;

    // dry mix
    Slew16 drySlew;
    // wet mix
    Slew16 wetSlew;

    // PM input
    fract32 pmIn;

    // shape mod input
    fract32 wmIn;

    // PM delay buffer
    fract32 modDelBuf[WAVES_PM_DEL_SAMPS];
    //  fract32* modDelBuf;
    // PM delay write index
    u32 modDelWrIdx;
    // PM delay read index
    u32 modDelRdIdx;

} wavesVoice;

typedef struct {

    wavesVoice voice[MAX_VOICES];
    fract32 amp_level;
    fract32 velocity;

} t_module;

#endif