/*----------------------------------------------------------------------

                     This file is part of Freetribe

                https://github.com/bangcorrupt/freetribe

----------------------------------------------------------------------*/

/**
 * @file    usb.c
 *
 * @brief   Minimal bare-metal USB CDC (Serial) example application for Freetribe.
 * tested with tio /dev/ttyACM0
 *
 */

/*----- Includes -----------------------------------------------------*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csl_interrupt.h"
#include "csl_psc.h"
#include "csl_usb.h"
#include "freetribe.h"
#include "hw_psc_AM1808.h"
#include "hw_syscfg0_AM1808.h"
#include "hw_types.h"
#include "hw_usb.h"
#include "hw_usbphyGS60.h"

#define USB_0_OTGBASE SOC_USB_0_OTG_BASE
#define USB_0_INTR_MASK_SET 0x30
#define USB_0_INTR_SRC_CLEAR 0x28
#define USB_0_END_OF_INTR 0x3c

static const uint8_t *g_pEP0Data = 0;
static uint32_t g_uEP0Len = 0;
static uint8_t cdcConnected = 0;
uint32_t rxrdy_count = 0;
uint32_t csrl0Count = 0;
uint16_t g_last_csrl0 = 0;

static uint32_t rxrdy_ep1_count = 0;

static uint8_t isConfigured = 0;

static void ProcessCommand(char *cmd);

static void EP0SendData(void) {
    uint32_t sendLen = (g_uEP0Len > 64) ? 64 : g_uEP0Len;
    if (sendLen > 0) {
        USBEndpointDataPut(USB0_BASE, USB_EP_0, (uint8_t *)g_pEP0Data, sendLen);
        g_pEP0Data += sendLen;
        g_uEP0Len -= sendLen;
    }
    if (g_uEP0Len == 0) {
        USBEndpointDataSend(USB0_BASE, USB_EP_0, USB_TRANS_IN_LAST);
    } else {
        USBEndpointDataSend(USB0_BASE, USB_EP_0, USB_TRANS_IN);
    }
}

#ifndef USB0_BASE
#define USB0_BASE SOC_USB_0_BASE
#endif

#define USB_REQ_GET_STATUS 0x00
#define USB_REQ_CLEAR_FEATURE 0x01
#define USB_REQ_SET_FEATURE 0x03
#define USB_REQ_SET_ADDRESS 0x05
#define USB_REQ_GET_DESCRIPTOR 0x06
#define USB_REQ_SET_DESCRIPTOR 0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09

#define USB_DESC_DEVICE 0x01
#define USB_DESC_CONFIGURATION 0x02
#define USB_DESC_STRING 0x03
#define USB_DESC_DEVICE_QUAL 0x06

#define USB_CDC_SET_LINE_CODING 0x20
#define USB_CDC_GET_LINE_CODING 0x21
#define USB_CDC_SET_CONTROL_LINE_STATE 0x22

typedef struct __attribute__((packed)) {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} USB_SetupPacket;

// EP definitions
// NOTE: EP1 is configured as BULK OUT (host -> device) and
//       EP2 is configured as BULK IN (device -> host). Keep these
//       macros consistent with USB_REQ_SET_CONFIGURATION below and
//       with configDescriptor's endpoint addresses (0x01 OUT, 0x82 IN).
#define CDC_EP_OUT USB_EP_1
#define CDC_EP_IN USB_EP_2
#define CDC_EP_INT USB_EP_3

USB_SetupPacket g_setup_packets[100];


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

/* end strings*/

static uint16_t pendingAddress = 0;
static uint8_t pendingSetAddress = 0;
static uint8_t cdcLineCoding[7] = {0x00, 0xC2, 0x01, 0x00, 0, 0, 8}; // 115200 8N1

void USB0DeviceIntHandler(void) {
    uint16_t csrl0 = HWREGH(USB0_BASE + USB_0_CSRL0);
    g_last_csrl0 = csrl0;
    static uint16_t last_csrl0 = 0;

    // Only log if something changed or important
    if ((csrl0 & 0x11) || ((last_csrl0 & 0x02) && !(csrl0 & 0x02))) {
        // ft_printf("USB: CSR0=%04x\n", csrl0);
    }

    if (csrl0 & 0x10) {                          // SETUPEND
        HWREGB(USB0_BASE + USB_0_CSRL0) |= 0x80; // Clear SETUPEND
    }

    uint32_t statusCtrl = USBIntStatusControl(USB0_BASE);
    if (statusCtrl & USB_INTCTRL_RESET) {
        pendingAddress = 0;
        pendingSetAddress = 0;
        isConfigured = 0;
        USBDevAddrSet(USB0_BASE, 0);
    }
static uint32_t last_statusEp = 0;
    uint32_t statusEp = USBIntStatusEndpoint(USB0_BASE);
    if (last_statusEp!=statusEp) {
        last_statusEp=statusEp;
         ft_printf("USB: EP status=%08x\n", statusEp);
    }

    // EP0 handling
    if (csrl0 & 0x01) { // RXRDY
        USB_SetupPacket setup;
        unsigned int sz;
        USBEndpointDataGet(USB0_BASE, USB_EP_0, (uint8_t *)&setup, &sz);
        g_setup_packets[rxrdy_count] = setup;
        rxrdy_count++;

        if (sz == 8) {
            // Clear RXRDY
            USBDevEndpointDataAck(USB0_BASE, USB_EP_0, false);

            if ((setup.bmRequestType & 0x60) == 0) { // Standard Request
                switch (setup.bRequest) {
                case USB_REQ_GET_DESCRIPTOR: {
                    uint8_t type = setup.wValue >> 8;
                    uint8_t idx = setup.wValue & 0xFF;
                    const uint8_t *desc = 0;
                    uint16_t len = 0;
                    if (type == USB_DESC_DEVICE) {
                        desc = deviceDescriptor;
                        len = sizeof(deviceDescriptor);
                    } else if (type == USB_DESC_CONFIGURATION) {
                        desc = configDescriptor;
                        len = sizeof(configDescriptor);
                    } else if (type == USB_DESC_DEVICE_QUAL) {
                        desc = devQualDescriptor;
                        len = sizeof(devQualDescriptor);
                    } else if (type == USB_DESC_STRING && idx < 4) {
                        desc = strings[idx];
                        len = stringLens[idx];
                    }
                    if (desc) {
                        if (len > setup.wLength)
                            len = setup.wLength;
                        g_pEP0Data = desc;
                        g_uEP0Len = len;
                        EP0SendData();
                    } else {
                        USBDevEndpointStall(USB0_BASE, USB_EP_0, USB_EP_DEV_IN);
                    }
                    break;
                }
                case USB_REQ_GET_STATUS: {
                    static const uint8_t statusResp[2] = {0x00, 0x00}; // bus-powered, no remote wakeup
                    if (setup.wLength >= 2) {
                        g_pEP0Data = statusResp;
                        g_uEP0Len = 2;
                        EP0SendData();
                    } else {
                        USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true);
                    }
                    break;
                }
                case USB_REQ_SET_ADDRESS:
                    pendingAddress = setup.wValue;
                    pendingSetAddress = 1;
                    USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true);
                    break;
                case USB_REQ_SET_CONFIGURATION:
                    //
                    // EP1 OUT - Bulk
                    //
                    USBDevEndpointConfigSet(USB0_BASE, USB_EP_1, 512, USB_EP_MODE_BULK | USB_EP_DEV_OUT);

                    //
                    // EP2 IN - Bulk
                    //
                    USBDevEndpointConfigSet(USB0_BASE, USB_EP_2, 512, USB_EP_MODE_BULK | USB_EP_DEV_IN);

                    //
                    // EP3 OUT - Interrupt
                    //
                    USBDevEndpointConfigSet(USB0_BASE, USB_EP_3, 64, USB_EP_MODE_INT | USB_EP_DEV_OUT);

                    //
                    // EP4 IN - Interrupt
                    //
                    USBDevEndpointConfigSet(USB0_BASE, USB_EP_4, 64, USB_EP_MODE_INT | USB_EP_DEV_IN);

                    // capaz falta habilitar mas int aca pero no uso int en este ejemplo, solo poll
                    USBIntEnableEndpoint(USB0_BASE, (1 << 18)); // EP2 RX IRQ

                    isConfigured = 1;
                    USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true);
                    break;
                default:
                    USBDevEndpointStall(USB0_BASE, USB_EP_0, USB_EP_DEV_IN);
                    break;
                }
            } else if ((setup.bmRequestType & 0x60) == 0x20) { // Class request
                switch (setup.bRequest) {
                case USB_CDC_GET_LINE_CODING:
                    g_pEP0Data = cdcLineCoding;
                    g_uEP0Len = 7;
                    EP0SendData();
                    break;
                case USB_CDC_SET_LINE_CODING:
                case USB_CDC_SET_CONTROL_LINE_STATE:
                    cdcConnected = (setup.wValue & 0x01); // DTR
                    USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true);
                    break;
                default:
                    USBDevEndpointStall(USB0_BASE, USB_EP_0, USB_EP_DEV_IN);
                    break;
                }
            }
        } else if (sz == 0) {
            // Status phase ZLP from host
            USBDevEndpointDataAck(USB0_BASE, USB_EP_0, false);
        } else {
            // Unexpected data size
            USBDevEndpointDataAck(USB0_BASE, USB_EP_0, false);
        }
    } else if (((last_csrl0 & 0x02) && !(csrl0 & 0x02)) || ((last_csrl0 & 0x08) && !(csrl0 & 0x08))) {
        // TX Complete (TXRDY cleared) OR Status phase complete (DATAEND cleared)
        if (g_uEP0Len > 0) {
            EP0SendData();
        } else if (pendingSetAddress) {
            USBDevAddrSet(USB0_BASE, pendingAddress);
            pendingSetAddress = 0;
        }
    }

    last_csrl0 = csrl0;

    // ---- Bulk OUT data from host arrives on EP1 (RXCSRL1 / RXCOUNT1) ----
    // EP2 is configured as BULK IN (device -> host), so it never has
    // incoming data; polling RXCSRL2 here was the bug that made the
    // host hang (it filled EP1's OUT FIFO and nobody ever drained it).
    if (HWREGH(USB0_BASE + USB_0_RXCSRL1) & 0x01) { // RXRDY on EP1 OUT

        uint32_t count = HWREGH(USB0_BASE + USB_0_RXCOUNT1);
        rxrdy_ep1_count++;

        for (uint32_t i = 0; i < count; i++) {
            uint8_t c = HWREGB(USB0_BASE + 0x20 + (1 * 4));
        }

        // Clear RXRDY AFTER reading FIFO
        HWREGH(USB0_BASE + USB_0_RXCSRL1) &= ~0x01;
    }

    // Clear interrupt in AINTC and OTG wrapper to prevent infinite loop
    IntSystemStatusClear(SYS_INT_USB0);
    HWREG(USB_0_OTGBASE + USB_0_END_OF_INTR) = 0;
}

/*----- Command Processing -------------------------------------------*/

static void ProcessCommand(char *cmd) {}

#define BUTTON_PLAY 0x2

static void _button_callback(uint8_t index, bool state) {

    if (index == BUTTON_PLAY && state) {
        int i = 0;
        for (i = 0; i < rxrdy_count; i++) {
            USB_SetupPacket setup = g_setup_packets[i];

            ft_printf("SETUP(sz=%u): bmRequestType=0x%02x bRequest=0x%02x wValue=0x%04x wIndex=0x%04x wLength=%u\n", 666, setup.bmRequestType, setup.bRequest, setup.wValue, setup.wIndex, setup.wLength);
        }
    }
}

t_status app_init(void) {
    ////ft_printff("USB: Starting init...\n");
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_USB0, 0, PSC_MDCTL_NEXT_ENABLE);
    UsbPhyOn();
    // Fix CFGCHIP2 for 24MHz crystal and device mode
    uint32_t cfgchip2 = HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2);
    cfgchip2 &= ~(0x0000000F | (3 << 13) | (1 << 12)); // Clear REFFREQ, OTGMODE, CLKMUX
    cfgchip2 |= (2 << 0) | (2 << 13) | (1 << 6);       // Set REFFREQ=24MHz, OTGMODE=Device, PHY_PLLON=1
    HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) = cfgchip2;

    // Wait for PHY clock to be good
    ////ft_printff("USB: Waiting for PHY Clock...\n");
    int timeout = 1000000;
    while (!(HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) & (1 << 17)) && timeout--)
        ;

    cfgchip2 = HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2);
    ////ft_printff("USB: PHY ON & Configured (CFGCHIP2=%08x)\n", cfgchip2);

    // Enable High Speed
    HWREGB(USB0_BASE + USB_0_POWER) |= 0x20;

    IntRegister(SYS_INT_USB0, USB0DeviceIntHandler);
    IntChannelSet(SYS_INT_USB0, 2);
    IntSystemEnable(SYS_INT_USB0);

    USBIntEnableControl(USB0_BASE, USB_INTCTRL_RESET | USB_INTCTRL_DISCONNECT);
    USBIntEnableEndpoint(USB0_BASE, USB_INTEP_ALL);

    USBDevConnect(USB0_BASE);

    // Disable interrupts in TI OTG wrapper for now to test stability with polling
    HWREG(USB_0_OTGBASE + USB_0_INTR_MASK_SET) = 0x00;

    ft_register_panel_callback(BUTTON_EVENT, _button_callback);

    return SUCCESS;
}

#define GPIO_POWER_BUTTON 128

void app_run(void) {
    static int heartbeat = 0;

    heartbeat++;
    if (heartbeat >= 1000000) {
        heartbeat = 0;
        static int ledState = 0;
        ledState = !ledState;
        ft_set_led(LED_PLAY, ledState ? 255 : 0);
        ft_printf("USB: RXRDY count=%u , CSR0 count=%u, last CSR0=%04x, rxrdy_ep1_count=%u", rxrdy_count, csrl0Count, g_last_csrl0, rxrdy_ep1_count);

    }

    // Manual poll (safer than current interrupt config which causes hangs)
    USB0DeviceIntHandler();

    if (per_gpio_get_indexed(GPIO_POWER_BUTTON) == 0) {
        ft_shutdown();
    }
}