/*----------------------------------------------------------------------
                     Freetribe - USB MSC (Pure Polling + Fixed EP0)
----------------------------------------------------------------------*/

#include <string.h>
#include <stdint.h>

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
#define USB0_BASE             SOC_USB_0_BASE
#endif
#define USB_0_OTGBASE         SOC_USB_0_OTG_BASE
#define USB_0_END_OF_INTR     0x3C
#define USB_0_INTR_MASK_SET   0x30

/* --- Declaración de GPIO --- */
extern int per_gpio_get_indexed(unsigned int id);

void BOT_Task(void) ;
void USB0DeviceIntHandler(void) ;
volatile uint32_t g_ulUSBInterruptStatus = 0;

/*====================================================================
 * Estructuras BOT (Bulk-Only Transport)
 *==================================================================*/
typedef struct {
    uint32_t dCBWSignature;
    uint32_t dCBWTag;
    uint32_t dCBWDataTransferLength;
    uint8_t  bmCBWFlags;
    uint8_t  bCBWLUN;
    uint8_t  bCBWCBLength;
    uint8_t  CBWCB[16];
} __attribute__((packed)) tCBW;

typedef struct {
    uint32_t dCSWSignature;
    uint32_t dCSWTag;
    uint32_t dCSWDataResidue;
    uint8_t  bCSWStatus;
} __attribute__((packed)) tCSW;

/*====================================================================
 * Descriptores y variables globales para EP0
 *==================================================================*/
static const uint8_t deviceDescriptor[] = {
    18, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 64,
    0x1C, 0x1C, 0x20, 0x00, 0x00, 0x02, 1, 2, 3, 1
};

static const uint8_t configDescriptor[] = {
    9, 2, 32, 0, 1, 1, 0, 0xC0, 50,
    9, 4, 0, 0, 2, 0x08, 0x06, 0x50, 0,
    7, 5, 0x81, 0x02, 64, 0, 0, // EP1 IN (Bulk)
    7, 5, 0x02, 0x02, 64, 0, 0  // EP2 OUT (Bulk)
};

static const uint8_t devQualDescriptor[] = {
    10, 0x06, 0x00, 0x02, 0x00, 0x00, 0x00, 64, 1, 0
};

static const uint8_t string0[] = { 4, 3, 0x09, 0x04 };
static const uint8_t string1[] = { 20, 3, 'F',0,'r',0,'e',0,'e',0,'t',0,'r',0,'i',0,'b',0,'e',0 };
static const uint8_t string2[] = { 24, 3, 'F',0,'l',0,'a',0,'s',0,'h',0,' ',0,'D',0,'r',0,'i',0,'v',0,'e',0 };
static const uint8_t string3[] = { 10, 3, '1',0,'2',0,'3',0,'4',0 };

static const uint8_t *const strings[] = { string0, string1, string2, string3 };
static const uint8_t stringLens[] = { sizeof(string0), sizeof(string1), sizeof(string2), sizeof(string3) };

static const uint8_t *g_pEP0Data = 0;
static uint32_t g_uEP0Len = 0;
static uint16_t pendingAddress = 0;
static uint8_t  pendingSetAddress = 0;
static uint8_t  isConfigured = 0;

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




/*====================================================================
 * Inicialización y Bucle Principal
 *==================================================================*/
t_status app_init(void) {
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_USB0, 0, PSC_MDCTL_NEXT_ENABLE);
    UsbPhyOn();

    uint32_t cfg2 = HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2);
    cfg2 &= ~(0x0000000F | (3 << 13) | (1 << 12));
    cfg2 |= (2 << 0) | (2 << 13) | (1 << 6); // 24MHz, PHY On, Device mode
    HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) = cfg2;

    int timeout = 1000000;
    while (!(HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) & (1 << 17)) && timeout--);

    HWREGB(USB0_BASE + USB_0_POWER) &= ~0x20; // Force Full Speed

    IntRegister(SYS_INT_USB0, USB0DeviceIntHandler);
    IntChannelSet(SYS_INT_USB0, 2);
    IntSystemDisable(SYS_INT_USB0); // Polling puro

    USBIntEnableControl(USB0_BASE, USB_INTCTRL_RESET | USB_INTCTRL_DISCONNECT);
    USBIntEnableEndpoint(USB0_BASE, USB_INTEP_ALL);
    
    USBDevConnect(USB0_BASE);

    // Enmascaramos interrupciones del wrapper OTG
    HWREG(USB_0_OTGBASE + USB_0_INTR_MASK_SET) = 0x1FF; 

    return SUCCESS;
}

void app_run(void) {
    // 1. Polling del EP0 (exactamente como funcionaba antes)
    USB0DeviceIntHandler();

    // 2. Polling del Bulk (Solo si Linux ya terminó de configurar el dispositivo)
    if (isConfigured) {
        BOT_Task();
    }

    static uint32_t heartbeat = 0;
    if (++heartbeat >= 100000) {
        heartbeat = 0;
        static int led = 0;
        led = !led;
        ft_set_led(LED_PLAY, led ? 255 : 0);
    }

    if (per_gpio_get_indexed(128) == 0) ft_shutdown();
}




void USB0DeviceIntHandler(void) {
    uint16_t csrl0 = HWREGH(USB0_BASE + USB_0_CSRL0);
    static uint16_t last_csrl0 = 0;

    if (csrl0 & 0x10) { 
        HWREGH(USB0_BASE + USB_0_CSRL0) = 0x80; // SVCSETUPEND
    }

    uint32_t statusCtrl = USBIntStatusControl(USB0_BASE);
    if (statusCtrl & USB_INTCTRL_RESET) {
        pendingAddress = 0;
        pendingSetAddress = 0;
        isConfigured = 0;
        g_ulUSBInterruptStatus = 0; // Limpiar status en reset
        USBDevAddrSet(USB0_BASE, 0);
    }

    // RESCATE DE STATUS: Guardamos los bits de los endpoints (EP1, EP2, etc.)
    // Si no hacemos esto, el BOT_Task nunca verá que llegó un paquete.
    g_ulUSBInterruptStatus |= USBIntStatusEndpoint(USB0_BASE);

    if (csrl0 & 0x01) { // RXRDY en EP0
        typedef struct { uint8_t bmReq; uint8_t bReq; uint16_t wVal; uint16_t wIdx; uint16_t wLen; } SetupPkt;
        SetupPkt setup;
        unsigned int sz;
        
        USBEndpointDataGet(USB0_BASE, USB_EP_0, (uint8_t *)&setup, &sz);

        if (sz == 8) {
            ft_printf("EP0: Req=0x%02X, Val=0x%04X\n", setup.bReq, setup.wVal);
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
                        USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true);
                        break;
                    
                    
case 0x09: // SET_CONFIGURATION
                        ft_printf("EP0: Configured!\n");
                        
                        // A. Configuración Lógica
                        USBDevEndpointConfigSet(USB0_BASE, USB_EP_1, 64, USB_EP_MODE_BULK | USB_EP_DEV_IN);
                        USBDevEndpointConfigSet(USB0_BASE, USB_EP_2, 64, USB_EP_MODE_BULK | USB_EP_DEV_OUT);

                        // B. CONFIGURACIÓN FÍSICA DE FIFOS (Vital para AM1808)
                        // Seleccionamos EP1 para configurar su FIFO de TX
                        HWREGB(USB0_BASE + USB_0_EPIDX) = 1; 
                        HWREGB(USB0_BASE + USB_0_TXFIFOADD) = 8;  // Offset 64 (8*8)
                        HWREGB(USB0_BASE + USB_0_TXFIFOSZ) = 3;   // 64 bytes (2^3 * 8)

                        // Seleccionamos EP2 para configurar su FIFO de RX
                        HWREGB(USB0_BASE + USB_0_EPIDX) = 2;
                        HWREGB(USB0_BASE + USB_0_RXFIFOADD) = 16; // Offset 128 (16*8)
                        HWREGB(USB0_BASE + USB_0_RXFIFOSZ) = 3;   // 64 bytes

                        // C. Habilitar interrupciones de los Endpoints Bulk
                        USBIntEnableEndpoint(USB0_BASE, (1 << 18) | (1 << 1)); 
                        
                        isConfigured = 1;
                        USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true);
                        break;



                    default:
                        USBDevEndpointStall(USB0_BASE, USB_EP_0, USB_EP_DEV_IN);
                        break;
                }
            } else if ((setup.bmReq & 0x60) == 0x20) { // Class Request (MSC)
                if (setup.bReq == 0xFE) { // Get Max LUN
                    ft_printf("EP0: Get Max LUN\n");
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

void BOT_Task(void) {
    // Verificar si el bit del EP2 (1 << 18) se activó en el handler
    // O si el hardware reporta el bit RXRDY directamente (por si acaso)
    if ((g_ulUSBInterruptStatus & (1 << 18)) || (HWREGH(USB0_BASE + USB_0_RXCSRL2) & 0x01)) {
        
        g_ulUSBInterruptStatus &= ~(1 << 18); // Limpiar bandera

        tCBW cbw;
        unsigned int bytesRead;
        
        // Leer el comando del host (CBW)
        USBEndpointDataGet(USB0_BASE, USB_EP_2, (uint8_t *)&cbw, &bytesRead);

        if (bytesRead >= 31 && cbw.dCBWSignature == 0x43425355) {
            ft_printf("BOT: CBW Detectado! Op=0x%02X\n", cbw.CBWCB[0]);

            // Ack manual: Indicar al MUSB que el FIFO está libre
            HWREGH(USB0_BASE + USB_0_RXCSRL2) &= ~0x01;

            tCSW csw;
            csw.dCSWSignature = 0x53425355;
            csw.dCSWTag = cbw.dCBWTag;
            csw.dCSWDataResidue = cbw.dCBWDataTransferLength;
            csw.bCSWStatus = 0x00;

            if (cbw.CBWCB[0] == 0x12) { // INQUIRY
                ft_printf("BOT: Inquiry\n");
                static const uint8_t inq[36] = {
                    0x00, 0x80, 0x02, 0x02, 0x1F, 0x00, 0x00, 0x00,
                    'F','R','E','E','T','R','I','B','E',
                    ' ',' ',' ',' ',' ',' ',' ',' ',
                    '1','.','0'
                };
                
                while(HWREGH(USB0_BASE + USB_0_TXCSRL1) & 0x01);
                USBEndpointDataPut(USB0_BASE, USB_EP_1, (uint8_t *)inq, 36);
                HWREGH(USB0_BASE + USB_0_TXCSRL1) |= 0x01;
                csw.dCSWDataResidue -= 36;
                while(HWREGH(USB0_BASE + USB_0_TXCSRL1) & 0x01);
            }

            // Enviar el CSW para finalizar la transacción
            while(HWREGH(USB0_BASE + USB_0_TXCSRL1) & 0x01);
            USBEndpointDataPut(USB0_BASE, USB_EP_1, (uint8_t *)&csw, 13);
            HWREGH(USB0_BASE + USB_0_TXCSRL1) |= 0x01;
            ft_printf("BOT: CSW Enviado\n");
        } else {
            // Si no es un CBW válido, limpiar el endpoint igualmente
            HWREGH(USB0_BASE + USB_0_RXCSRL2) &= ~0x01;
        }
    }
}