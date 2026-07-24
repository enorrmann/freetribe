#include "buffer.h"

#define USB_SERIAL_BUF_SIZE 512
static uint8_t g_usbRxBuf[USB_SERIAL_BUF_SIZE];
static uint32_t g_usbRxHead = 0;
static uint32_t g_usbRxTail = 0;

void usb_rx_push(uint8_t c) {
    uint32_t next = (g_usbRxHead + 1) % USB_SERIAL_BUF_SIZE;
    if (next != g_usbRxTail) {
        g_usbRxBuf[g_usbRxHead] = c;
        g_usbRxHead = next;
    }
}

int usb_rx_pop(uint8_t *c) {
    if (g_usbRxHead == g_usbRxTail)
        return 0;
    *c = g_usbRxBuf[g_usbRxTail];
    g_usbRxTail = (g_usbRxTail + 1) % USB_SERIAL_BUF_SIZE;
    return 1;
}
