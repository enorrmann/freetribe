#ifndef HS_USB_DESCRIPTORS_H
#define HS_USB_DESCRIPTORS_H

#include <stdint.h>

#define USB_MAX_PACKET_SIZE    512
/* begin device descriptors */

// ---------------------------------------------------------
// DEVICE QUALIFIER DESCRIPTOR
// ---------------------------------------------------------
static const uint8_t devQualDescriptor[] = {
    10,         // bLength: 10 bytes
    0x06,       // bDescriptorType: DEVICE_QUALIFIER (6)
    0x00, 0x02, // bcdUSB: 0x0200 (USB 2.0)
    0xFF,       // bDeviceClass: Vendor Specific (Ajustado de 0x02 a 0xFF)
    0x00,       // bDeviceSubClass: 0
    0x00,       // bDeviceProtocol: 0
    64,         // bMaxPacketSize0: 64 bytes
    1,          // bNumConfigurations: 1
    0           // bReserved: 0
};
// ---------------------------------------------------------
// DESCRIPTOR DE DISPOSITIVO
// ---------------------------------------------------------
static const uint8_t deviceDescriptor[] = {
    18,         // bLength: 18
    0x01,       // bDescriptorType: DEVICE
    0x00, 0x02, // bcdUSB: 0x0200
    0xFF,       // bDeviceClass: Vendor Specific (0xff)
    0x00,       // bDeviceSubClass: 0
    0x00,       // bDeviceProtocol: 0
    64,         // bMaxPacketSize0: 64
    0x86, 0x16, // idVendor: 0x1686 (ZOOM Corporation)
    0xDE, 0x00, // idProduct: 0x00de
    0x00, 0x00, // bcdDevice: 0x0000
    1,          // iManufacturer: 1
    2,          // iProduct: 2
    3,          // iSerialNumber: 3
    1           // bNumConfigurations: 1
};

// ---------------------------------------------------------
// DESCRIPTOR DE CONFIGURACIÓN Y ENDPOINTS
// ---------------------------------------------------------
static const uint8_t configDescriptor[] = {
    // CONFIGURATION DESCRIPTOR
    9,          // bLength: 9
    2,          // bDescriptorType: CONFIGURATION (0x02)
    46, 0,      // wTotalLength: 46
    1,          // bNumInterfaces: 1
    1,          // bConfigurationValue: 1
    0,          // iConfiguration: 0
    0x80,       // Configuration bmAttributes: 0x80
    0xF0,       // bMaxPower: 240 (480mA)

    // INTERFACE DESCRIPTOR (0.0)
    9,          // bLength: 9
    4,          // bDescriptorType: INTERFACE (0x04)
    0,          // bInterfaceNumber: 0
    0,          // bAlternateSetting: 0
    4,          // bNumEndpoints: 4
    0xFF,       // bInterfaceClass: Vendor Specific (0xff)
    0xFF,       // bInterfaceSubClass: 0xff
    0x00,       // bInterfaceProtocol: 0x00
    0,          // iInterface: 0

    // ENDPOINT DESCRIPTOR (EP1 OUT)
    7,          // bLength: 7
    5,          // bDescriptorType: ENDPOINT (0x05)
    0x01,       // bEndpointAddress: 0x01 OUT
    0x02,       // bmAttributes: Bulk-Transfer (0x02)
    0x00, 0x02, // wMaxPacketSize: 512
    0,          // bInterval: 0

    // ENDPOINT DESCRIPTOR (EP2 IN)
    7,          // bLength: 7
    5,          // bDescriptorType: ENDPOINT (0x05)
    0x82,       // bEndpointAddress: 0x82 IN
    0x02,       // bmAttributes: Bulk-Transfer (0x02)
    0x00, 0x02, // wMaxPacketSize: 512
    0,          // bInterval: 0

    // ENDPOINT DESCRIPTOR (EP3 OUT)
    7,          // bLength: 7
    5,          // bDescriptorType: ENDPOINT (0x05)
    0x03,       // bEndpointAddress: 0x03 OUT
    0x03,       // bmAttributes: Interrupt-Transfer (0x03)
    0x40, 0x00, // wMaxPacketSize: 64
    16,         // bInterval: 16

    // ENDPOINT DESCRIPTOR (EP4 IN)
    7,          // bLength: 7
    5,          // bDescriptorType: ENDPOINT (0x05)
    0x84,       // bEndpointAddress: 0x84 IN
    0x03,       // bmAttributes: Interrupt-Transfer (0x03)
    0x40, 0x00, // wMaxPacketSize: 64
    16          // bInterval: 16
};

// ---------------------------------------------------------
// STRINGS (Formato UTF-16 Little Endian)
// ---------------------------------------------------------
// Idioma (Inglés)
static const uint8_t string0[] = {4, 3, 0x09, 0x04}; 

// iManufacturer (1): "ZOOM Corporation"
static const uint8_t string1[] = {
    34, 3, 
    'Z', 0, 'O', 0, 'O', 0, 'M', 0, ' ', 0, 'C', 0, 'o', 0, 'r', 0, 
    'p', 0, 'o', 0, 'r', 0, 'a', 0, 't', 0, 'i', 0, 'o', 0, 'n', 0
}; 

// iProduct (2): "R24"
static const uint8_t string2[] = {
    8, 3, 
    'R', 0, '2', 0, '4', 0
}; 

// iSerialNumber (3): "0" (Genérico, ya que no está en la captura)
static const uint8_t string3[] = {
    4, 3, 
    '0', 0
};

static const uint8_t *const strings[] = {string0, string1, string2, string3};
static const uint8_t stringLens[] = {sizeof(string0), sizeof(string1), sizeof(string2), sizeof(string3)};


#endif