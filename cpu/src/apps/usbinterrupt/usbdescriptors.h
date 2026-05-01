#ifndef USB_DESCRIPTORS_H
#define USB_DESCRIPTORS_H

#include <stdint.h>

const uint8_t deviceDescriptor[] = {18, 1, 0x10, 0x01, 0x02, 0x00, 0x00, 64, 0x1C, 0x1C, 0x10, 0x00, 0x00, 0x02, 1, 2, 3, 1};
const uint8_t devQualDescriptor[] = {10, 6, 0x00, 0x02, 0x02, 0x00, 0x00, 64, 1, 0};
const uint8_t configDescriptor[] = {
    // Config
    9, 2, 67, 0, 2, 1, 0, 0xC0, 50,
    // Interface 0 (CDC Control)
    9, 4, 0, 0, 1, 0x02, 0x02, 0x01, 0,
    // CDC Header
    5, 0x24, 0x00, 0x10, 0x01,
    // Call Management
    5, 0x24, 0x01, 0x00, 1,
    // ACM
    4, 0x24, 0x02, 0x02,
    // Union
    5, 0x24, 0x06, 0, 1,
    // Endpoint Interrupt IN
    7, 5, 0x83, 3, 8, 0, 255,
    // Interface 1 (CDC Data)
    9, 4, 1, 0, 2, 0x0A, 0x00, 0x00, 0,
    // Endpoint Bulk OUT
    7, 5, 0x02, 2, 64, 0, 0,
    // Endpoint Bulk IN
    7, 5, 0x81, 2, 64, 0, 0};

const uint8_t string0[] = {4, 3, 0x09, 0x04};
const uint8_t string1[] = {20, 3, 'F', 0, 'r', 0, 'e', 0, 'e', 0, 't', 0, 'r', 0, 'i', 0, 'b', 0, 'e', 0};
const uint8_t string2[] = {16, 3, 'C', 0, 'D', 0, 'C', 0, ' ', 0, 'A', 0, 'C', 0, 'M', 0};
const uint8_t string3[] = {10, 3, '1', 0, '2', 0, '3', 0, '4', 0};

const uint8_t *const strings[] = {string0, string1, string2, string3};
const uint8_t stringLens[] = {sizeof(string0), sizeof(string1), sizeof(string2), sizeof(string3)};

#endif // USB_DESCRIPTORS_H