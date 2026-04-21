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
#include <string.h>

uint32_t to_send [32] ;

void module_param_callback(uint16_t module_id, uint16_t param_index, int32_t param_value);

void module_param_callback(uint16_t module_id, uint16_t param_index, int32_t param_value){
    ft_printf("Got param value: module %u, param %u, value %d", module_id, param_index, param_value);
}

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
wav_info_t wav_info[MAX_WAV_FILES];
//char wav_names[MAX_WAV_FILES][64]; // store names if needed
uint8_t wav_count = 0;

void _preload_files(const char *base_path) {
    FRESULT res;
    DIR dir;
    FILINFO fno;

    char fullpath[256];
    size_t base_len = strlen(base_path);

    // abrir directorio
    res = f_opendir(&dir, base_path);
    DEBUG_LOG("f_opendir('%s') = %d", base_path, (int)res);

    if (res != FR_OK)
        return;

    for (;;) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0)
            break;

        if (!(fno.fattrib & AM_DIR)) {
            const char *name = fno.fname;
            int len = strlen(name);

            if (len > 4 && strcasecmp(&name[len - 4], ".wav") == 0) {

                if (wav_count < MAX_WAV_FILES) {

                    // construir path completo
                    if (base_path[base_len - 1] == '/')
                        snprintf(fullpath, sizeof(fullpath), "%s%s", base_path, name);
                    else
                        snprintf(fullpath, sizeof(fullpath), "%s/%s", base_path, name);

                    if (f_open(&wav_files[wav_count], fullpath, FA_READ) == FR_OK) {
                        DEBUG_LOG("WAV[%d]: %s", wav_count, fullpath);
                        int res = wav_read_info(&wav_files[wav_count], &wav_info[wav_count]);
                        DEBUG_LOG("wav_read_info result: %i", (int)res);
                        wav_count++;
                    } else {
                        DEBUG_LOG("FAIL open: %s", fullpath);
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

void read_file_contents(const char *filename, int file_number) {
    __attribute__((aligned(4)))
    //#define BUFFER_SIZE (1024 * 1024 * 2) // max ok size
    #define BUFFER_SIZE (32*24* 128) // smaller buffer to test chunking 
    BYTE buffer[BUFFER_SIZE];

    
    FRESULT res;
    FIL file;
    //FIL * pFile  = &file;
    FIL * pFile =  &wav_files[file_number];
    wav_info_t * pInfo = &wav_info[file_number];
    f_lseek(pFile, 0); // Ensure we're at the start of the file
    UINT bytes_read = 0;

    // Open file for reading if using local file instead of preloaded
    if (pFile == &file) {
        DEBUG_LOG("Opening file: %s", filename);
        res = f_open(pFile, filename, FA_READ);
        if (res != FR_OK) {
            DEBUG_LOG("Failed to open file: %i", (int)res);
            return;
        }
    }
    
    f_lseek(pFile, pInfo->data_offset); // Seek to start of sample data

    // Read and print file contents
    DEBUG_LOG("File contents:");

    int bytes_per_sample = pInfo->bits_per_sample / 8; // 4 para int32/float32
    int frame_size = bytes_per_sample * pInfo->num_channels; // 4=mono, 8=stereo
    DEBUG_LOG("bytes_per_sample %i", (int)bytes_per_sample);
    DEBUG_LOG("frame_size %i", (int)frame_size);

    read_sample_fn read_sample = select_reader(pInfo);

    if (!read_sample) {
        DEBUG_LOG("no reader function: %i", (int)read_sample);
        return;
    }

    ipc_init_buffer();
    uint32_t bytes_remaining = pInfo->data_size;
    do {
        // Shift our destination RAM pointer to guarantee that when FatFs reaches the 
        // next 512-byte sector and triggers a hardware direct DMA transfer (disk_read), 
        // the target RAM address is perfectly 4-byte aligned. This prevents DMA offset
        // truncation bugs when reading WAVs that have odd-sized metadata chunks.
        uint32_t pad_offset = f_tell(pFile) % 4;
        BYTE *working_buffer = buffer + pad_offset;

        UINT bytes_to_read = sizeof(buffer) - pad_offset;
        // Ensure aligned chunk reading so we don't split frames
        bytes_to_read -= (bytes_to_read % frame_size);
        
        if (bytes_remaining < (uint32_t)bytes_to_read) {
            bytes_to_read = (UINT)bytes_remaining;
        }

        // buffer is overwritten each loop
        res = f_read(pFile, working_buffer, bytes_to_read, &bytes_read);
        int total_samples_per_channel = bytes_read / frame_size;
        if (res != FR_OK || bytes_read == 0) {
            break;
        }

        int i;
        for (i = 0; i < total_samples_per_channel; i++) {
            int32_t sample = read_sample(working_buffer, i * pInfo->num_channels);
            ipc_add_to_buffer(sample);
        }

        bytes_remaining -= bytes_read;

    } while (bytes_remaining > 0);
ipc_send_last_chunk();
    //ipc_send_buffer_chunked();
    //ipc_send_buffer_via_param();
    

    // Close file if it was opened here and not part of array
    if (pFile == &file) {
        res = f_close(pFile);
    }
    if (res != FR_OK) {
        DEBUG_LOG("Error closing file: %i", (int)res);
    }
}


/*----- Macros -------------------------------------------------------*/

// Bank 7 pin 15.
#define GPIO_POWER_BUTTON 128

void _trigger_callback(uint8_t pad, uint8_t vel, bool state) {
    if (state) {
        to_send[0] = pad;
        ft_set_module_param(0, PARAM_TRANSFER_MEMORY, to_send);
        ft_get_module_param(0, PARAM_TRANSFER_MEMORY);
            //read_file_contents("",pad);
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
    ft_register_dsp_callback(MSG_TYPE_MODULE, MODULE_PARAM_VALUE, module_param_callback);


    t_status status = ERROR;

    // Nothing to do here.
    //dev_sdcard_init();
    // _print_test_block();
    _mount_fs();
//_preload_files("/");
_preload_files("/samples/clean");
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
