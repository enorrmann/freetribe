/*----------------------------------------------------------------------
 * MIDI USB HOST - Freetribe (AM1808)
 * Usando exclusivamente la API de csl_usb.h
 *----------------------------------------------------------------------*/

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

#define USB_0_OTGBASE     SOC_USB_0_OTG_BASE
#define USB_0_END_OF_INTR 0x3c
#ifndef USB0_BASE
#define USB0_BASE         SOC_USB_0_BASE
#endif

/* DEVCTL bits (hw_usb.h sí los define, no csl_usb.h) */
#define DEVCTL_VBUS_MASK  0xC0
#define DEVCTL_VBUS_VALID 0xC0   /* bits[7:6] == 11 → VBUSValid */
#define DEVCTL_FSDEV      0x10
#define DEVCTL_SESSION    0x01

typedef enum {
    HOST_STATE_IDLE = 0,
    HOST_STATE_VBUS_RAMP,
    HOST_STATE_RESET,
    HOST_STATE_RESET_WAIT,
    HOST_STATE_READY,
    HOST_STATE_ERROR
} host_state_t;

static host_state_t host_state  = HOST_STATE_IDLE;
static uint8_t      host_active = 0;
static uint32_t     state_timer = 0;

/*----- Configuración de Pipes usando la API de la CSL -----*/

void USBHost_ConfigMidiPipes(void) {
    /*
     * USBHostEndpointConfig(base, endpoint, maxPacket, nakInterval,
     *                       targetEndpoint, flags)
     *
     * flags: USB_EP_MODE_BULK | USB_EP_SPEED_FULL | USB_EP_HOST_OUT
     */
    USBHostEndpointConfig(USB0_BASE,
                          USB_EP_1,
                          64,                /* MaxPacketSize */
                          0,                 /* NAK interval: sin límite */
                          1,                 /* Target EP en el dispositivo */
                          USB_EP_MODE_BULK | USB_EP_SPEED_FULL | USB_EP_HOST_OUT);

    /* Habilitar interrupción TX del EP1 */
    USBIntEnableEndpoint(USB0_BASE, USB_INTEP_HOST_OUT_1);

    host_active = 1;
}

/*----- Máquina de Estados usando USBIntStatusControl() -----*/

void USBHost_StateMachine(void) {
    uint8_t  devctl  = HWREGB(USB0_BASE + USB_0_DEVCTL);   /* hw_usb.h sí lo define */
    uint32_t intctrl = USBIntStatusControl(USB0_BASE);      /* ← reemplaza USB_0_INTRUSB */
    uint8_t  vbus    = devctl & DEVCTL_VBUS_MASK;

    static uint8_t last_devctl = 0;
    if (last_devctl != devctl) {
        ft_printf("DEVCTL: 0x%02X | STATE: %d", devctl, (int)host_state);
        last_devctl = devctl;
    }

    /* Desconexión desde cualquier estado activo */
    if ((intctrl & USB_INTCTRL_DISCONNECT) && host_state >= HOST_STATE_READY) {
        ft_printf("Device disconnected");
        host_active = 0;
        host_state  = HOST_STATE_IDLE;
        state_timer = 0;
        USBHostPwrDisable(USB0_BASE);   /* ← reemplaza GPIO VBUS */
        return;
    }

    switch (host_state) {

    case HOST_STATE_IDLE:
        USBHostPwrEnable(USB0_BASE);                    /* ← habilita VBUS */
        USBOTGSessionRequest(USB0_BASE, true);           /* ← activa SESSION */
        host_state  = HOST_STATE_VBUS_RAMP;
        state_timer = 0;
        break;

/* En HOST_STATE_VBUS_RAMP — aceptar también 0x80 (AValid) si FSDEV ya está */
case HOST_STATE_VBUS_RAMP: {
    uint8_t vbus = devctl & DEVCTL_VBUS_MASK;

    if (vbus == DEVCTL_VBUS_VALID) {
        /* Camino ideal: VBUS completo */
        ft_printf("VBUS estable (VBUSValid)");
        host_state = HOST_STATE_RESET;
        state_timer = 0;
    } else if ((vbus == 0x80) && (devctl & DEVCTL_FSDEV)) {
        /* VBUS en AValid pero dispositivo detectado → esperar un poco más */
        if (++state_timer > 100000) {
            ft_printf("WARN: VBUS en AValid con FSDEV — posible GPIO faltante");
            /* Aquí es donde debes habilitar el GPIO de VBUS si corresponde */
        }
    } else if (++state_timer > 500000) {
        ft_printf("ERROR: VBUS no sube. DEVCTL=0x%02X", devctl);
        ft_printf("Verificar GPIO switch VBUS en esquemático");
        host_state  = HOST_STATE_ERROR;
        state_timer = 0;
    }
    break;
}

    case HOST_STATE_RESET:
        if ((devctl & DEVCTL_FSDEV) || (intctrl & USB_INTCTRL_CONNECT)) {
            ft_printf("Dispositivo detectado — bus RESET");
            USBHostReset(USB0_BASE, true);          /* ← activa RESET */
            state_timer = 0;
            host_state  = HOST_STATE_RESET_WAIT;
        } else if (++state_timer > 1000000) {
            ft_printf("Sin dispositivo — reintentando VBUS");
            state_timer = 0;
            host_state  = HOST_STATE_VBUS_RAMP;
        }
        break;

    case HOST_STATE_RESET_WAIT:
        if (++state_timer == 50000) {
            USBHostReset(USB0_BASE, false);         /* ← suelta RESET tras 50ms */
            ft_printf("RESET soltado");
        } else if (state_timer >= 60000) {
            ft_printf("Recovery OK — configurando pipes");
            USBHost_ConfigMidiPipes();
            host_state  = HOST_STATE_READY;
            state_timer = 0;
        }
        break;

    case HOST_STATE_READY:
        /* Estado operativo */
        break;

    case HOST_STATE_ERROR:
        if (++state_timer > 2000000) {
            ft_printf("Reintentando...");
            host_active = 0;
            host_state  = HOST_STATE_IDLE;
            state_timer = 0;
        }
        break;
    }
}

/*----- Envío de Nota MIDI -----*/

void USBMidi_Host_SendNote(uint8_t note, uint8_t velocity) {
    if (!host_active) return;

    uint8_t midiPacket[4] = { 0x09, 0x90, note & 0x7F, velocity & 0x7F };

    int timeout = 100000;
    while ((HWREGH(USB0_BASE + USB_0_TXCSRL1) & 0x01) && timeout--);

    if (timeout <= 0) {
        ft_printf("TX timeout — flushing pipe");
        USBFIFOFlush(USB0_BASE, USB_EP_1, USB_EP_HOST_OUT);  /* ← flush por API */
        return;
    }

    USBEndpointDataPut(USB0_BASE, USB_EP_1, midiPacket, 4);
    HWREGH(USB0_BASE + USB_0_TXCSRL1) |= 0x01;
}

/*----- Inicialización -----*/
t_status app_init(void) {
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_USB0, 0, PSC_MDCTL_NEXT_ENABLE);
    UsbPhyOn();

    uint32_t cfg2 = HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2);
    cfg2 &= ~(0x3u | (3u << 13) | (1u << 12));
    cfg2 |=  (2u << 0)   /* Force Host */
          |  (1u << 6)   /* 24 MHz ref */
          |  (1u << 13)  /* PLL clock  */
          |  (1u << 19)  /* OTGVDET_EN — habilitar comparador VBUS ← NUEVO */
          |  (1u << 20); /* OTGID_FLOAT — flotar pin ID para forzar A-side ← NUEVO */
    HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) = cfg2;

    int phy_timeout = 1000000;
    while (!(HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) & (1u << 17)))
        if (--phy_timeout <= 0) { ft_printf("ERROR: PHY timeout"); return ERROR; }
    ft_printf("PHY listo");

    USBIntEnableControl(USB0_BASE, USB_INTCTRL_CONNECT   |
                                   USB_INTCTRL_DISCONNECT |
                                   USB_INTCTRL_VBUS_ERR);
    USBHostMode(USB0_BASE);

    host_state = HOST_STATE_IDLE;
    return SUCCESS;
}
/*----- Ciclo Principal -----*/


void check_vbus_status(void) {
    ft_printf("Checking VBUS status...");
    // Leemos el registro DEVCTL
    uint8_t devctl = HWREGB(SOC_USB_0_BASE + 0x01); 
    
    // Extraemos los bits 3 y 4 (VBUS level)
    uint8_t vbus_level = (devctl >> 3) & 0x03;

    switch(vbus_level) {
        case 0: 
            // VBUS < 0.8V: El cable está desconectado o el puerto USB de la PC está muerto.
            ft_printf("VBUS: Desconectado (0.0V - 0.8V)\n"); 
            break;
        case 1:
            ft_printf("VBUS: Sesión detectada pero baja (0.8V - 2.0V)\n");
            break;
        case 2:
            ft_printf("VBUS: Nivel insuficiente (2.0V - 4.4V)\n");
            break;
        case 3:
            // VBUS > 4.4V: Todo OK. Aquí es cuando el controlador permite la enumeración.
            ft_printf("VBUS: OK - Voltaje correcto (> 4.4V)\n");
            break;
            default:
            ft_printf(" VBUS no sube. DEVCTL=0x%02X", vbus_level);
            
    }
}

void app_run(void) {


        static int heartbeat = 0;
    heartbeat++;
    if (heartbeat >= 100000*2) {
        // ft_printf("Heartbeat: %d, CDC req_count: %d\n", heartbeat, req_count);
        check_vbus_status();
        heartbeat = 0;
    }

    static int note_timer = 0;

    if (per_gpio_get_indexed(128) == 0) {
        ft_shutdown();
    }

    USBHost_StateMachine();

    if (host_active) {
        if (++note_timer > 1000000) {
            note_timer = 0;
            USBMidi_Host_SendNote(60, 127);
        }
    }

    IntSystemStatusClear(SYS_INT_USB0);
    HWREG(USB_0_OTGBASE + USB_0_END_OF_INTR) = 0;
}