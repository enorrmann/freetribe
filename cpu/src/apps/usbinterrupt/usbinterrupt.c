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

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "freetribe.h"
#include "hw_types.h"
#include "hw_usb.h"
#include "csl_interrupt.h"
#include "csl_usb.h"
#include "csl_psc.h"
#include "hw_psc_AM1808.h"
#include "hw_usbphyGS60.h"
#include "hw_syscfg0_AM1808.h"
#include "hw_usbOtg_AM1808.h"


static const uint8_t* g_pEP0Data = 0;
static uint32_t g_uEP0Len = 0;
static  uint8_t req_count = 0;

/*----- USB Serial Buffer --------------------------------------------*/

#define USB_SERIAL_BUF_SIZE 512
static uint8_t g_usbRxBuf[USB_SERIAL_BUF_SIZE];
static uint32_t g_usbRxHead = 0;
static uint32_t g_usbRxTail = 0;


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
    18, 1, 0x10, 0x01, 0x02, 0x00, 0x00, 64,
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
static uint16_t cdcConnected = 0;

/*----- Handlers de Interrupción -----*/




void USB0DeviceIntHandler(void) {
    req_count++;
    // 1. LEER Y LIMPIAR EL CORE (Mentor Graphics)
    // Es vital leer estos registros para que el core baje sus flags internos
    uint32_t statusCtrl = USBIntStatusControl(USB0_BASE);
    uint32_t statusEp = USBIntStatusEndpoint(USB0_BASE);

    // 2. LIMPIAR EL WRAPPER DE TI (Nivel 2)
    // El registro INTR_SRC_CLEAR (0x28) requiere escribir 1s para limpiar
    // Le pasamos statusCtrl para limpiar los bits de RESET, SUSPEND, etc.
    HWREG(USB_0_OTGBASE + USB_0_INTR_SRC_CLEAR) = 0xFFFFFFFF; 

    // --- PROCESAMIENTO MÍNIMO ---
    if (statusCtrl & USB_INTCTRL_RESET) {
        isConfigured = 0;
        pendingSetAddress = 0;
    }
    // Aquí puedes llamar a tus funciones de procesamiento de EP0, etc.
    // ----------------------------

    // 3. LIMPIAR EL AINTC (Nivel 3 - Sistema)
    IntSystemStatusClear(SYS_INT_USB0);

    // 4. EOI (End Of Interrupt) - EL "KICK" FINAL
    // Sin esto, el Wrapper nunca libera la línea de IRQ hacia el CPU
    HWREG(USB_0_OTGBASE + USB_0_END_OF_INTR) = 0;
}


/*----- Inicialización -----*/

t_status app_init(void) {
    ft_printf("USB: Starting init...\n");

    // 1. Relojes y PHY
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_USB0, 0, PSC_MDCTL_NEXT_ENABLE);
    UsbPhyOn();

    uint32_t cfgchip2 = HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2);
    cfgchip2 &= ~(0x0000000F | (3 << 13) | (1 << 12)); 
    cfgchip2 |= (2 << 0) | (2 << 13) | (1 << 6); 
    HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) = cfgchip2;
    
    int timeout = 1000000;
    while (!(HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) & (1 << 17)) && timeout--);
    
    // 2. Forzar Full Speed para simplificar la enumeración inicial
    HWREGB(USB0_BASE + USB_0_POWER) &= ~0x20;

    // 3. LIMPIEZA TOTAL PRE-HABILITACIÓN
    USBIntStatusControl(USB0_BASE);
    USBIntStatusEndpoint(USB0_BASE);
    HWREG(USB_0_OTGBASE + USB_0_INTR_SRC_CLEAR) = 0xFFFFFFFF;
    HWREG(USB_0_OTGBASE + USB_0_END_OF_INTR) = 0;

    // 4. CONFIGURAR AINTC (ARM)
    IntRegister(SYS_INT_USB0, USB0DeviceIntHandler);
    IntChannelSet(SYS_INT_USB0, 2);
    IntSystemEnable(SYS_INT_USB0);

    // 5. CONFIGURAR MÁSCARAS
    // Habilitamos Reset, Disconnect, Suspend y Resume en el Core
    USBIntEnableControl(USB0_BASE, USB_INTCTRL_RESET | USB_INTCTRL_DISCONNECT | 
                                   USB_INTCTRL_SUSPEND | USB_INTCTRL_RESUME);
    USBIntEnableEndpoint(USB0_BASE, USB_INTEP_ALL);

    // 6. ABRIR EL PUENTE DEL WRAPPER (Bit 0 = USB0)
    // IMPORTANTE: Solo habilitamos el bit 0. Si usas 0x1F y no manejas 
    // DMA o eventos de VBUS, podrías disparar IRQs que no sabes limpiar.
    HWREG(USB_0_OTGBASE + USB_0_INTR_MASK_SET) = 0x01; 

    // 7. CONECTAR
    USBDevConnect(USB0_BASE);
    
    ft_printf("USB: Init complete. Waiting for bus reset...\n");
    
    return SUCCESS;
}

#define GPIO_POWER_BUTTON 128
void app_run(void) {
    static int heartbeat = 0;
    heartbeat++;
    if (heartbeat >= 100000) {
        ft_printf("Heartbeat: %d, CDC req_count: %d\n", heartbeat, req_count);
        heartbeat = 0;
        static int ledState = 0;
        ledState = !ledState;
        ft_set_led(LED_PLAY, ledState ? 255 : 0);
//            USBSerial_Printf("TEST"); // este funciona

    }
        
    // Manual poll (safer than current interrupt config which causes hangs)
 //   USB0DeviceIntHandler();
   // ProcessUSBSerial();

    if (per_gpio_get_indexed(GPIO_POWER_BUTTON) == 0) {
        ft_shutdown();
    }
}
