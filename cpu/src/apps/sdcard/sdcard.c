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
#include "wav_reader.h"

DIR dir;     // Directory object
FILINFO fno; // File information structure

void list_root(void) {
    FRESULT res;

    extern FATFS g_fatfs;
    DEBUG_LOG("Mounting filesystem...");
    res = f_mount(&g_fatfs, "", 0);
    DEBUG_LOG("f_mount result: %i", (int)res);
    if (FR_OK != res) {
        DEBUG_LOG("f_mount failed: %i", (int)res);
        return;
    }

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
    #define BUFFER_SIZE (1024 * 512 * 4)  // max ok size
    BYTE buffer[BUFFER_SIZE]; 

    extern FATFS g_fatfs;

    DEBUG_LOG("Mounting filesystem...");
    res = f_mount(&g_fatfs, "", 0);
    if (FR_OK != res) {
        DEBUG_LOG("f_mount failed: %i", (int)res);
        return;
    }

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
    ft_printf("File contents:");

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

    while (1) {
        // buffer is overwritten each loop
        res = f_read(&file, buffer, sizeof(buffer), &bytes_read);
        DEBUG_LOG("f_read bytes_read: %i", (int)bytes_read);
        int total_samples_per_channel = bytes_read / frame_size;
        DEBUG_LOG("total_samples_per_channel: %i", (int)total_samples_per_channel);
        if (res != FR_OK) {
            DEBUG_LOG("Error reading file: %i", (int)res);
            break;
        }

        ft_printf("sending parameters...");

        int i;
        for (i = 0; i < total_samples_per_channel; i++) {

            int32_t value = read_sample(buffer, i * info.num_channels);

            ft_set_module_param(0, 0, value);
        }
        ft_printf("params sent");

        if (bytes_read < BUFFER_SIZE) {
            break; // End of file
        }
    }

    // Close file
    res = f_close(&file);
    if (res != FR_OK) {
        DEBUG_LOG("Error closing file: %i", (int)res);
    }
}

/*----- Macros -------------------------------------------------------*/

// Bank 7 pin 15.
#define GPIO_POWER_BUTTON 128

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

/**
 * @brief   Initialise application.
 *
 * @return status   Status code indicating success:
 *                  - SUCCESS
 *                  - WARNING
 *                  - ERROR
 */
t_status app_init(void) {

    t_status status = ERROR;

    // Nothing to do here.
    dev_sdcard_init();
    // _print_test_block();

    // list_root();
     //read_file_contents("/clap.wav");
    // read_file_contents("/clap_i32t.wav");
    // read_file_contents("/brown.wav");
    read_file_contents("/brown_stereo.wav");
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
