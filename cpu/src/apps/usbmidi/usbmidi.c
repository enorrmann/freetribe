/*----------------------------------------------------------------------

                     This file is part of Freetribe

                https://github.com/bangcorrupt/freetribe

----------------------------------------------------------------------*/

/**
 * @file    usb.c
 *
 * @brief   Minimal bare-metal USB CDC (Serial) example application for
 * Freetribe.
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

/*----- USB Serial Buffer --------------------------------------------*/

#define USB_SERIAL_BUF_SIZE 512
static uint8_t g_usbRxBuf[USB_SERIAL_BUF_SIZE];
static uint32_t g_usbRxHead = 0;
static uint32_t g_usbRxTail = 0;

static void usb_rx_push(uint8_t c) {
    uint32_t next = (g_usbRxHead + 1) % USB_SERIAL_BUF_SIZE;
    if (next != g_usbRxTail) {
        g_usbRxBuf[g_usbRxHead] = c;
        g_usbRxHead = next;
    }
}

static int usb_rx_pop(uint8_t *c) {
    if (g_usbRxHead == g_usbRxTail)
        return 0;
    *c = g_usbRxBuf[g_usbRxTail];
    g_usbRxTail = (g_usbRxTail + 1) % USB_SERIAL_BUF_SIZE;
    return 1;
}

static uint8_t isConfigured = 0;

void USBSerial_Send(const uint8_t *data, uint32_t len) {
    if (!isConfigured)
        return;

    while (len > 0) {
        uint32_t sendLen = (len > 64) ? 64 : len;

        // Esperar que el endpoint esté libre
        while (HWREGH(USB0_BASE + USB_0_TXCSRL1) & 0x01)
            ;

        USBEndpointDataPut(USB0_BASE, USB_EP_1, (uint8_t *)data, sendLen);

        // Set TXRDY manualmente (más robusto)
        HWREGH(USB0_BASE + USB_0_TXCSRL1) |= 0x01;

        data += sendLen;
        len -= sendLen;
    }
}

void USBSerial_Printf(const char *format, ...) {
    va_list ap;
    static char str[256];

    va_start(ap, format);
    vsnprintf(str, sizeof(str), format, ap);
    va_end(ap);

    USBSerial_Send((uint8_t *)str, strlen(str));
}

static uint32_t usb_rx_count(void) {
    return (g_usbRxHead + USB_SERIAL_BUF_SIZE - g_usbRxTail) % USB_SERIAL_BUF_SIZE;
}

static uint8_t USBMidi_GetMessageLength(uint8_t status) {
    if (status < 0x80)
        return 0;
    if (status < 0xF0) {
        uint8_t code = status & 0xF0;
        if (code == 0xC0 || code == 0xD0)
            return 2;
        return 3;
    }
    if (status == 0xF1 || status == 0xF3)
        return 2;
    if (status == 0xF2)
        return 3;
    if (status >= 0xF8)
        return 1;
    if (status == 0xF6 || status == 0xF7)
        return 1;
    return 1;
}

static uint8_t USBMidi_GetCIN(uint8_t status, uint32_t len) {
    if (status < 0xF0) {
        uint8_t code = status & 0xF0;
        switch (code) {
        case 0x80:
            return 0x8;
        case 0x90:
            return 0x9;
        case 0xA0:
            return 0xA;
        case 0xB0:
            return 0xB;
        case 0xC0:
            return 0xC;
        case 0xD0:
            return 0xD;
        case 0xE0:
            return 0xE;
        default:
            return 0xF;
        }
    }
    switch (status) {
    case 0xF0:
        return 0x4; // SysEx starts and continues
    case 0xF1:
    case 0xF3:
        return 0x2;
    case 0xF2:
        return 0x3;
    case 0xF6:
    case 0xF8:
    case 0xF9:
    case 0xFA:
    case 0xFB:
    case 0xFC:
    case 0xFE:
    case 0xFF:
        return 0xF;
    case 0xF7:
        return (len == 1) ? 0x5 : 0x6;
    default:
        return 0xF;
    }
}

static void USBMidi_SendPacket(uint8_t cin, const uint8_t *midiData,
                              uint32_t midiLen) {
    uint8_t packet[4] = {0};
    packet[0] = cin;
    if (midiLen > 0)
        packet[1] = midiData[0];
    if (midiLen > 1)
        packet[2] = midiData[1];
    if (midiLen > 2)
        packet[3] = midiData[2];
    USBSerial_Send(packet, 4);
}

void USBMidi_Send(const uint8_t *midiData, uint32_t midiLen) {
    if (!isConfigured || midiData == NULL || midiLen == 0)
        return;

    uint32_t offset = 0;
    while (offset < midiLen) {
        uint8_t status = midiData[offset];
        if (status < 0x80) {
            offset++;
            continue;
        }
        uint8_t msgLen = USBMidi_GetMessageLength(status);
        if (msgLen == 0 || offset + msgLen > midiLen)
            break;
        uint8_t cin = USBMidi_GetCIN(status, msgLen);
        USBMidi_SendPacket(cin, midiData + offset, msgLen);
        offset += msgLen;
    }
}

void midi_send_note(uint8_t channel, uint8_t note, uint8_t velocity, uint8_t on_off) {
    uint8_t msg[3];
    msg[0] = (on_off ? 0x90 : 0x80) | (channel & 0x0F);
    msg[1] = note & 0x7F;
    msg[2] = velocity & 0x7F;
    USBMidi_Send(msg, 3);
}

static void USBMidi_HandleReceivedMidi(const uint8_t *midiData,
                                      uint32_t midiLen) {
    if (midiLen == 0)
        return;
    // Placeholder: process standard MIDI data received from host.
    // For now, echo it back to the host as a USB MIDI message.
    USBMidi_Send(midiData, midiLen);
}

static void USBMidi_DecodePacket(const uint8_t packet[4]) {
    uint8_t cin = packet[0] & 0x0F;
    uint8_t midiLen = 0;
    switch (cin) {
    case 0x2:
    case 0x6:
    case 0xA:
    case 0xB:
    case 0xC:
    case 0xD:
    case 0xE:
        midiLen = 3;
        break;
    case 0x3:
    case 0xF:
        midiLen = 1;
        break;
    case 0x4:
        midiLen = 2;
        break;
    case 0x5:
        midiLen = 3;
        break;
    default:
        return;
    }
    USBMidi_HandleReceivedMidi(packet + 1, midiLen);
}

static void ProcessUSBSerial(const uint8_t *midiData, uint32_t midiLen) {
    if (midiData == NULL || midiLen == 0)
        return;
    USBMidi_HandleReceivedMidi(midiData, midiLen);
}

static void ProcessUSBMidi(void) {
    while (usb_rx_count() >= 4) {
        uint8_t packet[4];
        for (int i = 0; i < 4; i++) {
            usb_rx_pop(&packet[i]);
        }
        USBMidi_DecodePacket(packet);
    }
}

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

static void USBSetDeviceType(void) {
    USBDevEndpointConfigSet(USB0_BASE, USB_EP_1, 64,
                            USB_EP_MODE_BULK | USB_EP_DEV_IN);
    USBDevEndpointConfigSet(USB0_BASE, USB_EP_2, 64,
                            USB_EP_MODE_BULK | USB_EP_DEV_OUT);

    USBIntEnableEndpoint(USB0_BASE, (1 << 18)); // EP2 RX IRQ

    isConfigured = 1;
    USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true);
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
#define CDC_EP_IN USB_EP_1
#define CDC_EP_OUT USB_EP_2
#define CDC_EP_INT USB_EP_3

static const uint8_t deviceDescriptor[] = {18,   1,    0x10, 0x01, 0x00, 0x00,
                                           0x00, 64,   0x1C, 0x1C, 0x10, 0x00,
                                           0x00, 0x02, 1,    2,    3,    1};

static const uint8_t devQualDescriptor[] = {10,   6,    0x00, 0x02, 0x02,
                                            0x00, 0x00, 64,   1,    0};

static const uint8_t configDescriptor[] = {
    // Config
    9, 2, 97, 0, 2, 1, 0, 0x80, 50,
    // Interface 0 (Audio Control)
    9, 4, 0, 0, 0, 0x01, 0x01, 0x00, 0,
    // Audio Control Header
    9, 0x24, 0x01, 0x00, 0x01, 0x09, 0x00, 0x01, 0x01,
    // Interface 1 (MIDI Streaming)
    9, 4, 1, 0, 2, 0x01, 0x03, 0x00, 0,
    // MIDI Streaming Header
    7, 0x24, 0x01, 0x00, 0x01, 0x3D, 0x00,
    // MIDI IN Jack (Embedded)
    6, 0x24, 0x02, 0x01, 0x01, 0x00,
    // MIDI IN Jack (External)
    6, 0x24, 0x02, 0x02, 0x02, 0x00,
    // MIDI OUT Jack (Embedded)
    9, 0x24, 0x03, 0x01, 0x03, 0x01, 0x02, 0x01, 0x00,
    // MIDI OUT Jack (External)
    9, 0x24, 0x03, 0x02, 0x04, 0x01, 0x01, 0x01, 0x00,
    // Endpoint Bulk OUT
    7, 5, 0x02, 2, 64, 0, 0,
    // Class-specific Bulk OUT
    5, 0x25, 0x01, 0x01, 0x01,
    // Endpoint Bulk IN
    7, 5, 0x81, 2, 64, 0, 0,
    // Class-specific Bulk IN
    5, 0x25, 0x01, 0x01, 0x03};

static const uint8_t string0[] = {4, 3, 0x09, 0x04};
static const uint8_t string1[] = {20,  3, 'F', 0, 'r', 0, 'e', 0, 'e', 0,
                                  't', 0, 'r', 0, 'i', 0, 'b', 0, 'e', 0};
static const uint8_t string2[] = {16,  3, 'F', 0, 'T', 0, 'B', 0,
                                  ' ', 0, 'M', 0, 'I', 0, 'D', 0};
static const uint8_t string3[] = {10, 3, '1', 0, '2', 0, '3', 0, '4', 0};

static const uint8_t *const strings[] = {string0, string1, string2, string3};
static const uint8_t stringLens[] = {sizeof(string0), sizeof(string1),
                                     sizeof(string2), sizeof(string3)};

static uint16_t pendingAddress = 0;
static uint8_t pendingSetAddress = 0;
static uint8_t cdcLineCoding[7] = {0x00, 0xC2, 0x01, 0x00,
                                   0,    0,    8}; // 115200 8N1

void USB0DeviceIntHandler(void) {
    uint16_t csrl0 = HWREGH(USB0_BASE + USB_0_CSRL0);
    static uint16_t last_csrl0 = 0;

    // Only log if something changed or important
    if ((csrl0 & 0x11) || ((last_csrl0 & 0x02) && !(csrl0 & 0x02))) {
        // ////ft_printff("USB: CSR0=%04x\n", csrl0);
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

    uint32_t statusEp = USBIntStatusEndpoint(USB0_BASE);

    // EP0 handling
    if (csrl0 & 0x01) { // RXRDY
        USB_SetupPacket setup;
        unsigned int sz;
        USBEndpointDataGet(USB0_BASE, USB_EP_0, (uint8_t *)&setup, &sz);

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
                case USB_REQ_SET_ADDRESS:
                    pendingAddress = setup.wValue;
                    pendingSetAddress = 1;
                    USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true);
                    break;
                case USB_REQ_SET_CONFIGURATION:
                    USBSetDeviceType();
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
    } else if (((last_csrl0 & 0x02) && !(csrl0 & 0x02)) ||
               ((last_csrl0 & 0x08) && !(csrl0 & 0x08))) {
        // TX Complete (TXRDY cleared) OR Status phase complete (DATAEND
        // cleared)
        if (g_uEP0Len > 0) {
            EP0SendData();
        } else if (pendingSetAddress) {
            USBDevAddrSet(USB0_BASE, pendingAddress);
            pendingSetAddress = 0;
        }
    }

    last_csrl0 = csrl0;

    if (HWREGH(USB0_BASE + USB_0_RXCSRL2) & 0x01) { // RXRDY

        // USBSerial_Printf("RX IRQ\r\n");  //acas1

        uint32_t count = HWREGH(USB0_BASE + USB_0_RXCOUNT2);

        for (uint32_t i = 0; i < count; i++) {
            uint8_t c = HWREGB(USB0_BASE + 0x20 + (2 * 4));
            usb_rx_push(c);
        }

        // Clear RXRDY AFTER reading FIFO
        HWREGH(USB0_BASE + USB_0_RXCSRL2) &= ~0x01;
    }

    // Clear interrupt in AINTC and OTG wrapper to prevent infinite loop
    IntSystemStatusClear(SYS_INT_USB0);
    HWREG(USB_0_OTGBASE + USB_0_END_OF_INTR) = 0;
}

/*----- Command Processing -------------------------------------------*/

static void ProcessCommand(char *cmd) {
    if (strlen(cmd) == 0)
        return;

    if (strcmp(cmd, "help") == 0) {
        USBSerial_Printf("Available commands:\r\n");
        USBSerial_Printf("  help          - Show this help\r\n");
        USBSerial_Printf("  info          - Show device info\r\n");
        USBSerial_Printf("  echo <msg>    - Echo message\r\n");
        USBSerial_Printf(
            "  led <val>     - Set Play LED brightness (0-255)\r\n");
        USBSerial_Printf("  reboot        - Shutdown system\r\n");
    } else if (strcmp(cmd, "info") == 0) {
        USBSerial_Printf("Manufacturer: Freetribe\r\n");
        USBSerial_Printf("Product: USB MIDI Device\r\n");
        USBSerial_Printf("Serial: 1234\r\n");
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        USBSerial_Printf("%s\r\n", cmd + 5);
    } else if (strncmp(cmd, "led ", 4) == 0) {
        int val = atoi(cmd + 4);
        ft_set_led(LED_PLAY, (uint8_t)val);
        USBSerial_Printf("LED Play set to %d\r\n", val);
    } else if (strcmp(cmd, "reboot") == 0) {
        USBSerial_Printf("Rebooting...\r\n");
        ft_shutdown();
    } else {
        USBSerial_Printf("Unknown command: %s\r\n", cmd);
    }
    USBSerial_Printf("> ");
}


t_status app_init(void) {
    ////ft_printff("USB: Starting init...\n");
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_USB0, 0, PSC_MDCTL_NEXT_ENABLE);
    UsbPhyOn();
    // Fix CFGCHIP2 for 24MHz crystal and device mode
    uint32_t cfgchip2 = HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2);
    cfgchip2 &=
        ~(0x0000000F | (3 << 13) | (1 << 12)); // Clear REFFREQ, OTGMODE, CLKMUX
    cfgchip2 |= (2 << 0) | (2 << 13) |
                (1 << 6); // Set REFFREQ=24MHz, OTGMODE=Device, PHY_PLLON=1
    HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) = cfgchip2;

    // Wait for PHY clock to be good
    ////ft_printff("USB: Waiting for PHY Clock...\n");
    int timeout = 1000000;
    while (!(HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) & (1 << 17)) &&
           timeout--)
        ;

    cfgchip2 = HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2);
    ////ft_printff("USB: PHY ON & Configured (CFGCHIP2=%08x)\n", cfgchip2);

    // Force Full Speed (disable High Speed)
    HWREGB(USB0_BASE + USB_0_POWER) &= ~0x20;
    ////ft_printff("USB: Forced Full Speed\n");

    IntRegister(SYS_INT_USB0, USB0DeviceIntHandler);
    IntChannelSet(SYS_INT_USB0, 2);
    IntSystemEnable(SYS_INT_USB0);

    USBIntEnableControl(USB0_BASE, USB_INTCTRL_RESET | USB_INTCTRL_DISCONNECT);
    USBIntEnableEndpoint(USB0_BASE, USB_INTEP_ALL);

    USBDevConnect(USB0_BASE);

    // Disable interrupts in TI OTG wrapper for now to test stability with
    // polling
    HWREG(USB_0_OTGBASE + USB_0_INTR_MASK_SET) = 0x00;

    return SUCCESS;
}

#define GPIO_POWER_BUTTON 128

void app_run(void) {
    static int heartbeat = 0;
    heartbeat++;
    if (heartbeat >= 100000) {
        heartbeat = 0;
        static int ledState = 0;
        ledState = !ledState;
        ft_set_led(LED_PLAY, ledState ? 255 : 0);
        //            USBSerial_Printf("TEST"); // este funciona
        midi_send_note(0, 60, 127, 1); // Note On
    }

    // Manual poll (safer than current interrupt config which causes hangs)
    USB0DeviceIntHandler();
    ProcessUSBMidi();

    if (per_gpio_get_indexed(GPIO_POWER_BUTTON) == 0) {
        ft_shutdown();
    }
}
