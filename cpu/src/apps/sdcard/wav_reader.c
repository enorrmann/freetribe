#include "wav_reader.h"
#include <string.h>

FRESULT wav_read_info(FIL *file, wav_info_t *info)
{
    FRESULT res;
    UINT br;

    char id[4];
    uint32_t size;

    // Go to start
    f_lseek(file, 0);

    // --- RIFF header ---
    res = f_read(file, id, 4, &br); // "RIFF"
    if (res != FR_OK || memcmp(id, "RIFF", 4) != 0) return FR_INVALID_OBJECT;

    res = f_read(file, &size, 4, &br); // file size (skip)

    res = f_read(file, id, 4, &br); // "WAVE"
    if (res != FR_OK || memcmp(id, "WAVE", 4) != 0) return FR_INVALID_OBJECT;

    int found_fmt = 0;
    int found_data = 0;

    // --- Iterate chunks ---
    while (!found_fmt || !found_data) {

        res = f_read(file, id, 4, &br);
        if (res != FR_OK || br != 4) return FR_DISK_ERR;

        res = f_read(file, &size, 4, &br);
        if (res != FR_OK || br != 4) return FR_DISK_ERR;

        if (memcmp(id, "fmt ", 4) == 0) {
            uint16_t audio_format;
            uint16_t num_channels;
            uint32_t sample_rate;
            uint32_t byte_rate;
            uint16_t block_align;
            uint16_t bits_per_sample;

            f_read(file, &audio_format, 2, &br);
            f_read(file, &num_channels, 2, &br);
            f_read(file, &sample_rate, 4, &br);
            f_read(file, &byte_rate, 4, &br);
            f_read(file, &block_align, 2, &br);
            f_read(file, &bits_per_sample, 2, &br);

            info->audio_format = audio_format;
            info->num_channels = num_channels;
            info->sample_rate = sample_rate;
            info->bits_per_sample = bits_per_sample;

            // Skip remaining fmt bytes if any
            if (size > 16) {
                f_lseek(file, f_tell(file) + (size - 16));
            }

            found_fmt = 1;
        }
        else if (memcmp(id, "data", 4) == 0) {
            info->data_offset = f_tell(file);
            info->data_size = size;

            // Move file pointer to start of data
            f_lseek(file, info->data_offset);

            found_data = 1;
        }
        else {
            // Skip unknown chunk
            f_lseek(file, f_tell(file) + size);
        }
    }

    return FR_OK;
}


int32_t read_s16(uint8_t *buf, int idx)
{
    int16_t *p = (int16_t *)buf;
    return ((int32_t)p[idx]) << 16;
}
int32_t read_s24(uint8_t *buf, int idx)
{
    uint8_t *p = buf + idx * 3;

    int32_t v = ((int32_t)p[0]) |
                ((int32_t)p[1] << 8) |
                ((int32_t)p[2] << 16);

    v = (v << 8) >> 8; // sign extend
    return v << 8;     // to Q1.31
}

int32_t read_s32(uint8_t *buf, int idx)
{
    int32_t *p = (int32_t *)buf;
    return p[idx];
}
int32_t read_f32(uint8_t *buf, int idx)
{
    float *p = (float *)buf;
    float f = p[idx];
    return (int32_t)(f * (float)INT32_MAX);
}
read_sample_fn select_reader(int bps, int format)
{
    if (bps == 16) {
        return read_s16;
    } else if (bps == 24) {
        return read_s24;
    } else if (bps == 32 && format == WAV_FORMAT_PCM) {
        return read_s32;
    } else if (bps == 32 && format == WAV_FORMAT_IEEE_FLOAT) {
        return read_f32;
    }

    return 0; // unsupported format
}
