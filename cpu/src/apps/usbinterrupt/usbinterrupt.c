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

#include <stdarg.h>

#include "csl_interrupt.h"
#include "csl_usb.h"
#include "freetribe.h"
#include "hw_syscfg0_AM1808.h"
#include "hw_usb.h"

#include "buffer.h"

static const uint8_t *g_pEP0Data = 0;
static uint32_t g_uEP0Len = 0;

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

static const uint8_t deviceDescriptor[] = {18, 1, 0x10, 0x01, 0x02, 0x00, 0x00, 64, 0x1C, 0x1C, 0x10, 0x00, 0x00, 0x02, 1, 2, 3, 1};
static const uint8_t devQualDescriptor[] = {10, 6, 0x00, 0x02, 0x02, 0x00, 0x00, 64, 1, 0};
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
    7, 5, 0x81, 2, 64, 0, 0};

static const uint8_t string0[] = {4, 3, 0x09, 0x04};
static const uint8_t string1[] = {20, 3, 'F', 0, 'r', 0, 'e', 0, 'e', 0, 't', 0, 'r', 0, 'i', 0, 'b', 0, 'e', 0};
static const uint8_t string2[] = {16, 3, 'C', 0, 'D', 0, 'C', 0, ' ', 0, 'A', 0, 'C', 0, 'M', 0};
static const uint8_t string3[] = {10, 3, '1', 0, '2', 0, '3', 0, '4', 0};

static const uint8_t *const strings[] = {string0, string1, string2, string3};
static const uint8_t stringLens[] = {sizeof(string0), sizeof(string1), sizeof(string2), sizeof(string3)};

static uint16_t pendingAddress = 0;
static uint8_t pendingSetAddress = 0;
static uint8_t isConfigured = 0;
static uint8_t cdcLineCoding[7] = {0x00, 0xC2, 0x01, 0x00, 0, 0, 8}; // 115200 8N1
static uint16_t cdcConnected = 0;

void USBSerial_Send(const uint8_t *data, uint32_t len) {
    if (!isConfigured)
        return;

    while (len > 0) {
        uint32_t sendLen = (len > 64) ? 64 : len;

        // Esperar que el endpoint esté libre
        while (HWREGH(USB0_BASE + USB_0_TXCSRL1) & USB_TXCSRL1_TXRDY)
            ;

        USBEndpointDataPut(USB0_BASE, USB_EP_1, (uint8_t *)data, sendLen);

        // Set TXRDY manualmente (más robusto)
        HWREGH(USB0_BASE + USB_0_TXCSRL1) |= USB_TXCSRL1_TXRDY;

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

/*----- Command Processing -------------------------------------------*/

static void ProcessCommand(char *cmd) {
    if (strlen(cmd) == 0)
        return;

    if (strcmp(cmd, "help") == 0) {
        USBSerial_Printf("Available commands:\r\n");
        USBSerial_Printf("  help          - Show this help\r\n");
        USBSerial_Printf("  info          - Show device info\r\n");
        USBSerial_Printf("  echo <msg>    - Echo message\r\n");
        USBSerial_Printf("  led <val>     - Set Play LED brightness (0-255)\r\n");
        USBSerial_Printf("  reboot        - Shutdown system\r\n");
    } else if (strcmp(cmd, "info") == 0) {
        USBSerial_Printf("Manufacturer: Freetribe\r\n");
        USBSerial_Printf("Product: CDC ACM Command Service\r\n");
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

static void ProcessUSBSerial(void) {
    static char lineBuf[128];
    static uint32_t lineIdx = 0;
    static uint8_t welcomeShown = 0;
    uint8_t c;

    if (!isConfigured) {
        welcomeShown = 0;
        return;
    }

    while (usb_rx_pop(&c)) {

        if (!welcomeShown) {
            USBSerial_Printf("\r\n--- Freetribe USB Command Service ---\r\n");
            USBSerial_Printf("Type 'help' for available commands.\r\n> ");
            welcomeShown = 1;
        }
        if (c == '\r' || c == '\n') {
            lineBuf[lineIdx] = '\0';
            USBSerial_Printf("\r\n");
            ProcessCommand(lineBuf);
            lineIdx = 0;
        } else if (c == 0x08 || c == 0x7F) { // Backspace
            if (lineIdx > 0) {
                lineIdx--;
                USBSerial_Printf("\b \b");
            }
        } else if (lineIdx < sizeof(lineBuf) - 1) {
            lineBuf[lineIdx++] = c;
            uint8_t echoBuf[1] = {c};
            USBSerial_Send(echoBuf, 1);
        }
    }
}

/*----- Handlers de Interrupción -----*/
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

/*
 * Bloque de manejo de recepción para el Endpoint 2 (Bulk OUT)
 * Este bloque se ejecuta cuando el host (PC) envía datos al dispositivo.
 */
static void EP2Handler() {
    if (HWREGH(USB0_BASE + USB_0_RXCSRL2) & USB_RXCSRL2_RXRDY) {
        /*
         * HWREGH(USB0_BASE + USB_0_RXCSRL2): Accede al "Receive Control and Status Register Low" del EP2.
         * USB_RXCSRL2_RXRDY: Macro que representa el bit 0 (Receive Packet Ready).
         * Indica que hay un paquete de datos nuevo esperando en el FIFO.
         */

        // USB_0_RXCOUNT2: Registro que contiene el número de bytes recibidos en el último paquete.
        uint32_t count = HWREGH(USB0_BASE + USB_0_RXCOUNT2);

        for (uint32_t i = 0; i < count; i++) {
            /*
             * HWREGB(USB0_BASE + USB_0_FIFO2): Accede al registro del FIFO del Endpoint 2.
             * Cada lectura de este registro extrae un byte del buffer de hardware del USB.
             */
            uint8_t c = HWREGB(USB0_BASE + USB_0_FIFO2);

            // Empujamos el byte al buffer circular de software para su procesamiento posterior.
            usb_rx_push(c);
        }

        /*
         * Llamamos al motor de comandos para procesar los bytes (ej. buscar '\n')
         * inmediatamente después de vaciar el hardware.
         */
        ProcessUSBSerial();

        /*
         * HWREGH(USB0_BASE + USB_0_RXCSRL2) &= ~USB_RXCSRL2_RXRDY:
         * Limpiamos el bit RXRDY escribiendo un 0.
         * Esto le indica al controlador USB que el FIFO ya fue leído y que
         * puede aceptar el siguiente paquete desde la PC.
         */
        HWREGH(USB0_BASE + USB_0_RXCSRL2) &= ~USB_RXCSRL2_RXRDY;
    }
}

void USB0DeviceIntHandler(void) {

    /* csrl0  es el registro de estado/control del endpoint 0, que es el endpoint de control usado para la enumeración y manejo de
    solicitudes estándar de USB. Este registro tiene varios bits que indican el estado actual del endpoint,
    como si hay datos listos para ser leídos (RXRDY), si el endpoint está listo para enviar datos (TXRDY),
    si se ha recibido un paquete SETUP, entre otros. En este handler, se lee este registro para determinar qué
    tipo de evento ocurrió en el endpoint 0 y cómo responder a él.*/

    uint16_t csrl0 = HWREGH(USB0_BASE + USB_0_CSRL0);
    static uint16_t last_csrl0 = 0;

    if (csrl0 & USB_CSRL0_SETEND) {              // SETUPEND
        HWREGB(USB0_BASE + USB_0_CSRL0) |= 0x80; // Clear SETUPEND
    }

    uint32_t statusCtrl = USBIntStatusControl(USB0_BASE);
    uint32_t statusEp = USBIntStatusEndpoint(USB0_BASE);

    if (statusCtrl & USB_INTCTRL_RESET) {
        pendingAddress = 0;
        pendingSetAddress = 0;
        isConfigured = 0;
        USBDevAddrSet(USB0_BASE, 0);
    }

    if (statusEp != 0) {
        ft_printf("EP Interrupt: 0x%08x\n", statusEp);
    }

    // EP0 handling
    if (csrl0 & USB_CSRL0_RXRDY) { // RXRDY
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
                    USBDevEndpointConfigSet(USB0_BASE, USB_EP_1, 64, USB_EP_MODE_BULK | USB_EP_DEV_IN);
                    USBDevEndpointConfigSet(USB0_BASE, USB_EP_2, 64, USB_EP_MODE_BULK | USB_EP_DEV_OUT);

                    USBIntEnableEndpoint(USB0_BASE, (1 << 18)); // EP2 RX IRQ (OUT)

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

        // Detecta si el hardware terminó una transmisión (TXRDY pasó de 1 a 0) 
        // o si finalizó la fase de estatus (DATAEND pasó de 1 a 0).
    } else if (((last_csrl0 & USB_CSRL0_TXRDY) && !(csrl0 & USB_CSRL0_TXRDY)) || ((last_csrl0 & USB_CSRL0_DATAEND) && !(csrl0 & USB_CSRL0_DATAEND))) {
        
        // Caso A: Todavía quedan datos en el buffer para enviar
        if (g_uEP0Len > 0) {
            EP0SendData();
        // Caso B: No hay más datos y había un cambio de dirección pendiente (SET_ADDRESS)
        // Nota: En USB, la nueva dirección se aplica solo DESPUÉS de completar la fase de estatus
        } else if (pendingSetAddress) {
            USBDevAddrSet(USB0_BASE, pendingAddress);
            pendingSetAddress = 0;
        }
    }

    last_csrl0 = csrl0;

    EP2Handler();

    tripleAck();
}

/*----- Inicialización -----*/

t_status app_init(void) {
    ft_printf("USB: Starting init...\n");

    // 1. Relojes y PHY
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_USB0, 0, PSC_MDCTL_NEXT_ENABLE);
    UsbPhyOn();

    uint32_t cfgchip2 = HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2);
    cfgchip2 &= ~(0x0000000F | CFGCHIP2_OTGMODE | CFGCHIP2_USB1PHYCLKMUX);
    cfgchip2 |= CFGCHIP2_REFFREQ_24MHZ | CFGCHIP2_FORCE_DEVICE | CFGCHIP2_PHY_PLLON;
    HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) = cfgchip2;

    int timeout = 1000000;
    // Esperamos a que el PHY reporte que el reloj está listo (Clock Good)
    while (!(HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) & SYSCFG_CFGCHIP2_USB0PHYCLKGD) && timeout--)
        ;

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
    USBIntEnableControl(USB0_BASE, USB_INTCTRL_RESET | USB_INTCTRL_DISCONNECT | USB_INTCTRL_SUSPEND | USB_INTCTRL_RESUME);
    USBIntEnableEndpoint(USB0_BASE, USB_INTEP_ALL);

    // Habilitamos:
    // Bit 0 (EP0), Bit 1 (EP1 TX), Bit 18 (EP2 RX)
    // Y los bits de control del bus (Reset, Suspend, etc., que suelen estar en los bits altos)
    HWREG(USB_0_OTGBASE + USB_0_INTR_MASK_SET) = 0x01FF001F;

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
        // ft_printf("Heartbeat: %d, CDC req_count: %d\n", heartbeat, req_count);
        heartbeat = 0;
        static int ledState = 0;
        ledState = !ledState;
        ft_set_led(LED_PLAY, ledState ? 255 : 0);
    }

    if (per_gpio_get_indexed(GPIO_POWER_BUTTON) == 0) {
        ft_shutdown();
    }
}
