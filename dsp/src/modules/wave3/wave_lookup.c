#include "wave_lookup.h"
#include "types.h"
#include "aleph.h"
#include "sample_manager.h"

#define FRACT32_MAX ((fract32)0x7fffffff) /* max value of a fract32 */


/**
 * @brief Versión simplificada usando delta de fase para ajuste de índice
 * @param p Fase actual
 * @param dp Delta de fase (usado para ajuste de índice)
 * @return Muestra directa de 16-bit fractional
 */
fract16 wavetable_lookup_delta(fract32 phase, fract32 dp) {

    uint32_t phase_norm = (uint32_t)(phase);

    int index =
        (phase_norm >> (32 - 10)); // this index is ok up to 1024 samples,
                                   // because it inside the cycle
    // int morph_offset = (int)(dp) ; // x 10 or x even x 100 works
    //  int morph_offset = (int)(dp) ; // x 10 or x even x 100 works

    int morph_offset = fract32_smul(1000, dp); // testing values for lfo depth
    index += morph_offset;

    fract32 sample0 = data_sdram[index];

    // Convertir a 16-bit y retornar
    return (fract16)shr_fr1x32(sample0, 16);
}

fract16 _sample_playback_delta(int32_t phase, fract32 freq, int32_t sample_number) {
    int32_t index = (phase >> 12);
    int32_t frac  = phase & 0xFFF; // lower 12 bits as fractional part (0..4095)
    Sample s = &samples[sample_number];
    index = index >> s->quality;

    if (s->loop_point > 0) {
        index = index % s->loop_point;
    }

    index += s->start_position;
    index += s->global_offset;
    if (index < 0 || index + 1 >= s->end_position)
        return 0;

    fract32 s0 = data_sdram[index];
    fract32 s1 = data_sdram[index + 1];

    // linear interpolation: s0 + frac*(s1 - s0)
    // frac is 12-bit, so scale by >>12
    fract32 interp = s0 + (((s1 - s0) * frac) >> 12);

    return interp;
}

fract16 sample_playback_delta(int32_t phase, fract32 freq, int32_t sample_number) {
    int32_t index = (phase >> 12);
    int32_t frac  = phase & 0xFFF; // lower 12 bits as fractional part (0..4095)
    
    Sample s = &samples[sample_number];

    index = index >> s->quality;

    if (s->loop_point > 0) {
        index = index % s->loop_point;
    }

    index += s->start_position;
    index += s->global_offset;

    int32_t end_pos = s->end_position;

    if (index < 0 || index + 1 >= end_pos)
        return 0;

    fract32 s0 = data_sdram[index];
    fract32 s1 = data_sdram[index + 1];

    // --- Linear interpolation ---
    fract32 interp = s0 + (((s1 - s0) * frac) >> 12);

    // --- Fade-out section ---
    // define how many samples before end to start fading
    const int32_t FADE_LEN = 256;  // adjust (128–1024 typical)
    int32_t fade_start = end_pos - FADE_LEN;

    if (index >= fade_start) {
        int32_t dist_to_end = end_pos - index; // 0..FADE_LEN
        if (dist_to_end < 0) dist_to_end = 0;
        if (dist_to_end > FADE_LEN) dist_to_end = FADE_LEN;

        // compute fade factor in Q12 (0..4096)
        int32_t fade = (dist_to_end << 12) / FADE_LEN;

        // apply fade to interp (Q1.31 * Q12 → shift by 12)
        interp = (interp * fade) >> 12;
    }

    return interp;
}

