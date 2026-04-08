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

                       Copyright bangcorrupt 2024

----------------------------------------------------------------------*/

/**
 * @file    sdcard.c
 *
 * @brief   Example app showing how to use the SD card.
 */

/*----- Includes -----------------------------------------------------*/

#include "per_gpio.h"

#include "freetribe.h"
// BEGIN FF
#include "ff.h"
#include "macros.h"
// END FF

#include "ipc_helper.h"
#include "wav_reader.h"
#include "parameters.h"


void _mount_fs();

void _mount_fs(){
    extern FATFS g_fatfs;
    DEBUG_LOG("Mounting filesystem...");
    FRESULT res = f_mount(&g_fatfs, "", 0);
    DEBUG_LOG("f_mount result: %i", (int)res);
    if (FR_OK != res) {
        DEBUG_LOG("f_mount failed: %i", (int)res);
        
    }
}



DIR dir;     // Directory object
FILINFO fno; // File information structure

void _trigger_callback(uint8_t pad, uint8_t vel, bool state);


#define MAX_WAV_FILES 16

FIL wav_files[MAX_WAV_FILES];
char wav_names[MAX_WAV_FILES][64]; // store names if needed
uint8_t wav_count = 0;

void _preload_files(void) {
    FRESULT res;

    ft_printf("Opening root directory...");
    res = f_opendir(&dir, "/");
    DEBUG_LOG("f_opendir result: %i", (int)res);

    if (res != FR_OK)
        return;

    for (;;) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0)
            break;

        if (!(fno.fattrib & AM_DIR)) {
            // check .wav extension
            const char *name = fno.fname;
            int len = strlen(name);

            if (len > 4 && strcasecmp(&name[len - 4], ".wav") == 0) {
                if (wav_count < MAX_WAV_FILES) {
                    strcpy(wav_names[wav_count], name);

                    // open file
                    if (f_open(&wav_files[wav_count], name, FA_READ) == FR_OK) {
                        ft_printf("WAV[%d]: %s", wav_count, name);
                        wav_count++;
                    }
                }
            }
        }
    }

    f_closedir(&dir);
}

void list_root(void) {
    FRESULT res;

    // Open root directory
    DEBUG_LOG("Opening root directory...");
    res = f_opendir(&dir, "/");
    DEBUG_LOG("f_opendir result: %i", (int)res);
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno); // Read next item
            if (res != FR_OK || fno.fname[0] == 0)
                break; // Exit on error or end of dir

            if (fno.fattrib & AM_DIR) {
                DEBUG_LOG("[DIR]  %s", fno.fname);
            } else {
                DEBUG_LOG("       %s  (%lu bytes)", fno.fname,
                          (unsigned long)fno.fsize);
            }
        }
        f_closedir(&dir);
    } else {
        DEBUG_LOG("Failed to open root directory (%d)", res);
    }
}

void read_file_contents(const char *filename) {

    wav_info_t info;

    FRESULT res;
    FIL file;
    UINT bytes_read = 0;

    __attribute__((aligned(512)))
#define BUFFER_SIZE (1024 * 512 * 4) // max ok size
    BYTE buffer[BUFFER_SIZE];
    // BYTE buffer_2[BUFFER_SIZE]; // this works so 512kB is definitely not the

    // Open file for reading
    DEBUG_LOG("Opening file: %s", filename);
    res = f_open(&file, filename, FA_READ);
    if (res != FR_OK) {
        DEBUG_LOG("Failed to open file: %i", (int)res);
        return;
    }
    if (wav_read_info(&file, &info) == FR_OK) {
        DEBUG_LOG("SR: %lu", info.sample_rate);
        DEBUG_LOG("Bits: %u", info.bits_per_sample);
        DEBUG_LOG("Ch: %u", info.num_channels);
        DEBUG_LOG("Data offset: %lu", info.data_offset);
        DEBUG_LOG("Data size: %lu", info.data_size);
        DEBUG_LOG("Audio format: %u", info.audio_format);
        f_lseek(&file, info.data_offset); // Seek to start of sample data
    }

    // Read and print file contents
    DEBUG_LOG("File contents:");

    int bytes_per_sample = info.bits_per_sample / 8; // 4 para int32/float32
    int frame_size = bytes_per_sample * info.num_channels; // 4=mono, 8=stereo
    DEBUG_LOG("bytes_per_sample %i", (int)bytes_per_sample);
    DEBUG_LOG("frame_size %i", (int)frame_size);

    read_sample_fn read_sample =
        select_reader(info.bits_per_sample, info.audio_format);

    if (!read_sample) {
        DEBUG_LOG("no reader function: %i", (int)read_sample);
        return;
    }

    ipc_init_buffer();
    do {
        // buffer is overwritten each loop
        res = f_read(&file, buffer, sizeof(buffer), &bytes_read);
        int total_samples_per_channel = bytes_read / frame_size;
        if (res != FR_OK) {
            break;
        }

        int i;
        for (i = 0; i < total_samples_per_channel; i++) {
            int32_t sample = read_sample(buffer, i * info.num_channels);
            ipc_add_to_buffer(sample);

        }

    } while (bytes_read ==
             BUFFER_SIZE); // if we read less than the buffer size, we know
                           // we've reached the end of the file

    //ipc_send_buffer_chunked();
    ipc_send_buffer_via_param();
    

    // Close file
    res = f_close(&file);
    if (res != FR_OK) {
        DEBUG_LOG("Error closing file: %i", (int)res);
    }
}

/*----- Macros -------------------------------------------------------*/

// Bank 7 pin 15.
#define GPIO_POWER_BUTTON 128

void _trigger_callback(uint8_t pad, uint8_t vel, bool state) {
    if (state) {
        
        switch (pad) {
        case 0:
            read_file_contents("/clap.wav");
            break;
        case 1:
            read_file_contents("/kick-1985.wav");
            break;
        case 2:
            read_file_contents("/hh-808.wav");
            break;

        default:
       // ipc_simple_send(pad); // for testing    
            break;
        }
    }
}

/**
 * @brief   Initialise application.
 *
 * @return status   Status code indicating success:
 *                  - SUCCESS
 *                  - WARNING
 *                  - ERROR
 */
t_status app_init(void) {
    ft_register_panel_callback(TRIGGER_EVENT, _trigger_callback);

    t_status status = ERROR;

    // Nothing to do here.
    //dev_sdcard_init();
    // _print_test_block();
    _mount_fs();
_preload_files();
    // list_root();
    // read_file_contents("/clap.wav");
    // read_file_contents("/clap_i32t.wav");
    // read_file_contents("/brown.wav");
    // read_file_contents("/brown_stereo.wav");
    // read_file_contents("/clap_f32.wav"); // float test

    status = SUCCESS;
    return status;
}

/**
 * @brief   Run application.
 *
 * Test if GPIO index 128 (bank 7 pin 15) is low, if so, shutdown.
 */
void app_run(void) {

    if (per_gpio_get_indexed(GPIO_POWER_BUTTON) == 0) {

        ft_shutdown();

        // Should never reach here.
    }
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
