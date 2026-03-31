#ifndef SIMPLE_REVERB_H
#define SIMPLE_REVERB_H

#include <stdint.h>
#include <stdlib.h>

/**
 * Applies a simple reverb effect to a buffer of audio samples.
 *
 * @param buffer Pointer to the input/output audio buffer.
 * @param num_samples The number of samples in the buffer to process.
 */
int32_t apply_simple_reverb(int32_t input, int line_number) ;
int32_t apply_complex_reverb(int32_t input);

#endif // SIMPLE_REVERB_H