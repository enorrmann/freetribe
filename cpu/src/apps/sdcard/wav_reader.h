#ifndef WAV_READER_H
#define WAV_READER_H

#include <stdint.h>
#include "ff.h"   // FatFs

typedef enum {
    WAV_FORMAT_UNKNOWN      = 0,
    WAV_FORMAT_PCM          = 1,
    WAV_FORMAT_IEEE_FLOAT   = 3,
    WAV_FORMAT_ALAW         = 6,
    WAV_FORMAT_MULAW        = 7,
    WAV_FORMAT_EXTENSIBLE   = 65534
} wav_audio_format_t;

typedef struct {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint16_t bits_per_sample;

    uint32_t data_offset; // byte offset where samples start
    uint32_t data_size;   // size of sample data in bytes
} wav_info_t;

/**
 * @brief Parse WAV file and extract basic info
 * 
 * @param file Opened FIL*
 * @param info Output struct
 * @return FRESULT
 */
FRESULT wav_read_info(FIL *file, wav_info_t *info);

#endif