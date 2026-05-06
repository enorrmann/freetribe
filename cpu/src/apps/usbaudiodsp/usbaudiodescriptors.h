#ifndef _USB_AUDIO_DESCRIPTORS_H_
#define _USB_AUDIO_DESCRIPTORS_H_

#include <stdint.h>

/*
 * Device Descriptor:
 */
const uint8_t deviceDescriptor[] = {
    18,   1,    0x00, 0x02, /* bcdUSB: 2.00 */
    0x00, 0x00, 0x00, 64,   /* bMaxPacketSize0 */
    0x1C, 0x1C,             /* idVendor */
    0x10, 0x00,             /* idProduct */
    0x00, 0x02,             /* bcdDevice */
    1,    2,    3,          /* iManufacturer, iProduct, iSerialNumber */
    1                       /* bNumConfigurations */
};

const uint8_t devQualDescriptor[] = {10,   6,    0x00, 0x02, /* bcdUSB: 2.00 */
                                     0x02,                   /* bDeviceClass (Audio) */
                                     0x00, 0x00, 64,         /* bMaxPacketSize0 */
                                     1,    0};

const uint8_t configDescriptor[] = {
    /* Config */
    9, 2, 190, 0, 3, 1, 0, 0x80, 50,

    /* Interface 0 (Audio Control) */
    9, 4, 0, 0, 0, 0x01, 0x01, 0x00, 0,

    /* Audio Control Header */
    10, 0x24, 0x01, 0x00, 0x01, 0x48, 0x00, 0x02, 0x01, 0x02,

    /* USB Streaming Input Terminal (playback) */
    12, 0x24, 0x02, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    /* Feature Unit (playback) */
    10, 0x24, 0x06, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,

    /* Output Terminal (speaker) */
    9, 0x24, 0x03, 0x03, 0x01, 0x03, 0x00, 0x02, 0x00,

    /* Microphone Input Terminal */
    12, 0x24, 0x02, 0x04, 0x02, 0x01, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,

    /* Feature Unit (capture) */
    10, 0x24, 0x06, 0x05, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,

    /* USB Streaming Output Terminal (capture) */
    9, 0x24, 0x03, 0x06, 0x01, 0x01, 0x00, 0x05, 0x00,

    /* Interface 1 alt 0 (Playback – zero-bandwidth) */
    9, 4, 1, 0, 0, 0x01, 0x02, 0x00, 0,

    /* Interface 1 alt 1 (Playback – active) */
    9, 4, 1, 1, 1, 0x01, 0x02, 0x00, 0,

    /* Audio Streaming Header (playback) */
    7, 0x24, 0x01, 0x01, 0x01, 0x01, 0x00,

    /* Format Type I */
    11, 0x24, 0x02, 0x01, 0x02, 0x02, 16, 1, 0x80, 0xBB, 0x00,

    /* Standard Endpoint OUT */
    7, 5, 0x02, /* bEndpointAddress */
    1,          /* bmAttributes (Isochronous) */
    192, 0,     /* wMaxPacketSize */
    4,          /* bInterval: 4 (2^(4-1) = 8 micro-frames = 1ms) */

    /* Class-specific Endpoint OUT */
    7, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00,

    /* Interface 2 alt 0 (Capture – zero-bandwidth) */
    9, 4, 2, 0, 0, 0x01, 0x02, 0x00, 0,

    /* Interface 2 alt 1 (Capture – active) */
    9, 4, 2, 1, 1, 0x01, 0x02, 0x00, 0,

    /* Audio Streaming Header (capture) */
    7, 0x24, 0x01, 0x06, 0x01, 0x01, 0x00,

    /* Format Type I */
    11, 0x24, 0x02, 0x01, 0x02, 0x02, 16, 1, 0x80, 0xBB, 0x00,

    /* Standard Endpoint IN */
    7, 5, 0x81, /* bEndpointAddress */
    1,          /* bmAttributes (Isochronous) */
    192, 0,     /* wMaxPacketSize */
    4,          /* bInterval: 4 (2^(4-1) = 8 micro-frames = 1ms) */

    /* Class-specific Endpoint IN */
    7, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00};

const uint8_t string0[] = {4, 3, 0x09, 0x04};
const uint8_t string1[] = {20, 3, 'F', 0, 'r', 0, 'e', 0, 'e', 0, 't', 0, 'r', 0, 'i', 0, 'b', 0, 'e', 0};
const uint8_t string2[] = {20, 3, 'F', 0, 'T', 0, 'B', 0, ' ', 0, 'A', 0, 'u', 0, 'd', 0, 'i', 0, 'o', 0};
const uint8_t string3[] = {10, 3, '1', 0, '2', 0, '3', 0, '4', 0};

const uint8_t *const strings[] = {string0, string1, string2, string3};
const uint8_t stringLens[] = {sizeof(string0), sizeof(string1), sizeof(string2), sizeof(string3)};

#endif /* _USB_AUDIO_DESCRIPTORS_H_ */