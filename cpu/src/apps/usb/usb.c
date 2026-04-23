/*----------------------------------------------------------------------

                     This file is part of Freetribe

                https://github.com/bangcorrupt/freetribe

----------------------------------------------------------------------*/

/**
 * @file    usb.c
 *
 * @brief   Minimal bare-metal USB CDC (Serial) example application for Freetribe.
 */

/*----- Includes -----------------------------------------------------*/

#include "freetribe.h"
#include "hw_types.h"
#include "hw_usb.h"
#include "csl_interrupt.h"
#include "csl_usb.h"
#include "csl_psc.h"
#include "hw_psc_AM1808.h"
#include "hw_usbphyGS60.h"
#include "hw_syscfg0_AM1808.h"

#ifndef USB0_BASE
#define USB0_BASE SOC_USB_0_BASE
#endif

#define USB_REQ_GET_STATUS        0x00
#define USB_REQ_CLEAR_FEATURE     0x01
#define USB_REQ_SET_FEATURE       0x03
#define USB_REQ_SET_ADDRESS       0x05
#define USB_REQ_GET_DESCRIPTOR    0x06
#define USB_REQ_SET_DESCRIPTOR    0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09

#define USB_DESC_DEVICE           0x01
#define USB_DESC_CONFIGURATION    0x02
#define USB_DESC_STRING           0x03
#define USB_DESC_DEVICE_QUAL      0x06

#define USB_CDC_SET_LINE_CODING         0x20
#define USB_CDC_GET_LINE_CODING         0x21
#define USB_CDC_SET_CONTROL_LINE_STATE  0x22

typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} USB_SetupPacket;

// EP definitions
#define CDC_EP_IN   USB_EP_1
#define CDC_EP_OUT  USB_EP_2
#define CDC_EP_INT  USB_EP_3

static const uint8_t deviceDescriptor[] = {
    18, 1, 0x00, 0x02, 0x02, 0x00, 0x00, 64,
    0x1C, 0x1C, 0x10, 0x00, 0x00, 0x02, 1, 2, 3, 1
};

static const uint8_t devQualDescriptor[] = {
    10, 6, 0x00, 0x02, 0x02, 0x00, 0x00, 64, 1, 0
};

static const uint8_t configDescriptor[] = {
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
    7, 5, 0x81, 2, 64, 0, 0
};

static const uint8_t string0[] = { 4, 3, 0x09, 0x04 };
static const uint8_t string1[] = { 20, 3, 'F',0,'r',0,'e',0,'e',0,'t',0,'r',0,'i',0,'b',0,'e',0 };
static const uint8_t string2[] = { 16, 3, 'C',0,'D',0,'C',0,' ',0,'A',0,'C',0,'M',0 };
static const uint8_t string3[] = { 10, 3, '1',0,'2',0,'3',0,'4',0 };

static const uint8_t* const strings[] = { string0, string1, string2, string3 };
static const uint8_t stringLens[] = { sizeof(string0), sizeof(string1), sizeof(string2), sizeof(string3) };

static uint16_t pendingAddress = 0;
static uint8_t pendingSetAddress = 0;
static uint8_t cdcLineCoding[7] = {0x00, 0xC2, 0x01, 0x00, 0, 0, 8}; // 115200 8N1
static uint8_t isConfigured = 0;

void USB0DeviceIntHandler(void) {
    uint32_t statusCtrl = USBIntStatusControl(USB0_BASE);
    uint32_t statusEp = USBIntStatusEndpoint(USB0_BASE);

    if (statusCtrl & USB_INTCTRL_RESET) {
        pendingAddress = 0;
        pendingSetAddress = 0;
        isConfigured = 0;
        USBDevAddrSet(USB0_BASE, 0);
    }

    if (statusEp & 0x00010001) { // EP0 Rx/Tx
        USB_SetupPacket setup;
        unsigned int sz;
        if (USBEndpointDataAvail(USB0_BASE, USB_EP_0)) {
            USBEndpointDataGet(USB0_BASE, USB_EP_0, (uint8_t*)&setup, &sz);
            
            if (sz == 8) {
                if ((setup.bmRequestType & 0x60) == 0) { // Standard Request
                    switch(setup.bRequest) {
                        case USB_REQ_GET_DESCRIPTOR: {
                            uint8_t type = setup.wValue >> 8;
                            uint8_t idx = setup.wValue & 0xFF;
                            const uint8_t* desc = 0;
                            uint16_t len = 0;
                            if (type == USB_DESC_DEVICE) {
                                desc = deviceDescriptor; len = sizeof(deviceDescriptor);
                            } else if (type == USB_DESC_CONFIGURATION) {
                                desc = configDescriptor; len = sizeof(configDescriptor);
                            } else if (type == USB_DESC_DEVICE_QUAL) {
                                desc = devQualDescriptor; len = sizeof(devQualDescriptor);
                            } else if (type == USB_DESC_STRING && idx < 4) {
                                desc = strings[idx]; len = stringLens[idx];
                            }
                            if (desc) {
                                if (len > setup.wLength) len = setup.wLength;
                                USBEndpointDataPut(USB0_BASE, USB_EP_0, (uint8_t*)desc, len);
                                USBEndpointDataSend(USB0_BASE, USB_EP_0, USB_TRANS_IN);
                            } else {
                                USBDevEndpointStall(USB0_BASE, USB_EP_0, USB_EP_DEV_IN);
                            }
                            break;
                        }
                        case USB_REQ_SET_ADDRESS:
                            pendingAddress = setup.wValue;
                            pendingSetAddress = 1;
                            USBEndpointDataSend(USB0_BASE, USB_EP_0, USB_TRANS_IN);
                            break;
                        case USB_REQ_SET_CONFIGURATION:
                            USBDevEndpointConfigSet(USB0_BASE, USB_EP_1, 64, USB_EP_MODE_BULK | USB_EP_DEV_IN);
                            USBDevEndpointConfigSet(USB0_BASE, USB_EP_2, 64, USB_EP_MODE_BULK | USB_EP_DEV_OUT);
                            isConfigured = 1;
                            USBEndpointDataSend(USB0_BASE, USB_EP_0, USB_TRANS_IN);
                            break;
                        case USB_REQ_GET_CONFIGURATION: {
                            uint8_t cfg = isConfigured;
                            USBEndpointDataPut(USB0_BASE, USB_EP_0, &cfg, 1);
                            USBEndpointDataSend(USB0_BASE, USB_EP_0, USB_TRANS_IN);
                            break;
                        }
                        default:
                            USBDevEndpointStall(USB0_BASE, USB_EP_0, USB_EP_DEV_IN);
                            break;
                    }
                } else if ((setup.bmRequestType & 0x60) == 0x20) { // Class request
                    switch(setup.bRequest) {
                        case USB_CDC_SET_LINE_CODING:
                            USBEndpointDataSend(USB0_BASE, USB_EP_0, USB_TRANS_IN);
                            break;
                        case USB_CDC_GET_LINE_CODING:
                            USBEndpointDataPut(USB0_BASE, USB_EP_0, cdcLineCoding, 7);
                            USBEndpointDataSend(USB0_BASE, USB_EP_0, USB_TRANS_IN);
                            break;
                        case USB_CDC_SET_CONTROL_LINE_STATE:
                            USBEndpointDataSend(USB0_BASE, USB_EP_0, USB_TRANS_IN);
                            break;
                        default:
                            USBDevEndpointStall(USB0_BASE, USB_EP_0, USB_EP_DEV_IN);
                            break;
                    }
                }
            } else {
                USBEndpointDataSend(USB0_BASE, USB_EP_0, USB_TRANS_IN);
            }
        } else {
            if (pendingSetAddress) {
                USBDevAddrSet(USB0_BASE, pendingAddress);
                pendingSetAddress = 0;
            }
        }
    }

    if (statusEp & 0x00040000) { // EP2 Rx
        uint8_t rxBuf[64];
        unsigned int rxSz = 0;
        USBEndpointDataGet(USB0_BASE, USB_EP_2, rxBuf, &rxSz);
        if (rxSz > 0) {
            USBEndpointDataPut(USB0_BASE, USB_EP_1, rxBuf, rxSz);
            USBEndpointDataSend(USB0_BASE, USB_EP_1, USB_TRANS_IN);
        }
    }
}

t_status app_init(void) {
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_USB0, 0, PSC_MDCTL_NEXT_ENABLE);
    UsbPhyOn();

    IntRegister(SYS_INT_USB0, USB0DeviceIntHandler);
    IntChannelSet(SYS_INT_USB0, 2);
    IntSystemEnable(SYS_INT_USB0);
    
    USBIntEnableControl(USB0_BASE, USB_INTCTRL_RESET | USB_INTCTRL_DISCONNECT);
    USBIntEnableEndpoint(USB0_BASE, USB_INTEP_ALL);

    USBDevConnect(USB0_BASE);
    return SUCCESS;
}

#define GPIO_POWER_BUTTON 128
void app_run(void) {
        if (per_gpio_get_indexed(GPIO_POWER_BUTTON) == 0) {

        ft_shutdown();

        // Should never reach here.
    }

}
