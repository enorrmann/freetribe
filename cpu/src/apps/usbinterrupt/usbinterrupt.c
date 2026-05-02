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
// #include "usbdescriptors.h"
#include "hs_usbdescriptors.h"

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

#define USB_REQ_TYPE_MASK 0x60
#define USB_REQ_TYPE_STANDARD 0x00
#define USB_REQ_TYPE_CLASS 0x20
#define USB_REQ_TYPE_VENDOR 0x40

#define USB_CDC_SET_LINE_CODING 0x20
#define USB_CDC_GET_LINE_CODING 0x21
#define USB_CDC_SET_CONTROL_LINE_STATE 0x22

// Endpoint 0 siempre es 64 en este hardware
#define USB_EP0_MAX_PACKET_SIZE 64

#define USB_SETUP_PACKET_SIZE 8 // El tamaño fijo de los paquetes SETUP en USB es de 8 bytes

typedef struct __attribute__((packed)) {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} USB_SetupPacket;

static uint16_t pendingAddress = 0;
static uint8_t pendingSetAddress = 0;
static uint8_t isConfigured = 0;
static uint8_t cdcLineCoding[7] = {0x00, 0xC2, 0x01, 0x00, 0, 0, 8}; // 115200 8N1
static uint16_t cdcConnected = 0;

void USBSerial_Send(const uint8_t *data, uint32_t len) {
    if (!isConfigured)
        return;

    while (len > 0) {
        uint32_t sendLen = (len > USB_MAX_PACKET_SIZE) ? USB_MAX_PACKET_SIZE : len;

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
        uint8_t val = atoi(cmd + 4);
        ft_set_led(LED_PLAY, (uint8_t)val);
        ft_set_led(LED_PAD_0_BLUE, (uint8_t)val);

        USBSerial_Printf("LED play set to %d\r\n", val);
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
    uint32_t sendLen = (g_uEP0Len > USB_EP0_MAX_PACKET_SIZE) ? USB_EP0_MAX_PACKET_SIZE : g_uEP0Len;
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

    uint16_t csrl2 = HWREGH(USB0_BASE + USB_0_RXCSRL2);

    if (csrl2 & USB_RXCSRL2_RXRDY) { // vino algo por el ep 2
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

void EP0Handler() {
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
    // uint32_t statusEp = USBIntStatusEndpoint(USB0_BASE);

    if (statusCtrl & USB_INTCTRL_RESET) {
        pendingAddress = 0;
        pendingSetAddress = 0;
        isConfigured = 0;
        USBDevAddrSet(USB0_BASE, 0);
    }

    // EP0 handling
    if (csrl0 & USB_CSRL0_RXRDY) { // RXRDY, el Host ha terminado de enviar un paquete de datos hacia el dispositivo y ese paquete ya está almacenado físicamente en el FIFO del Endpoint 0
        USB_SetupPacket setup;
        unsigned int sz;
        USBEndpointDataGet(USB0_BASE, USB_EP_0, (uint8_t *)&setup, &sz);

        if (sz == USB_SETUP_PACKET_SIZE) { // tamaño estblecido para paquetes SETUP por el protocolo USB (8 bytes)
            // Clear RXRDY
            USBDevEndpointDataAck(USB0_BASE, USB_EP_0, false);

            if ((setup.bmRequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_STANDARD) { // Standard Request
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
                    USBDevEndpointConfigSet(USB0_BASE, USB_EP_1, USB_MAX_PACKET_SIZE, USB_EP_MODE_BULK | USB_EP_DEV_IN);
                    USBDevEndpointConfigSet(USB0_BASE, USB_EP_2, USB_MAX_PACKET_SIZE, USB_EP_MODE_BULK | USB_EP_DEV_OUT);

                    USBIntEnableEndpoint(USB0_BASE, USB_INTEP_DEV_OUT_2); // el trabajo real lo hace HWREG(USB_0_OTGBASE + USB_0_INTR_MASK_SET)

                    isConfigured = 1;
                    USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true);
                    break;
                default:
                    USBDevEndpointStall(USB0_BASE, USB_EP_0, USB_EP_DEV_IN);
                    break;
                }
            } else if ((setup.bmRequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS) { // Class request
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
        // Detectamos que el hardware terminó de enviar el paquete anterior, el host me mandó un ack avisando que recibio lo que envie
    } else if (((last_csrl0 & USB_CSRL0_TXRDY) && !(csrl0 & USB_CSRL0_TXRDY)) || ((last_csrl0 & USB_CSRL0_DATAEND) && !(csrl0 & USB_CSRL0_DATAEND))) {

        // Caso A: Todavía quedan datos en el buffer para enviar
        if (g_uEP0Len > 0) {
            // ft_printf("EP0:  %u bytes left", g_uEP0Len); // aca entra a veces asi que no se como refactorear EP0SendData(); con parametros
            // por que no se cual es el data que mandaria aca
            EP0SendData();
            // Caso B: No hay más datos y había un cambio de dirección pendiente (SET_ADDRESS)
            // Nota: En USB, la nueva dirección se aplica solo DESPUÉS de completar la fase de estatus
        } else if (pendingSetAddress) {
            USBDevAddrSet(USB0_BASE, pendingAddress);
            pendingSetAddress = 0;
        }
    }

    last_csrl0 = csrl0;
}

void USB0DeviceIntHandler(void) {

    EP0Handler();
    EP2Handler();

    tripleAck(); // sin esto se cuelga el main
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

    // Esperamos a que el PHY reporte que el reloj está listo (Clock Good)
    while (!(HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) & SYSCFG_CFGCHIP2_USB0PHYCLKGD))
        ;

    // 2. Forzar Full Speed para simplificar la enumeración inicial
    // HWREGB(USB0_BASE + USB_0_POWER) &= ~0x20;

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
    // USBIntEnableControl(USB0_BASE, USB_INTCTRL_RESET | USB_INTCTRL_DISCONNECT | USB_INTCTRL_SUSPEND | USB_INTCTRL_RESUME);
    // USBIntEnableEndpoint(USB0_BASE, USB_INTEP_ALL); // redundante con la linea siguiente

    // Habilitamos:
    // Bit 0 (EP0), Bit 1 (EP1 TX), Bit 18 (EP2 RX)
    // Y los bits de control del bus (Reset, Suspend, etc., que suelen estar en los bits altos)
    HWREG(USB_0_OTGBASE + USB_0_INTR_MASK_SET) = 0x01FF001F; // esto hace lo mismo que USBIntEnableEndpoint y USBIntEnableControl juntos, pero con una sola escritura al wrapper

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
        ft_set_led(LED_TAP, ledState ? 255 : 0);
    }

    if (per_gpio_get_indexed(GPIO_POWER_BUTTON) == 0) {
        ft_shutdown();
    }
}
