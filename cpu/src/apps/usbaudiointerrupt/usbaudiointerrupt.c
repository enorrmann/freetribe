/*----------------------------------------------------------------------

                     This file is part of Freetribe

                https://github.com/bangcorrupt/freetribe

----------------------------------------------------------------------*/

/**
 * @file    usbaudio.c
 *
 * @brief   Minimal bare-metal USB Audio example application for Freetribe.
 *
 * Fixes applied:
 *  1. GET_INTERFACE: dangling pointer replaced with static buffer.
 *  2. USBAudio_SendCapture: only sends when capture interface alt-setting == 1.
 *  3. USBAudio_ProcessOut: only reads when playback interface alt-setting == 1.
 *  4. SET_INTERFACE: now configures / deconfigures endpoints on alt transitions.
 *  5. app_run: removed double-call to USB0DeviceIntHandler + ProcessUSBAudio;
 *     audio processing lives exclusively in ProcessUSBAudio.
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

/*----- Defines -------------------------------------------------------*/

#define USB_0_OTGBASE        SOC_USB_0_OTG_BASE
#define USB_0_INTR_MASK_SET  0x30
#define USB_0_INTR_SRC_CLEAR 0x28
#define USB_0_END_OF_INTR    0x3c

#ifndef USB0_BASE
#define USB0_BASE SOC_USB_0_BASE
#endif

#define USB_REQ_GET_STATUS       0x00
#define USB_REQ_CLEAR_FEATURE    0x01
#define USB_REQ_SET_FEATURE      0x03
#define USB_REQ_SET_ADDRESS      0x05
#define USB_REQ_GET_DESCRIPTOR   0x06
#define USB_REQ_SET_DESCRIPTOR   0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09
#define USB_REQ_GET_INTERFACE    0x0A
#define USB_REQ_SET_INTERFACE    0x0B

#define USB_DESC_DEVICE      0x01
#define USB_DESC_CONFIGURATION 0x02
#define USB_DESC_STRING      0x03
#define USB_DESC_DEVICE_QUAL 0x06

#define AUDIO_EP_IN              USB_EP_1
#define AUDIO_EP_OUT             USB_EP_2
#define AUDIO_EP_MAX_PACKET_SIZE 192

/* Interface indices in g_interfaceAltSetting[] */
#define IFACE_AUDIO_CONTROL   0
#define IFACE_PLAYBACK        1   /* Interface 1: host → device */
#define IFACE_CAPTURE         2   /* Interface 2: device → host */

/*----- Types ---------------------------------------------------------*/

typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} USB_SetupPacket;

/*----- Static descriptors --------------------------------------------*/

static const uint8_t deviceDescriptor[] = {
    18, 1, 0x10, 0x01, 0x00, 0x00,
    0x00, 64, 0x1C, 0x1C, 0x10, 0x00,
    0x00, 0x02, 1, 2, 3, 1
};

static const uint8_t devQualDescriptor[] = {
    10, 6, 0x00, 0x02, 0x02,
    0x00, 0x00, 64, 1, 0
};

static const uint8_t configDescriptor[] = {
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
    /* Interface 1 alt 0 (Audio Streaming playback – zero-bandwidth) */
    9, 4, 1, 0, 0, 0x01, 0x02, 0x00, 0,
    /* Interface 1 alt 1 (Audio Streaming playback – active) */
    9, 4, 1, 1, 1, 0x01, 0x02, 0x00, 0,
    /* Audio Streaming Header (playback) */
    7, 0x24, 0x01, 0x01, 0x01, 0x01, 0x00,
    /* Format Type I */
    11, 0x24, 0x02, 0x01, 0x02, 0x02, 16, 1, 0x80, 0xBB, 0x00,
    /* Standard Endpoint OUT */
    7, 5, 0x02, 1, 192, 0, 1,
    /* Class-specific Endpoint OUT */
    7, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00,
    /* Interface 2 alt 0 (Audio Streaming capture – zero-bandwidth) */
    9, 4, 2, 0, 0, 0x01, 0x02, 0x00, 0,
    /* Interface 2 alt 1 (Audio Streaming capture – active) */
    9, 4, 2, 1, 1, 0x01, 0x02, 0x00, 0,
    /* Audio Streaming Header (capture) */
    7, 0x24, 0x01, 0x06, 0x01, 0x01, 0x00,
    /* Format Type I */
    11, 0x24, 0x02, 0x01, 0x02, 0x02, 16, 1, 0x80, 0xBB, 0x00,
    /* Standard Endpoint IN */
    7, 5, 0x81, 1, 192, 0, 1,
    /* Class-specific Endpoint IN */
    7, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00
};

static void tripleAck() {

    // . LIMPIAR EL AINTC (Nivel 3 - Sistema)
    IntSystemStatusClear(SYS_INT_USB0);

    // . EOI (End Of Interrupt) - EL "KICK" FINAL
    // Sin esto, el Wrapper nunca libera la línea de IRQ hacia el CPU
    HWREG(USB_0_OTGBASE + USB_0_END_OF_INTR) = 0;

    // . LIMPIAR EL WRAPPER DE TI (Nivel 2)
    // El registro INTR_SRC_CLEAR (0x28) requiere escribir 1s para limpiar
    // Le pasamos statusCtrl para limpiar los bits de RESET, SUSPEND, etc.
    HWREG(USB_0_OTGBASE + USB_0_INTR_SRC_CLEAR) = 0xFFFFFFFF;
}

static const uint8_t string0[] = {4, 3, 0x09, 0x04};
static const uint8_t string1[] = {
    20, 3, 'F',0,'r',0,'e',0,'e',0,'t',0,'r',0,'i',0,'b',0,'e',0
};
static const uint8_t string2[] = {
    20, 3, 'F',0,'T',0,'B',0,' ',0,'A',0,'u',0,'d',0,'i',0,'o',0
};
static const uint8_t string3[] = {10, 3, '1',0,'2',0,'3',0,'4',0};

static const uint8_t *const strings[]  = {string0, string1, string2, string3};
static const uint8_t stringLens[]      = {
    sizeof(string0), sizeof(string1), sizeof(string2), sizeof(string3)
};

/*----- State ---------------------------------------------------------*/

static const uint8_t *g_pEP0Data   = 0;
static uint32_t       g_uEP0Len    = 0;

static uint8_t  isConfigured = 0;

static uint8_t  g_audioOutPacket[AUDIO_EP_MAX_PACKET_SIZE];
static uint32_t g_audioOutLen = 0;
static uint8_t  g_audioInPacket[AUDIO_EP_MAX_PACKET_SIZE];
static uint32_t g_squarePhase = 0;

static uint16_t pendingAddress    = 0;
static uint8_t  pendingSetAddress = 0;

/*
 * g_interfaceAltSetting[n]:
 *   0  = zero-bandwidth (interface inactive, no isochronous traffic)
 *   1  = operational alt setting
 */
static uint8_t g_interfaceAltSetting[3] = {0, 0, 0};

/*----- Internal helpers ----------------------------------------------*/

static void EP0SendData(void) {
    uint32_t sendLen = (g_uEP0Len > 64) ? 64 : g_uEP0Len;
    if (sendLen > 0) {
        USBEndpointDataPut(USB0_BASE, USB_EP_0, (uint8_t *)g_pEP0Data, sendLen);
        g_pEP0Data += sendLen;
        g_uEP0Len  -= sendLen;
    }
    if (g_uEP0Len == 0) {
        USBEndpointDataSend(USB0_BASE, USB_EP_0, USB_TRANS_IN_LAST);
    } else {
        USBEndpointDataSend(USB0_BASE, USB_EP_0, USB_TRANS_IN);
    }
}

/*
 * USBSetDeviceType – called once on SET_CONFIGURATION.
 * Configures both endpoints in isochronous mode and marks device ready.
 */
static void USBSetDeviceType(void) {
    USBDevEndpointConfigSet(USB0_BASE, AUDIO_EP_IN,  AUDIO_EP_MAX_PACKET_SIZE,
                            USB_EP_MODE_ISOC | USB_EP_DEV_IN  | USB_EP_AUTO_SET);
    USBDevEndpointConfigSet(USB0_BASE, AUDIO_EP_OUT, AUDIO_EP_MAX_PACKET_SIZE,
                            USB_EP_MODE_ISOC | USB_EP_DEV_OUT |
                            USB_EP_AUTO_REQUEST | USB_EP_AUTO_CLEAR);

    /* Enable EP2 RX interrupt */
    USBIntEnableEndpoint(USB0_BASE, (1 << 18));

    isConfigured = 1;
    USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true);
}

/*
 * USBActivatePlayback / USBDeactivatePlayback
 *
 * FIX 4: Manage endpoint state when SET_INTERFACE changes the alt setting
 * for the playback interface (Interface 1).
 *
 * alt=0 → zero-bandwidth, flush any pending data, do not process OUT packets.
 * alt=1 → re-arm the endpoint for isochronous OUT transfers.
 */
static void USBActivatePlayback(void) {
    USBDevEndpointConfigSet(USB0_BASE, AUDIO_EP_OUT, AUDIO_EP_MAX_PACKET_SIZE,
                            USB_EP_MODE_ISOC | USB_EP_DEV_OUT |
                            USB_EP_AUTO_REQUEST | USB_EP_AUTO_CLEAR);
    g_audioOutLen = 0;
}

static void USBDeactivatePlayback(void) {
    /* Flush any data sitting in the RX FIFO */
    USBDevEndpointDataAck(USB0_BASE, AUDIO_EP_OUT, false);
    g_audioOutLen = 0;
}

/*
 * USBActivateCapture / USBDeactivateCapture
 *
 * FIX 4 continued: Manage state for the capture interface (Interface 2).
 *
 * Note: isochronous endpoints do NOT support the HALT feature per USB spec
 * (USB 2.0 §9.4.5), so stall/unstall calls are intentionally omitted.
 * The alt-setting guard in USBAudio_SendCapture() is sufficient to stop
 * the device from sending IN packets when the interface is at alt=0.
 */
static void USBActivateCapture(void) {
    g_squarePhase = 0;
    g_audioOutLen = 0;
}

static void USBDeactivateCapture(void) {
    /* Nothing hardware-level needed for ISO endpoints.
     * The guard in USBAudio_SendCapture() stops transmission. */
    g_audioOutLen = 0;
}

/*----- Audio helpers -------------------------------------------------*/

static void USBAudio_GenerateSquareWave(void) {
    const int16_t  amplitude  = 0x4000;
    const uint32_t sampleRate = 48000;
    const uint32_t frequency  = 440 / 2;

    for (uint32_t frame = 0; frame < AUDIO_EP_MAX_PACKET_SIZE / 4; frame++) {
        int16_t  sample = (g_squarePhase < (sampleRate / 2)) ? amplitude : -amplitude;
        uint32_t index  = frame * 4;

        g_audioInPacket[index + 0] = (uint8_t)(sample & 0xFF);
        g_audioInPacket[index + 1] = (uint8_t)((sample >> 8) & 0xFF);
        /* Duplicate left channel to right */
        g_audioInPacket[index + 2] = g_audioInPacket[index + 0];
        g_audioInPacket[index + 3] = g_audioInPacket[index + 1];

        g_squarePhase += frequency;
        if (g_squarePhase >= sampleRate)
            g_squarePhase -= sampleRate;
    }
}

static void USBAudio_HandleOutPacket(const uint8_t *data, uint32_t len) {
    if (len > AUDIO_EP_MAX_PACKET_SIZE)
        len = AUDIO_EP_MAX_PACKET_SIZE;
    memcpy(g_audioOutPacket, data, len);
    g_audioOutLen = len;
}

/*
 * USBAudio_SendCapture
 *
 * FIX 2: Only send isochronous IN data when the capture interface (Interface 2)
 * has been activated by the host (alt-setting == 1).
 * Previously this ran unconditionally, which kept EP_IN busy even before the
 * host opened the capture stream, causing aplay to report the device as busy.
 */
static void USBAudio_SendCapture(void) {
    if (!isConfigured)
        return;

    /* FIX 2: respect the USB Audio Class lifecycle */
    if (g_interfaceAltSetting[IFACE_CAPTURE] != 1)
        return;

    if (g_audioOutLen == 0) {
        USBAudio_GenerateSquareWave();
    }

    uint32_t       packetLen = g_audioOutLen ? g_audioOutLen : AUDIO_EP_MAX_PACKET_SIZE;
    const uint8_t *src       = g_audioOutLen ? g_audioOutPacket : g_audioInPacket;

    if (USBEndpointDataPut(USB0_BASE, AUDIO_EP_IN, (uint8_t *)src, packetLen) != 0)
        return;

    USBEndpointDataSend(USB0_BASE, AUDIO_EP_IN, USB_TRANS_IN);
    g_audioOutLen = 0;
}

/*
 * USBAudio_ProcessOut
 *
 * FIX 3 + FIX 5: Only read the OUT endpoint when the playback interface
 * (Interface 1) has alt-setting == 1.  Also removed duplicate call path
 * (see app_run notes below).
 */
static void USBAudio_ProcessOut(void) {
    /* FIX 5: respect the USB Audio Class lifecycle */
    if (g_interfaceAltSetting[IFACE_PLAYBACK] != 1)
        return;

    uint32_t count = USBEndpointDataAvail(USB0_BASE, AUDIO_EP_OUT);
    if (count == 0)
        return;

    uint32_t len = count;
    if (len > AUDIO_EP_MAX_PACKET_SIZE)
        len = AUDIO_EP_MAX_PACKET_SIZE;

    if (USBEndpointDataGet(USB0_BASE, AUDIO_EP_OUT, g_audioOutPacket, &len) == 0)
        USBAudio_HandleOutPacket(g_audioOutPacket, len);

    USBDevEndpointDataAck(USB0_BASE, AUDIO_EP_OUT, false);
}

static void ProcessUSBAudio(void) {
    USBAudio_ProcessOut();
    USBAudio_SendCapture();
}

/*----- USB Device interrupt handler ----------------------------------*/

void USB0DeviceIntHandler(void) {
    uint16_t csrl0 = HWREGH(USB0_BASE + USB_0_CSRL0);
    static uint16_t last_csrl0 = 0;

    if (csrl0 & 0x10) {                          /* SETUPEND */
        HWREGB(USB0_BASE + USB_0_CSRL0) |= 0x80; /* Clear SETUPEND */
    }

    uint32_t statusCtrl = USBIntStatusControl(USB0_BASE);
    if (statusCtrl & USB_INTCTRL_RESET) {
        pendingAddress    = 0;
        pendingSetAddress = 0;
        isConfigured      = 0;
        g_interfaceAltSetting[0] = 0;
        g_interfaceAltSetting[1] = 0;
        g_interfaceAltSetting[2] = 0;
        USBDevAddrSet(USB0_BASE, 0);
    }

    /* EP0 handling */
    if (csrl0 & 0x01) { /* RXRDY */
        USB_SetupPacket setup;
        unsigned int sz;
        USBEndpointDataGet(USB0_BASE, USB_EP_0, (uint8_t *)&setup, &sz);

        if (sz == 8) {
            USBDevEndpointDataAck(USB0_BASE, USB_EP_0, false);

            if ((setup.bmRequestType & 0x60) == 0) { /* Standard request */
                switch (setup.bRequest) {

                case USB_REQ_GET_DESCRIPTOR: {
                    uint8_t        type = setup.wValue >> 8;
                    uint8_t        idx  = setup.wValue & 0xFF;
                    const uint8_t *desc = 0;
                    uint16_t       len  = 0;

                    if (type == USB_DESC_DEVICE) {
                        desc = deviceDescriptor;
                        len  = sizeof(deviceDescriptor);
                    } else if (type == USB_DESC_CONFIGURATION) {
                        desc = configDescriptor;
                        len  = sizeof(configDescriptor);
                    } else if (type == USB_DESC_DEVICE_QUAL) {
                        desc = devQualDescriptor;
                        len  = sizeof(devQualDescriptor);
                    } else if (type == USB_DESC_STRING && idx < 4) {
                        desc = strings[idx];
                        len  = stringLens[idx];
                    }

                    if (desc) {
                        if (len > setup.wLength)
                            len = setup.wLength;
                        g_pEP0Data = desc;
                        g_uEP0Len  = len;
                        EP0SendData();
                    } else {
                        USBDevEndpointStall(USB0_BASE, USB_EP_0, USB_EP_DEV_IN);
                    }
                    break;
                }

                case USB_REQ_SET_ADDRESS:
                    pendingAddress    = setup.wValue;
                    pendingSetAddress = 1;
                    USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true);
                    break;

                /*
                 * FIX 1: GET_INTERFACE previously stored the address of a local
                 * variable 'alt' in g_pEP0Data.  By the time EP0SendData (or the
                 * TX-complete path) used that pointer, the stack frame was gone →
                 * undefined behaviour / garbage byte sent to host.
                 *
                 * Fix: use a static buffer so the pointer remains valid after the
                 * case block exits.
                 */
                case USB_REQ_GET_INTERFACE: {
                    static uint8_t altResponse;          /* FIX 1: static */
                    uint8_t iface = setup.wIndex & 0xFF;
                    altResponse = (iface < sizeof(g_interfaceAltSetting))
                                  ? g_interfaceAltSetting[iface]
                                  : 0;
                    g_pEP0Data = &altResponse;
                    g_uEP0Len  = 1;
                    EP0SendData();
                    break;
                }

                /*
                 * FIX 4: SET_INTERFACE now activates or deactivates the relevant
                 * isochronous endpoint when the host switches between alt=0
                 * (zero-bandwidth) and alt=1 (operational).
                 *
                 * This is what allows aplay to take ownership of the playback
                 * interface without seeing it as permanently busy.
                 */
                case USB_REQ_SET_INTERFACE: {
                    uint8_t iface = setup.wIndex & 0xFF;
                    uint8_t alt   = setup.wValue & 0xFF;

                    if (iface >= sizeof(g_interfaceAltSetting)) {
                        USBDevEndpointStall(USB0_BASE, USB_EP_0, USB_EP_DEV_IN);
                        break;
                    }

                    uint8_t prevAlt = g_interfaceAltSetting[iface];
                    g_interfaceAltSetting[iface] = alt;

                    if (iface == IFACE_PLAYBACK) {
                        if (alt == 1 && prevAlt != 1)
                            USBActivatePlayback();
                        else if (alt == 0 && prevAlt != 0)
                            USBDeactivatePlayback();
                    } else if (iface == IFACE_CAPTURE) {
                        if (alt == 1 && prevAlt != 1)
                            USBActivateCapture();
                        else if (alt == 0 && prevAlt != 0)
                            USBDeactivateCapture();
                    }

                    USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true);
                    break;
                }

                case USB_REQ_SET_CONFIGURATION:
                    /* Reset alt-settings to zero-bandwidth on new configuration */
                    g_interfaceAltSetting[0] = 0;
                    g_interfaceAltSetting[1] = 0;
                    g_interfaceAltSetting[2] = 0;
                    USBSetDeviceType();
                    break;

                default:
                    USBDevEndpointStall(USB0_BASE, USB_EP_0, USB_EP_DEV_IN);
                    break;
                }

            } else if ((setup.bmRequestType & 0x60) == 0x20) { /* Class request */
                if (setup.wLength > 0 && (setup.bmRequestType & 0x80)) {
                    static uint8_t zeroBuffer[64] = {0};
                    uint16_t len = setup.wLength;
                    if (len > sizeof(zeroBuffer))
                        len = sizeof(zeroBuffer);
                    g_pEP0Data = zeroBuffer;
                    g_uEP0Len  = len;
                    EP0SendData();
                } else {
                    USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true);
                }
            }

        } else if (sz == 0) {
            /* Status phase ZLP from host */
            USBDevEndpointDataAck(USB0_BASE, USB_EP_0, false);
        } else {
            /* Unexpected data size */
            USBDevEndpointDataAck(USB0_BASE, USB_EP_0, false);
        }

    } else if (((last_csrl0 & 0x02) && !(csrl0 & 0x02)) ||
               ((last_csrl0 & 0x08) && !(csrl0 & 0x08))) {
        /* TX complete (TXRDY cleared) or status phase complete (DATAEND cleared) */
        if (g_uEP0Len > 0) {
            EP0SendData();
        } else if (pendingSetAddress) {
            USBDevAddrSet(USB0_BASE, pendingAddress);
            pendingSetAddress = 0;
        }
    }

    last_csrl0 = csrl0;

    tripleAck();

}

/*----- Application entry points --------------------------------------*/

t_status app_init(void) {
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_USB0, 0, PSC_MDCTL_NEXT_ENABLE);
    UsbPhyOn();

    /* Configure CFGCHIP2 for 24 MHz crystal and device mode */
    uint32_t cfgchip2 = HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2);
    cfgchip2 &= ~(0x0000000F | (3 << 13) | (1 << 12)); /* Clear REFFREQ/OTGMODE/CLKMUX */
    cfgchip2 |=  (2 << 0) | (2 << 13) | (1 << 6);     /* REFFREQ=24MHz, OTGMODE=Device, PHY_PLLON */
    HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) = cfgchip2;

    /* Wait for PHY clock */
    int timeout = 1000000;
    while (!(HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) & (1 << 17)) && timeout--)
        ;

    IntRegister(SYS_INT_USB0, USB0DeviceIntHandler);
    IntChannelSet(SYS_INT_USB0, 2);
    IntSystemEnable(SYS_INT_USB0);

    USBIntEnableControl(USB0_BASE, USB_INTCTRL_RESET | USB_INTCTRL_DISCONNECT);
    USBIntEnableEndpoint(USB0_BASE, USB_INTEP_ALL);

    USBDevConnect(USB0_BASE);

// Definiciones de bits para el Wrapper (basadas en el mapeo del AM1808)
#define USB_INTEP_DEV_IN_3  0x00000008  // Bit para EP3 TX (Audio IN / Mic)
#define USB_INTEP_DEV_OUT_3 0x00080000  // Bit para EP3 RX (Audio OUT / Speaker)

// Actualización en app_init()
#define USB_WRAPPER_CTRL_MASK 0x01FF0000 // Bits de control (Reset, Resume, etc.)

// Combinamos:
// EP0 (Control) + EP1/2 (CDC/Comandos) + EP3 (Audio Streaming)
uint32_t uiIntMask = USB_WRAPPER_CTRL_MASK | 
                     USB_INTEP_0 |              // EP0 Control
                     USB_INTEP_DEV_IN_1 |       // EP1 TX (CDC)
                     USB_INTEP_DEV_OUT_2 |      // EP2 RX (CDC)
                     USB_INTEP_DEV_OUT_3 |      // EP3 RX (Audio Out - Streaming)
                     USB_INTEP_DEV_IN_3;        // EP3 IN  (Audio In / Feedback)

HWREG(USB_0_OTGBASE + USB_0_INTR_MASK_SET) = uiIntMask;
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
    }

    /*
     * FIX 3: Previously app_run called BOTH USB0DeviceIntHandler() AND
     * ProcessUSBAudio(), while USB0DeviceIntHandler itself also called
     * USBAudio_ProcessOut() on RXRDY.  This meant ProcessOut ran twice per
     * loop, potentially consuming a packet before SendCapture could use it.
     *
     * Now:
     *   - USB0DeviceIntHandler handles only control/status (EP0 + resets).
     *   - ProcessUSBAudio is the single place audio IN/OUT is serviced.
     */
    //USB0DeviceIntHandler();
    ProcessUSBAudio();

    if (per_gpio_get_indexed(GPIO_POWER_BUTTON) == 0) {
        ft_shutdown();
    }
}