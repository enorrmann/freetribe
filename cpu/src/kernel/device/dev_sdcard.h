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
 * @file    dev_sdcard.h
 *
 * @brief   Public API for SD card device.
 */

#ifndef DEV_SDCARD_H
#define DEV_SDCARD_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdint.h>

#include "macros.h"

#include "sd_protocol.h"
#include "per_mmcsd.h"
#include "ft_error.h"
#include <stdbool.h>


/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
    uint16_t        rca;
    uint8_t         is_mmc:1;
    uint8_t         is_hc:1;
    uint8_t         is_bus4bit:1;
    sdp_cur_stat_t  ci_stat;
    sdp_r1_stat_t   r1_stat;
    CID_t           cid;
    CSD_t           csd;
} sd_sm_t;

typedef struct {
    int rt;
    const char* estr;
} sdmmc_estr_t;


typedef enum {
	SDCARD_OK = 0,
	SDCARD_NO_RESPONSE = -1,
	SDCARD_CRC_ERROR = -2,
	SDCARD_UNSUPPORTED = -3,
	SDCARD_ERROR = -4,
} t_sdcard_status;

/*----- Extern function prototypes -----------------------------------*/

extern sd_sm_t sd_sm[1];

t_sdcard_status dev_sdcard_init(void);
t_sdcard_status dev_sdcard_read(uint32_t blk_nr, uint32_t blk_cnt, uint32_t* buf);
t_sdcard_status dev_sdcard_write(uint32_t blk_nr, uint32_t blk_cnt, const uint32_t* buf);
const char* dev_sdcard_err_string(int rt);

#ifdef __cplusplus
}
#endif
#endif

/*----- End of file --------------------------------------------------*/
