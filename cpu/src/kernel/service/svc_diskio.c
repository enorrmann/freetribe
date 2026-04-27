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

                       Copyright bangcorrupt 2023

----------------------------------------------------------------------*/

/**
 * @file    svc_io.c.
 *
 * @brief   Disk IO service layer.
 */

/*----- Includes -----------------------------------------------------*/

#include <ff.h>
#include <diskio.h>

#include "macros.h"
#include "freetribe.h"

#include "dev_sdcard.h"
#include "svc_diskio.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

FATFS g_fatfs;
PARTITION VolToPart[FF_VOLUMES] = { { 0, 0 } };

/*----- Extern variable definitions ----------------------------------*/

extern sd_sm_t sd_sm[1];

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

/**
 * @brief   Initialize Disk Drive.
 * 
 * @param   pdrv   Physical drive number (0)
 */
DSTATUS disk_initialize(BYTE pdrv) {
    DSTATUS stat;
    int result;

    if (SDCARD_OK != dev_sdcard_init()) {
        DEBUG_LOG("disk_initialize() dev_sdcard_init() failed");
        return STA_NODISK;
    }
    
    return 0;
}

/**
 * @brief   Returns the current status of a drive.
 * 
 * @param   drv   Physical drive number (0)
 */
DSTATUS disk_status(BYTE drv) {
    return 0;
}

/**
 * @brief   Read sector(s) from the disk drive.
 * 
 * @param   drv     Physical drive number (0)
 * @param   buff    Pointer to the data buffer to store read data
 * @param   sector  Start sector in LBA
 * @param   count   Number of sectors to read
 */
DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
	
    DEBUG_LOG("disk_read(%i, %p, %i, %i)", (int)pdrv, (void*)buff, (int)sector, (int)count);

    uint32_t *buf_u32 = (uint32_t*)buff; // be aware of this
    t_sdcard_status st = dev_sdcard_read(sector, count, buf_u32);

    if (SDCARD_OK != st) {
        DEBUG_LOG("disk_read->dev_sdcard_read error: %i", (int)st);
        return RES_ERROR;
    }

	return RES_OK;
}

/**
 * @brief   Write sector(s) to the disk drive.
 * 
 * @param   pdrv    Physical drive number (0)
 * @param   buff    Data to be written
 * @param   sector  Start sector in LBA
 * @param   count   Number of sectors to write
 */
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {

    DEBUG_LOG("disk_write(%i, %p, %i, %i)", (int)pdrv, (void*)buff, (int)sector, (int)count);

    const uint32_t *buf_u32 = (const uint32_t*)buff; // be aware of this
    t_sdcard_status st = dev_sdcard_write(sector, count, buf_u32);

    if (SDCARD_OK != st) {
        DEBUG_LOG("disk_write->dev_sdcard_write error: %i", (int)st);
    }

    return RES_OK;
}

/**
 * @brief   Control interface between SD card driver and FatFS. FatFS
 *          calls this function to inform itself about the SD card driver.
 * 
 * @param   pdrv    Physical drive number (0)
 * @param   cmd     Control code
 * @param   buff    Buffer to send/receive control data
 */
DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv != 0) return RES_PARERR;

    switch (cmd) {
case GET_SECTOR_COUNT:
    if (buff) {
        // Asumiendo que sd_sm[0].csd es una estructura donde los campos 
        // ya están extraídos y alineados (como sugiere tu DEBUG RAW)
        uint32_t n_sectors = 0;
        
        // Usamos la estructura que ya conocemos del dump
        if (sd_sm[0].csd.CSD_STRUCTURE == 0x01) { // SDHC
            // En tu dump, el "device size" (C_SIZE) se mostró como 0xE8F5
            // Si la estructura mapea C_SIZE directamente como un uint32_t:
            uint32_t c_size = sd_sm[0].csd.C_SIZE; 

            // Aplicamos la fórmula mágica para SDHC
            n_sectors = (c_size + 1) * 1024;
            
            ft_printf("SDHC Detectada. C_SIZE: 0x%X, Sectores: %u\n", c_size, n_sectors);
        } else {
            // Lógica para V1 (Standard Capacity)
            // Aquí tendrías que usar los campos c_size, c_size_mult y read_bl_len
            // que tu driver ya haya extraído previamente.
            uint32_t c_size = sd_sm[0].csd.C_SIZE;
            uint32_t c_size_mult = sd_sm[0].csd.C_SIZE_MULT;
            uint32_t read_bl_len = sd_sm[0].csd.READ_BL_LEN;
            
            n_sectors = (c_size + 1) << (c_size_mult + 2 + read_bl_len - 9);
            ft_printf("SD Standard Detectada. C_SIZE: 0x%X, C_SIZE_MULT: %u, READ_BL_LEN: %u, Sectores: %u\n", 
                c_size, c_size_mult, read_bl_len, n_sectors);
        }

        *(uint32_t*)buff = n_sectors;
        return RES_OK;
    }
    break;
        case GET_SECTOR_SIZE:
            if (buff) {
                *(WORD*)buff = 512;
                return RES_OK;
            }
            break;

        case CTRL_SYNC:
            return RES_OK;
    }
    return RES_PARERR;
}

 /**
 * @brief   Real time clock service to be called from FatFS module.
 *          Any valid time must be returned even if the system does
 *          not support a real time clock.
 */
DWORD get_fattime(void) {
    // @TODO: implement RTC
    return   ((2007UL-1980) << 25)  // Year 2007
            | (6UL << 21)           // Month June
            | (5UL << 16)           // Day 5
            | (11U << 11)           // Hour 11
            | (38U << 5)            // Min 38
            | (0U >> 1);            // Sec 0
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
