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
 * @file    per_gpio.h
 *
 * @brief   Public API for BF523 GPIO driver.
 */

#ifndef PER_GPIO_H
#define PER_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdint.h>

/*----- Macros -------------------------------------------------------*/

#define HWAIT PG0
#define UART0_TX PG7

/*----- Typedefs -----------------------------------------------------*/

typedef enum { PORT_F, PORT_G, PORT_H } e_port;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void per_gpio_init(void);
uint16_t per_gpio_get_port(uint8_t port);
void per_gpio_set_port(uint8_t port, uint16_t value);

#ifdef __cplusplus
}
#endif
#endif

/*----- End of file --------------------------------------------------*/
