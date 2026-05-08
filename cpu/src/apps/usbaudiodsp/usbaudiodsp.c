/*----------------------------------------------------------------------

                     This file is part of Freetribe

                https://github.com/bangcorrupt/freetribe

----------------------------------------------------------------------*/

/**
 * @file    usbaudiointerrupt.c
 *
 * @brief   Bare-metal USB Audio (UAC1) application for Freetribe / AM1808.
 *
 * Architecture notes:
 *
 *   Interrupt routing (AM1808 USB Wrapper → AINTC):
 *     The AM1808 wraps the Mentor MUSBMHDRC USB core with a TI-specific
 *     interrupt aggregation layer.  All interrupt sources (endpoint TX/RX
 *     and generic INTRUSB events) are reflected in the wrapper's INTR_SRC
 *     register (OTG_BASE + 0x20).  The bit mapping is:
 *
 *       Bits [4:0]   – TX EP0-EP4
 *       Bits [20:17] – RX EP1-EP4
 *       Bits [23:16] – INTRUSB mirror (Suspend/Resume/Reset/SOF/Connect/…)
 *         Bit 16 = Suspend, 17 = Resume, 18 = Reset,
 *         Bit 19 = SOF,     20 = Connect, 21 = Disconnect
 *       Bits [31:24] – Reserved / not writable on this silicon
 *
 *   IMPORTANT – Mentor core INTRUSB at offset 0x0A (USB_0_IS) is
 *   unreliable on this AM1808 silicon.  Reads return 0 most of the time
 *   even when events are pending.  The wrapper INTR_SRC at bits [23:16]
 *   reliably mirrors the same information and is used exclusively.
 *
 *   SOF-driven isochronous audio:
 *     The host sends a Start-of-Frame token every 1 ms (full-speed).
 *     This appears as INTR_SRC bit 19 (WRAPPER_SOF_BIT).  On each SOF
 *     the ISR calls USBAudio_SendCapture() to push one audio packet to
 *     the isochronous IN endpoint, keeping the audio stream synchronised
 *     with the USB frame clock.
 *
 *   ISR safety:
 *     No blocking I/O (ft_printf, SPI, UART) is performed inside the
 *     ISR.  Diagnostic counters (g_isrCount, g_sofCount) are updated
 *     atomically and printed only from the non-interrupt app_run() loop.
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

#include "usbaudiodescriptors.h"

#include "dev_dsp_ipc.h"

/*----- Defines -------------------------------------------------------*/

#define USB_0_OTGBASE SOC_USB_0_OTG_BASE
#define USB_0_INTR_MASK_SET 0x30
#define USB_0_INTR_SRC_CLEAR 0x28
#define USB_0_END_OF_INTR 0x3c

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
#define USB_REQ_GET_INTERFACE 0x0A
#define USB_REQ_SET_INTERFACE 0x0B

#define USB_DESC_DEVICE 0x01
#define USB_DESC_CONFIGURATION 0x02
#define USB_DESC_STRING 0x03
#define USB_DESC_DEVICE_QUAL 0x06

#define AUDIO_EP_IN USB_EP_1
#define AUDIO_EP_OUT USB_EP_2
#define AUDIO_EP_MAX_PACKET_SIZE 192

/* Interface indices in g_interfaceAltSetting[] */
#define IFACE_AUDIO_CONTROL 0
#define IFACE_PLAYBACK 1 /* Interface 1: host → device */
#define IFACE_CAPTURE 2  /* Interface 2: device → host */

void USB0DeviceIntHandler(void) ;
void USBAudio_SetFrequency(uint32_t frequency);

#define IPC_BUFFER_SIZE_IN_BYTES (192 * 2)
uint8_t ipc_rx_buffer[2][IPC_BUFFER_SIZE_IN_BYTES] __attribute__((aligned(4)));
volatile uint8_t ipc_read_idx = 0;
volatile uint8_t ipc_write_idx = 0;
volatile bool ipc_data_ready = false;
volatile bool ipc_transfer_in_progress = false;

uint32_t g_dsp_buffer_index = 0;

/*----- Types ---------------------------------------------------------*/

typedef struct __attribute__((packed)) {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} USB_SetupPacket;


void ipc_callback(void *ctx, t_ipc_status status);
void get_dsp_data(void);



volatile int32_t g_last_dsp_sample = 0;
volatile uint32_t g_sof_count = 0;
volatile int g_usb_err_count = 0;
volatile int g_ipc_err_count = 0;

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

/*----- State ---------------------------------------------------------*/

static const uint8_t *g_pEP0Data = 0;
static uint32_t g_uEP0Len = 0;

static uint8_t isConfigured = 0;
static uint8_t g_audioOutPacket[AUDIO_EP_MAX_PACKET_SIZE];
static uint32_t g_audioOutLen = 0;
static uint8_t g_audioInPacket[AUDIO_EP_MAX_PACKET_SIZE] __attribute__((aligned(4)));
static uint32_t g_squarePhase = 0;

/* Wavetable / NCO State */
#define WAVETABLE_SIZE 1024
static int16_t g_wavetable[WAVETABLE_SIZE];
static uint32_t g_phaseIncrement = 0;

static uint16_t pendingAddress = 0;
static uint8_t pendingSetAddress = 0;

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
        g_uEP0Len -= sendLen;
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
    USBDevEndpointConfigSet(USB0_BASE, AUDIO_EP_IN, AUDIO_EP_MAX_PACKET_SIZE, USB_EP_MODE_ISOC | USB_EP_DEV_IN | USB_EP_AUTO_SET);
    USBDevEndpointConfigSet(USB0_BASE, AUDIO_EP_OUT, AUDIO_EP_MAX_PACKET_SIZE, USB_EP_MODE_ISOC | USB_EP_DEV_OUT | USB_EP_AUTO_REQUEST | USB_EP_AUTO_CLEAR);

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
    USBDevEndpointConfigSet(USB0_BASE, AUDIO_EP_OUT, AUDIO_EP_MAX_PACKET_SIZE, USB_EP_MODE_ISOC | USB_EP_DEV_OUT | USB_EP_AUTO_REQUEST | USB_EP_AUTO_CLEAR);
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

    /* Sincronización CPU-DSP: 
     * Reseteamos el puntero de escritura del DSP (en la dir 0x5C) a 0.
     * Ponemos el puntero de lectura del CPU a 500 (la mitad del buffer).
     * Esto garantiza que los punteros estén separados por 0.5 segundos 
     * y no se crucen causando saltos de fase / glitches.
     */
    static uint32_t zero = 0;
    dev_dsp_ipc_transfer(0x0000005C, &zero, 1, NULL, NULL);
    g_dsp_buffer_index = 500;
}

static void USBDeactivateCapture(void) {
    /* Nothing hardware-level needed for ISO endpoints.
     * The guard in USBAudio_SendCapture() stops transmission. */
    g_audioOutLen = 0;
}

/*----- Audio helpers ------------------------------*/
/**
 * Fills the audio buffer from the pre-calculated wavetable.
 * Uses a 32-bit NCO for high precision and returns the next phase.
 */
static uint32_t USBAudio_FillFromLUT(uint32_t phase) {
    for (uint32_t i = 0; i < 48; i++) {
        /* Index is the top 10 bits of our 32-bit phase (32 - log2(1024) = 22) */
        uint16_t idx = (uint16_t)(phase >> 22);
        int16_t sample = g_wavetable[idx];

        g_audioInPacket[i * 4 + 0] = (uint8_t)(sample & 0xFF);
        g_audioInPacket[i * 4 + 1] = (uint8_t)((sample >> 8) & 0xFF);
        g_audioInPacket[i * 4 + 2] = g_audioInPacket[i * 4 + 0];
        g_audioInPacket[i * 4 + 3] = g_audioInPacket[i * 4 + 1];

        phase += g_phaseIncrement;
    }
    return phase;
}

/**
 * Updates the oscillator frequency in real-time.
 */
void USBAudio_SetFrequency(uint32_t frequency) {
    /* increment = (f / fs) * 2^32 */
    g_phaseIncrement = (uint32_t)(((unsigned long long)frequency << 32) / 48000);
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

    if (!isConfigured || g_interfaceAltSetting[IFACE_CAPTURE] != 1)
        return;

    /* 1. If we have fresh data, convert it from 32-bit DSP format to 16-bit USB format */
    if (ipc_data_ready) {
        // Leemos el índice actual y limpiamos el flag ANTES de convertir para evitar race conditions
        uint8_t local_idx = ipc_read_idx;
        ipc_data_ready = false; 

        int32_t *src = (int32_t *)ipc_rx_buffer[local_idx];
        int16_t *dst = (int16_t *)g_audioInPacket;
        for (int i = 0; i < 48; i++) {
            dst[i*2 + 0] = (int16_t)(src[i*2 + 0] >> 16); 
            dst[i*2 + 1] = (int16_t)(src[i*2 + 1] >> 16);
        }
    }

    /* 2. Put the 16-bit converted data into hardware FIFO */
    if (USBEndpointDataPut(USB0_BASE, AUDIO_EP_IN, g_audioInPacket, AUDIO_EP_MAX_PACKET_SIZE) == 0) {
        USBEndpointDataSend(USB0_BASE, AUDIO_EP_IN, USB_TRANS_IN);
    } else {
        g_usb_err_count++;
    }

    get_dsp_data();
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

    if (USBEndpointDataGet(USB0_BASE, AUDIO_EP_OUT, g_audioOutPacket, &len) == 0) {
        USBAudio_HandleOutPacket(g_audioOutPacket, len);
    }

    USBDevEndpointDataAck(USB0_BASE, AUDIO_EP_OUT, false);
}

/*----- USB Device interrupt handler ----------------------------------*/

/*
 * Wrapper INTR_SRC maps INTRUSB bits to bits [23:16]:
 *   Bit 16 = Suspend,  Bit 17 = Resume,  Bit 18 = Reset,
 *   Bit 19 = SOF,      Bit 20 = Connect, Bit 21 = Disconnect
 *
 * The Mentor core INTRUSB register at 0x0A is unreliable on this silicon
 * (only returns data sporadically). Use wrapper INTR_SRC instead.
 */
#define WRAPPER_RESET_BIT  0x00040000  /* INTR_SRC bit 18 */
#define WRAPPER_SOF_BIT    0x00080000  /* INTR_SRC bit 19 */

void EP0IntHandler(uint32_t wrapperSrc);

void EP0IntHandler(uint32_t wrapperSrc) {
    uint16_t csrl0 = HWREGH(USB0_BASE + USB_0_CSRL0);
    static uint16_t last_csrl0 = 0;

    if (csrl0 & 0x10) {                          /* SETUPEND */
        HWREGB(USB0_BASE + USB_0_CSRL0) |= 0x80; /* Clear SETUPEND */
    }

    /* Read endpoint status to clear pending EP interrupt bits */
    (void)USBIntStatusEndpoint(USB0_BASE);

    if (wrapperSrc & WRAPPER_RESET_BIT) {
        pendingAddress = 0;
        pendingSetAddress = 0;
        isConfigured = 0;
        g_interfaceAltSetting[0] = 0;
        g_interfaceAltSetting[1] = 0;
        g_interfaceAltSetting[2] = 0;
        USBDevAddrSet(USB0_BASE, 0);
    }

    /* SOF - Start of Frame (wrapper bit 19) */
    if (wrapperSrc & WRAPPER_SOF_BIT) {
        /* Send isochronous audio each 1ms frame */
        g_sof_count++;
        USBAudio_SendCapture();
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
                    static uint8_t altResponse; /* FIX 1: static */
                    uint8_t iface = setup.wIndex & 0xFF;
                    altResponse = (iface < sizeof(g_interfaceAltSetting)) ? g_interfaceAltSetting[iface] : 0;
                    g_pEP0Data = &altResponse;
                    g_uEP0Len = 1;
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
                    uint8_t alt = setup.wValue & 0xFF;

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
                    g_uEP0Len = len;
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

    } else if (((last_csrl0 & 0x02) && !(csrl0 & 0x02)) || ((last_csrl0 & 0x08) && !(csrl0 & 0x08))) {
        /* TX complete (TXRDY cleared) or status phase complete (DATAEND cleared) */
        if (g_uEP0Len > 0) {
            EP0SendData();
        } else if (pendingSetAddress) {
            USBDevAddrSet(USB0_BASE, pendingAddress);
            pendingSetAddress = 0;
        }
    }

    last_csrl0 = csrl0;

    
}

/*----- Application entry points --------------------------------------*/

void fillLutTableSquare(){
    /* Initialize Wavetable with a square wave */
    int i;
    for (i = 0; i < WAVETABLE_SIZE; i++) {
        g_wavetable[i] = (i < (WAVETABLE_SIZE / 2)) ? 0x4000 : -0x4000;
    }

}

void fillLutTableSaw(){
    /* Initialize Wavetable with a descending sawtooth wave */
    int i;
    for (i = 0; i < WAVETABLE_SIZE; i++) {
        // La fórmula interpola de 0x4000 (i=0) a -0x4000 (i=WAVETABLE_SIZE-1)
        g_wavetable[i] = 0x4000 - (0x8000 * i / WAVETABLE_SIZE);
    }
}
t_status app_init(void) {
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_USB0, 0, PSC_MDCTL_NEXT_ENABLE);
    UsbPhyOn();

    /* Configure CFGCHIP2 for 24 MHz crystal and device mode */
    uint32_t cfgchip2 = HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2);
    cfgchip2 &= ~(0x0000000F | (3 << 13) | (1 << 12)); /* Clear REFFREQ/OTGMODE/CLKMUX */
    cfgchip2 |= (2 << 0) | (2 << 13) | (1 << 6);       /* REFFREQ=24MHz, OTGMODE=Device, PHY_PLLON */
    HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) = cfgchip2;

    /* Wait for PHY clock */
    int timeout = 1000000;
    while (!(HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) & (1 << 17)) && timeout--)
        ;

    IntRegister(SYS_INT_USB0, USB0DeviceIntHandler);
    IntChannelSet(SYS_INT_USB0, 2);
    IntSystemEnable(SYS_INT_USB0);

    /*
     * Enable Mentor core interrupts (writes to USB_0_IE at offset 0x0B).
     * Even though we read events from the wrapper, the core IE register
     * controls whether the core SETS the corresponding bits that the
     * wrapper then mirrors.  Without enabling SOF here, bit 19 of
     * INTR_SRC would never assert.
     */
    USBIntEnableControl(USB0_BASE, USB_INTCTRL_RESET | USB_INTCTRL_DISCONNECT | USB_INTCTRL_SOF);
    USBIntEnableEndpoint(USB0_BASE, USB_INTEP_ALL);

    //fillLutTable();
    fillLutTableSaw();
    
    /* Pre-calculate phase increment for 480 Hz @ 48 kHz */
    USBAudio_SetFrequency(480/4);

    USBDevConnect(USB0_BASE);

    /*
     * Configure the TI wrapper interrupt mask.
     *
     * Bits [4:0]   = TX EP0-EP4 (0x1F)  – all TX endpoints
     * Bits [20:17] = RX EP1-EP4         – all RX endpoints
     * Bits [23:16] = INTRUSB mirror     – SOF, Reset, Suspend, etc.
     *
     * Note: bit 25 (documented as "generic interrupt" aggregate) is NOT
     * writable on this silicon variant.  Instead, individual INTRUSB
     * events appear at bits [23:16] and are enabled via 0x00FF0000.
     * The mask 0x01FF01FF covers all implemented EP + INTRUSB bits.
     */
    uint32_t uiIntMask = 0x01FF01FF;  /* All EP TX/RX + INTRUSB events */
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

    if (per_gpio_get_indexed(GPIO_POWER_BUTTON) == 0) {
        ft_shutdown();
    }

    static int debug_timer = 0;
    debug_timer++;
    if (debug_timer >= 100000) {
        debug_timer = 0;
        //ft_printf("D:0x%08x I:%d R:%d S:%d U:%d P:%d\n",             (unsigned int)g_last_dsp_sample, (int)g_dsp_buffer_index, (int)ipc_data_ready,             (unsigned int)g_sof_count, g_usb_err_count, g_ipc_err_count);
    }
}

/**
 * @brief  Top-level USB interrupt handler (registered with AINTC).
 *
 * Flow:
 *   1. Read wrapper INTR_SRC (the ONLY reliable source of interrupt status
 *      on this AM1808 silicon — Mentor core INTRUSB at 0x0A is broken).
 *   2. Discard-read the Mentor core INTRUSB register to clear any latched
 *      core-level bits and prevent them from re-triggering.
 *   3. Dispatch to EP0IntHandler for control/SOF and USBAudio_ProcessOut
 *      for isochronous playback.
 *   4. tripleAck: clear AINTC status, write wrapper INTR_SRC_CLEAR, and
 *      write EOI to release the interrupt line.
 */
void USB0DeviceIntHandler(void) {

    /* Read wrapper INTR_SRC — reliable source for all interrupt events */
    uint32_t wrapperSrc = HWREG(USB_0_OTGBASE + USB_0_INTR_SRC);

    /* Read & discard Mentor INTRUSB to clear core-level bits */
    (void)HWREGB(USB0_BASE + USB_0_IS);


    EP0IntHandler(wrapperSrc);
    USBAudio_ProcessOut();
    tripleAck();
}


// usar double buffer aca para no bloquear la llamada 
void get_dsp_data(){
    const uint32_t dsp_ring_buffer_address = 0x00000060;

    // Si ya hay una transferencia pendiente o datos listos, no empezamos otra
    if (ipc_data_ready || ipc_transfer_in_progress) return; 

    ipc_transfer_in_progress = true;

    // Calculamos el offset en bytes
    uint32_t dsp_address = dsp_ring_buffer_address + (g_dsp_buffer_index * IPC_BUFFER_SIZE_IN_BYTES);

    t_ipc_status status = dev_dsp_ipc_read(
        dsp_address, 
        (uint32_t *)ipc_rx_buffer[ipc_write_idx], 
        IPC_BUFFER_SIZE_IN_BYTES / 4, 
        ipc_callback, 
        (void *)0x23AC1D23 
    );

    if (status != IPC_SUCCESS) {
        ipc_transfer_in_progress = false;
        g_ipc_err_count++;
    }
}

// llamado cuando termina la transferencia de datos del ipc
void ipc_callback(void *ctx, t_ipc_status status) {
    ipc_transfer_in_progress = false;

    if (status == IPC_SUCCESS) {
        // Indicamos que el buffer actual está listo para ser leído
        ipc_read_idx = ipc_write_idx;
        ipc_data_ready = true;
        
        // Avanzamos el índice del DSP (circular de 1000 bloques de 96 muestras)
        g_dsp_buffer_index ++;
        if (g_dsp_buffer_index >= 1000) {
            g_dsp_buffer_index = 0;
        }

        // Alternamos el buffer de escritura para la próxima petición
        ipc_write_idx = (ipc_write_idx + 1) % 2;
    } else {
        g_ipc_err_count++;
    }
}
