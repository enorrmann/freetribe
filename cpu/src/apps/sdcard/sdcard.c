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
    const int buffer_size = 4096;
    char buffer[buffer_size];

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
    DEBUG_LOG("File contents:");
    // f_lseek(&file, 3); // seek to offset 3 for testing
    while (1) {
        res = f_read(&file, buffer, sizeof(buffer), &bytes_read);
        DEBUG_LOG("f_read bytes_read: %i", (int)bytes_read);
        if (res != FR_OK) {
            DEBUG_LOG("Error reading file: %i", (int)res);
            break;
        }

        // Print each byte/character
static uint8_t temp[4];
static int temp_index = 0;

for (UINT i = 0; i < bytes_read; i++) {
    temp[temp_index++] = buffer[i];

    if (temp_index == 4) {
        int32_t value =  (int32_t)temp[0] |
                        ((int32_t)temp[1] << 8) |
                        ((int32_t)temp[2] << 16) |
                        ((int32_t)temp[3] << 24);

        ft_set_module_param(0, 0, value);
        temp_index = 0;
    }
}        // DEBUG_LOG("%s", buffer);

        if (bytes_read < buffer_size) {
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
    read_file_contents("/clap_i32t.wav");
    //clap_f32.wav float

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
