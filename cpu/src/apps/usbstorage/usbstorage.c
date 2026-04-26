/*----------------------------------------------------------------------
                     Freetribe - USB MSC Fixed (Template from MIDI)
----------------------------------------------------------------------*/

#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include "freetribe.h"
#include "hw_types.h"
#include "hw_usb.h"
#include "csl_interrupt.h"
#include "csl_usb.h"
#include "csl_psc.h"
#include "hw_psc_AM1808.h"
#include "hw_usbphyGS60.h"
#include "hw_syscfg0_AM1808.h"

/* Definiciones de Hardware */
#ifndef USB0_BASE
#define USB0_BASE             SOC_USB_0_BASE
#endif
#define USB_0_OTGBASE         SOC_USB_0_OTG_BASE
#define USB_0_INTR_MASK_SET   0x30
#define USB_0_END_OF_INTR     0x3C

/* Descriptores MSC (BCD 2.00 para compatibilidad xHCI) */
static const uint8_t deviceDescriptor[] = {
    18, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 64,
    0x1C, 0x1C, 0x20, 0x00, 0x00, 0x02, 1, 2, 3, 1
};

static const uint8_t configDescriptor[] = {
    9, 2, 32, 0, 1, 1, 0, 0xC0, 50,
    9, 4, 0, 0, 2, 0x08, 0x06, 0x50, 0,
    7, 5, 0x81, 0x02, 64, 0, 0,
    7, 5, 0x02, 0x02, 64, 0, 0
};

static const uint8_t devQualDescriptor[] = {
    10, 0x06, 0x00, 0x02, 0x00, 0x00, 0x00, 64, 1, 0
};

/* Strings */
static const uint8_t string0[] = { 4, 3, 0x09, 0x04 };
static const uint8_t string1[] = { 20, 3, 'F',0,'r',0,'e',0,'e',0,'t',0,'r',0,'i',0,'b',0,'e',0 };
static const uint8_t string2[] = { 22, 3, 'F',0,'l',0,'a',0,'s',0,'h',0,' ',0,'D',0,'r',0,'i',0,'v',0,'e',0 };
static const uint8_t string3[] = { 10, 3, '1',0,'2',0,'3',0,'4',0 };
static const uint8_t *const strings[] = { string0, string1, string2, string3 };
static const uint8_t stringLens[] = { sizeof(string0), sizeof(string1), sizeof(string2), sizeof(string3) };

/* Variables de Estado */
static const uint8_t *g_pEP0Data = 0;
static uint32_t g_uEP0Len = 0;
static uint16_t pendingAddress = 0;
static uint8_t pendingSetAddress = 0;
static uint8_t isConfigured = 0;

/*====================================================================
 * EP0 Helpers (Usando lógica de usbmidi.c)
 *==================================================================*/

static void EP0SendData(void) {
    uint32_t sendLen = (g_uEP0Len > 64) ? 64 : g_uEP0Len;
    if (sendLen > 0) {
        USBEndpointDataPut(USB0_BASE, USB_EP_0, (uint8_t *)g_pEP0Data, sendLen);
        g_pEP0Data += sendLen;
        g_uEP0Len -= sendLen;
    }
    
    // Si no quedan datos, cerrar con LAST para activar DATAEND
    if (g_uEP0Len == 0) {
        USBEndpointDataSend(USB0_BASE, USB_EP_0, USB_TRANS_IN_LAST);
    } else {
        USBEndpointDataSend(USB0_BASE, USB_EP_0, USB_TRANS_IN);
    }
}

/*====================================================================
 * Handler de Interrupción (Polling)
 *==================================================================*/

void USB0DeviceIntHandler(void) {
    uint16_t csrl0 = HWREGH(USB0_BASE + USB_0_CSRL0);
    static uint16_t last_csrl0 = 0;

    // 1. Limpiar SETUPEND si el host abortó una transferencia previa
    if (csrl0 & 0x10) { 
        HWREGH(USB0_BASE + USB_0_CSRL0) = 0x80; // SVCSETUPEND
    }

    // 2. Manejo de Reset
    uint32_t statusCtrl = USBIntStatusControl(USB0_BASE);
    if (statusCtrl & USB_INTCTRL_RESET) {
        pendingAddress = 0;
        pendingSetAddress = 0;
        isConfigured = 0;
        USBDevAddrSet(USB0_BASE, 0);
    }

    USBIntStatusEndpoint(USB0_BASE);

    // 3. Recepción de SETUP o Datos en EP0
    if (csrl0 & 0x01) { // RXRDY
        typedef struct { uint8_t bmReq; uint8_t bReq; uint16_t wVal; uint16_t wIdx; uint16_t wLen; } SetupPkt;
        SetupPkt setup;
        unsigned int sz;
        
        USBEndpointDataGet(USB0_BASE, USB_EP_0, (uint8_t *)&setup, &sz);

        if (sz == 8) {
            // ACK de recepción (SVCRXPKTRDY)
            USBDevEndpointDataAck(USB0_BASE, USB_EP_0, false);

            if ((setup.bmReq & 0x60) == 0) { // Standard Request
                switch (setup.bReq) {
                    case 0x06: { // GET_DESCRIPTOR
                        uint8_t type = setup.wVal >> 8;
                        uint8_t idx = setup.wVal & 0xFF;
                        const uint8_t *desc = 0;
                        uint16_t len = 0;

                        if (type == 1)      { desc = deviceDescriptor; len = sizeof(deviceDescriptor); }
                        else if (type == 2) { desc = configDescriptor; len = sizeof(configDescriptor); }
                        else if (type == 6) { desc = devQualDescriptor; len = sizeof(devQualDescriptor); }
                        else if (type == 3 && idx < 4) { desc = strings[idx]; len = stringLens[idx]; }

                        if (desc) {
                            if (len > setup.wLen) len = setup.wLen;
                            g_pEP0Data = desc;
                            g_uEP0Len = len;
                            EP0SendData();
                        } else {
                            USBDevEndpointStall(USB0_BASE, USB_EP_0, USB_EP_DEV_IN);
                        }
                        break;
                    }
                    case 0x05: // SET_ADDRESS
                        pendingAddress = setup.wVal;
                        pendingSetAddress = 1;
                        USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true); // Status ZLP
                        break;
                    case 0x09: // SET_CONFIGURATION
                        USBDevEndpointConfigSet(USB0_BASE, USB_EP_1, 64, USB_EP_MODE_BULK | USB_EP_DEV_IN);
                        USBDevEndpointConfigSet(USB0_BASE, USB_EP_2, 64, USB_EP_MODE_BULK | USB_EP_DEV_OUT);
                        USBIntEnableEndpoint(USB0_BASE, (1 << 18)); // EP2 RX
                        isConfigured = 1;
                        USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true);
                        break;
                    default:
                        USBDevEndpointStall(USB0_BASE, USB_EP_0, USB_EP_DEV_IN);
                        break;
                }
            } else if ((setup.bmReq & 0x60) == 0x20) { // Class Request (MSC)
                if (setup.bReq == 0xFE) { // Get Max LUN
                    static const uint8_t maxLun = 0;
                    g_pEP0Data = &maxLun;
                    g_uEP0Len = 1;
                    EP0SendData();
                } else if (setup.bReq == 0xFF) { // Bulk Only Reset
                    USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true);
                }
            }
        } else {
            USBDevEndpointDataAck(USB0_BASE, USB_EP_0, false);
        }
    } 
    // 4. Fase de continuación de Datos o finalización de Dirección
    else if (((last_csrl0 & 0x02) && !(csrl0 & 0x02)) || ((last_csrl0 & 0x08) && !(csrl0 & 0x08))) {
        if (g_uEP0Len > 0) {
            EP0SendData();
        } else if (pendingSetAddress) {
            USBDevAddrSet(USB0_BASE, pendingAddress);
            pendingSetAddress = 0;
        }
    }

    last_csrl0 = csrl0;
    IntSystemStatusClear(SYS_INT_USB0);
    HWREG(USB_0_OTGBASE + USB_0_END_OF_INTR) = 0;
}

/*====================================================================
 * Inicialización y Ejecución
 *==================================================================*/

t_status app_init(void) {
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_USB0, 0, PSC_MDCTL_NEXT_ENABLE);
    UsbPhyOn();

    uint32_t cfgchip2 = HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2);
    cfgchip2 &= ~(0x0000000F | (3 << 13) | (1 << 12));
    cfgchip2 |= (2 << 0) | (2 << 13) | (1 << 6); // 24MHz, Device mode, PHY On
    HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) = cfgchip2;

    int timeout = 1000000;
    while (!(HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) & (1 << 17)) && timeout--);

    HWREGB(USB0_BASE + USB_0_POWER) &= ~0x20; // Force Full Speed

    IntRegister(SYS_INT_USB0, USB0DeviceIntHandler);
    IntChannelSet(SYS_INT_USB0, 2);
    // Usamos IntSystemDisable para asegurar polling puro como en tu usbstorage.c original
    IntSystemDisable(SYS_INT_USB0); 

    USBIntEnableControl(USB0_BASE, USB_INTCTRL_RESET | USB_INTCTRL_DISCONNECT);
    USBIntEnableEndpoint(USB0_BASE, USB_INTEP_ALL);
    USBDevConnect(USB0_BASE);

    HWREG(USB_0_OTGBASE + USB_0_INTR_MASK_SET) = 0x1FF;

    return SUCCESS;
}

void app_run(void) {
    static int heartbeat = 0;
    if (++heartbeat >= 100000) {
        heartbeat = 0;
        static int ledState = 0;
        ledState = !ledState;
        ft_set_led(LED_PLAY, ledState ? 255 : 0);
    }

    // Polling del handler
    USB0DeviceIntHandler();

    // Aquí iría la lógica de BOT_Task() si el dispositivo está configurado
    // if (isConfigured) BOT_Task();

    if (per_gpio_get_indexed(128) == 0) {
        ft_shutdown();
    }
}