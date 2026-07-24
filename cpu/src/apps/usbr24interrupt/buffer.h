#ifndef BUFFER_H
#define BUFFER_H

#include <stdint.h>

void usb_rx_push(uint8_t c);
int usb_rx_pop(uint8_t *c);

#endif // BUFFER_H