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

fract16 sample_playback_delta(int32_t phase, fract32 freq) {
    

    //int32_t index = (phase>>12); // 10 1024 12 4096 cant address full range
    int32_t index = phase >> (samples[0]->quality); // ADJUST SAMPLE PLAYBACK

    if (samples[0]->loop_point>0 ) {
        index = index % samples[0]->loop_point ;
    }
    
    index+= (samples[0]->start_position);

    fract32 sample0 = data_sdram[index];
    return sample0;


}
