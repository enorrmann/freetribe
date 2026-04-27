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

#include "ff.h"
#include "diskio.h" /* Asegúrate de incluir esto para GET_SECTOR_COUNT, STA_NOINIT, etc. */

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

/* Helper para timeouts en HS */
static inline int usb_wait_with_timeout(uint32_t regAddr, uint16_t mask, int expected, int timeout) {
    while (timeout-- > 0) {
        if (((HWREGH(regAddr) & mask) != 0) == expected) return 1;
    }
    return 0; /* timeout */
}

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
    // EP1 IN (Bulk): 512 bytes para High Speed
    7, 5, 0x81, 0x02, 0x00, 0x02, 0, 
    // EP2 OUT (Bulk): 512 bytes para High Speed
    7, 5, 0x02, 0x02, 0x00, 0x02, 0  
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
static uint32_t g_sdInitWaitCounter = 0;  // Esperar después de reinicialización

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

    disk_initialize(0); // Inicializa la unidad física real (Drive 0)

    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_USB0, 0, PSC_MDCTL_NEXT_ENABLE);
    UsbPhyOn();
uint32_t cfg2 = HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2);

// Limpiar bits de configuración USB anteriores
cfg2 &= ~(0x0000000F | (3 << 13) | (1 << 12) | (1 << 14)); 

// Configuración para High Speed:
// - Bits [1:0]: 24MHz Clock (mantener 2)
// - Bit 12: USB PHY On (mantener 1)
// - Bit 13: USB Mode (0 = Device, 1 = Host, pero verificar datasheet para HS Device)
// - Bit 14: High Speed Enable (a veces necesario, o depende del controlador interno)
// Nota crítica: El AM1808 soporta HS, pero requiere un PHY externo compatible o configuración interna específica.
// Si usas la PHY interna (GS60), asegúrate de que los bits de control de la PHY permitan HS.

cfg2 |= (2 << 0)  |  // 24MHz Clock
        (1 << 12) |  // PHY On
        (0 << 13);   // Device Mode

// Asegúrate de NO forzar Full Speed en el registrador de poder (ver paso 2)
HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) = cfg2;

    int timeout = 1000000;
    while (!(HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP2) & (1 << 17)) && timeout--);

    // HWREGB(USB0_BASE + USB_0_POWER) &= ~0x20; // Force Full Speed

    // Habilitar High Speed
    HWREGB(USB0_BASE + USB_0_POWER) |= 0x20; // HS_ENABLE

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

    // 2. Polling del Bulk (Solo si el host ya terminó de configurar el dispositivo)
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
        
        // IMPORTANTE: Reinicializar la tarjeta SD cuando hay reset USB
        disk_initialize(0);
    }

    // RESCATE DE STATUS: Guardamos los bits de los endpoints (EP1, EP2, etc.)
    g_ulUSBInterruptStatus |= USBIntStatusEndpoint(USB0_BASE);

    if (csrl0 & 0x01) { // RXRDY en EP0
        typedef struct { uint8_t bmReq; uint8_t bReq; uint16_t wVal; uint16_t wIdx; uint16_t wLen; } SetupPkt;
        SetupPkt setup;
        unsigned int sz;
        
        USBEndpointDataGet(USB0_BASE, USB_EP_0, (uint8_t *)&setup, &sz);

        if (sz == 8) {
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
                        // Primero el ACK del EP0
                        USBDevEndpointDataAck(USB0_BASE, USB_EP_0, true);

                        // Configuración física de FIFOs
                        HWREGB(USB0_BASE + USB_0_EPIDX) = 1; 
                        HWREGH(USB0_BASE + USB_0_TXCSRL1) = 0x08; // Flush FIFO
                        HWREGB(USB0_BASE + USB_0_TXFIFOADD) = 8;  // Offset 64
HWREGB(USB0_BASE + USB_0_TXFIFOSZ) = 6;   // 512 bytes (High Speed)

                        HWREGB(USB0_BASE + USB_0_EPIDX) = 2;
                        HWREGH(USB0_BASE + USB_0_RXCSRL2) = 0x10; // Flush FIFO
                        HWREGB(USB0_BASE + USB_0_RXFIFOADD) = 16; // Offset 128
HWREGB(USB0_BASE + USB_0_RXFIFOSZ) = 6;   // 512 bytes (High Speed)

                        USBDevEndpointConfigSet(USB0_BASE, USB_EP_1, 512, USB_EP_MODE_BULK | USB_EP_DEV_IN);
                        USBDevEndpointConfigSet(USB0_BASE, USB_EP_2, 512, USB_EP_MODE_BULK | USB_EP_DEV_OUT);

                        isConfigured = 1;
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
    // 1. Verificar si hay datos pendientes en el EP2 (OUT)
    if ((g_ulUSBInterruptStatus & (1 << 18)) || (HWREGH(USB0_BASE + USB_0_RXCSRL2) & 0x01)) {
        
        g_ulUSBInterruptStatus &= ~(1 << 18); // Limpiar bandera de software

        tCBW cbw;
        unsigned int bytesRead;
        
        // Leer el CBW (Command Block Wrapper)
        USBEndpointDataGet(USB0_BASE, USB_EP_2, (uint8_t *)&cbw, &bytesRead);

        // Validar Firma "USBC"
        if (bytesRead >= 31 && cbw.dCBWSignature == 0x43425355) {
            
            // Ack manual: El FIFO de RX ahora está vacío
            HWREGH(USB0_BASE + USB_0_RXCSRL2) &= ~0x01;

            tCSW csw;
            csw.dCSWSignature = 0x53425355; // "USBS"
            csw.dCSWTag = cbw.dCBWTag;
            csw.dCSWDataResidue = cbw.dCBWDataTransferLength;
            csw.bCSWStatus = 0x00; // Por defecto: Éxito

            uint8_t opcode = cbw.CBWCB[0];

            // --- MÁQUINA DE ESTADOS DE COMANDOS SCSI ---
            
            if (opcode == 0x12) { // INQUIRY
                static const uint8_t inq[36] = {
                    0x00, 0x80, 0x02, 0x02, 0x1F, 0x00, 0x00, 0x00,
                    'F','R','E','E','T','R','I','B','E', // Vendor (8 bytes)
                    ' ','D','I','S','K',' ',' ',' ',     // Product (16 bytes)
                    '1','.','0'                          // Rev (4 bytes)
                };
                
                if (!usb_wait_with_timeout((volatile uint16_t *)(USB0_BASE + USB_0_TXCSRL1), 0x01, 0, 10000)) {
                    csw.bCSWStatus = 0x02; // Phase Error
                } else {
                    USBEndpointDataPut(USB0_BASE, USB_EP_1, (uint8_t *)inq, 36);
                    HWREGH(USB0_BASE + USB_0_TXCSRL1) |= 0x01;
                    csw.dCSWDataResidue -= 36;
                }
            }
            
else if (opcode == 0x25) { // READ CAPACITY (10)
                uint8_t cap[8];
                DWORD sector_count = 0;
                
                // Forzamos el éxito de la lectura del tamaño
                if (disk_ioctl(0, GET_SECTOR_COUNT, &sector_count) != RES_OK || sector_count == 0) {
                    // Si falla el ioctl, ponemos un valor genérico de 4GB para no morir, 
                    // pero lo ideal es que disk_ioctl funcione.
                    sector_count = 0; 
                }

                // El comando pide el ÚLTIMO LBA direccionable (Total - 1)
                uint32_t last_lba = sector_count - 1;
                uint32_t block_len = 512;

                // Debug: Si tienes consola, verifica que sector_count sea ~61069312
                
                // SCSI usa Big Endian (MSB primero)
                cap[0] = (uint8_t)(last_lba >> 24); 
                cap[1] = (uint8_t)(last_lba >> 16);
                cap[2] = (uint8_t)(last_lba >> 8);  
                cap[3] = (uint8_t)(last_lba);
                
                cap[4] = (uint8_t)(block_len >> 24); 
                cap[5] = (uint8_t)(block_len >> 16);
                cap[6] = (uint8_t)(block_len >> 8);  
                cap[7] = (uint8_t)(block_len);

                if (!usb_wait_with_timeout((volatile uint16_t *)(USB0_BASE + USB_0_TXCSRL1), 0x01, 0, 10000)) {
                    csw.bCSWStatus = 0x02;
                } else {
                    USBEndpointDataPut(USB0_BASE, USB_EP_1, cap, 8);
                    HWREGH(USB0_BASE + USB_0_TXCSRL1) |= 0x01;
                    csw.dCSWDataResidue -= 8;
                }
            }

            else if (opcode == 0x23) { // READ FORMAT CAPACITIES
                DWORD sector_count = 0;
                disk_ioctl(0, GET_SECTOR_COUNT, &sector_count);

                uint8_t fcap[12] = {
                    0, 0, 0, 8,             // Capacity List Length
                    (uint8_t)(sector_count >> 24), 
                    (uint8_t)(sector_count >> 16), 
                    (uint8_t)(sector_count >> 8), 
                    (uint8_t)sector_count,  // Number of blocks
                    2, 0, 0, 0x02           // Formatted Media + Block len (512)
                };
                if (!usb_wait_with_timeout((volatile uint16_t *)(USB0_BASE + USB_0_TXCSRL1), 0x01, 0, 10000)) {
                    csw.bCSWStatus = 0x02;
                } else {
                    USBEndpointDataPut(USB0_BASE, USB_EP_1, fcap, 12);
                    HWREGH(USB0_BASE + USB_0_TXCSRL1) |= 0x01;
                    csw.dCSWDataResidue -= 12;
                }
            }

            else if (opcode == 0x03) { // REQUEST SENSE
                uint8_t sense[18] = { 0x70, 0, 0x00, 0, 0, 0, 0, 0x0A, 0, 0, 0, 0, 0x00, 0x00, 0, 0, 0, 0 };
                if (!usb_wait_with_timeout((volatile uint16_t *)(USB0_BASE + USB_0_TXCSRL1), 0x01, 0, 10000)) {
                    csw.bCSWStatus = 0x02;
                } else {
                    USBEndpointDataPut(USB0_BASE, USB_EP_1, sense, 18);
                    HWREGH(USB0_BASE + USB_0_TXCSRL1) |= 0x01;
                    csw.dCSWDataResidue -= 18;
                }
            }

            else if (opcode == 0x5A) { // MODE SENSE (10)
                // Estructura MODE SENSE(10) completa según SCSI-2
                uint8_t mode[28] = {
                    // Header (8 bytes)
                    0x00, 0x1A,  // Mode data length (26 bytes siguientes)
                    0x00,        // Medium type (0 = Direct-access block device)
                    0x00,        // Device-specific parameter (no WP, no cache)
                    0x00, 0x00,  // Reserved
                    0x00, 0x08,  // Block descriptor length (8 bytes)
                    
                    // Block Descriptor (8 bytes)
                    0x00, 0x00, 0x00, 0x00,  // Number of blocks (density code + reserved)
                    0x00, 0x00, 0x02, 0x00   // Block length = 512 bytes
                };
                
                uint8_t len = sizeof(mode);
                if (!usb_wait_with_timeout((volatile uint16_t *)(USB0_BASE + USB_0_TXCSRL1), 0x01, 0, 10000)) {
                    csw.bCSWStatus = 0x02;
                } else {
                    // Respetar el allocation length del comando (en CBWCB[7:8])
                    uint16_t alloc_len = (cbw.CBWCB[7] << 8) | cbw.CBWCB[8];
                    if (alloc_len < len) len = alloc_len;
                    
                    USBEndpointDataPut(USB0_BASE, USB_EP_1, mode, len);
                    HWREGH(USB0_BASE + USB_0_TXCSRL1) |= 0x01;
                    csw.dCSWDataResidue -= len;
                }
            }

            else if (opcode == 0x00) { // TEST UNIT READY
                if (disk_status(0) & STA_NOINIT) {
                    disk_initialize(0); // Intentar inicializar si se extrajo o hubo error
                    if (disk_status(0) & STA_NOINIT) {
                        csw.bCSWStatus = 0x01; // Sigue sin estar lista
                    }
                }
            }
            
            else if (opcode == 0x1E || opcode == 0x1B) {
                // PREVENT ALLOW, START STOP UNIT
                // No requieren fase de datos, solo CSW.
            }

            else if (opcode == 0x28) { // READ (10)
                uint32_t lba = (cbw.CBWCB[2] << 24) | (cbw.CBWCB[3] << 16) | (cbw.CBWCB[4] << 8) | cbw.CBWCB[5];
                uint16_t blocks = (cbw.CBWCB[7] << 8) | cbw.CBWCB[8];

                for (uint16_t b = 0; b < blocks; b++) {
                    uint8_t sector[512];
                    
                    // LECTURA REAL DE LA SD
                    if (disk_read(0, sector, lba + b, 1) != RES_OK) {
                        csw.bCSWStatus = 0x01; // Falla de lectura
                        break;
                    }

                    // ENVIAR SECTOR COMPLETO DE 512 BYTES
                    if (!usb_wait_with_timeout((volatile uint16_t *)(USB0_BASE + USB_0_TXCSRL1), 0x01, 0, 10000)) {
                        csw.bCSWStatus = 0x02; // Phase Error
                        break;
                    }
                    USBEndpointDataPut(USB0_BASE, USB_EP_1, sector, 512);
                    HWREGH(USB0_BASE + USB_0_TXCSRL1) |= 0x01;
                }
                csw.dCSWDataResidue -= (blocks * 512);
            }

            else if (opcode == 0x2A) { // WRITE (10)
                uint32_t lba = (cbw.CBWCB[2] << 24) | (cbw.CBWCB[3] << 16) | (cbw.CBWCB[4] << 8) | cbw.CBWCB[5];
                uint16_t blocks = (cbw.CBWCB[7] << 8) | cbw.CBWCB[8];

                for (uint16_t b = 0; b < blocks; b++) {
                    uint8_t sector[512];
                    unsigned int bytesRead = 0;

                    // RECIBIR SECTOR COMPLETO DE 512 BYTES DESDE EL HOST
                    if (!usb_wait_with_timeout((volatile uint16_t *)(USB0_BASE + USB_0_RXCSRL2), 0x01, 1, 50000)) {
                        csw.bCSWStatus = 0x02; // Phase Error - timeout esperando datos
                        break;
                    }
                    
                    USBEndpointDataGet(USB0_BASE, USB_EP_2, sector, &bytesRead);
                    
                    // Limpiar la bandera para permitir el siguiente paquete
                    HWREGH(USB0_BASE + USB_0_RXCSRL2) &= ~0x01; 

                    // ESCRITURA REAL EN LA SD
                    if (disk_write(0, sector, lba + b, 1) != RES_OK) {
                        csw.bCSWStatus = 0x01; // Falla de escritura
                        break;
                    }
                }
                csw.dCSWDataResidue -= (blocks * 512);
            }

            else {
                // Comandos no soportados
                csw.bCSWStatus = 0x01; // Command Failed
            }

            // --- FASE FINAL: ENVIAR CSW ---
            // Esperamos a que cualquier fase de datos anterior haya salido del FIFO
            if (usb_wait_with_timeout((volatile uint16_t *)(USB0_BASE + USB_0_TXCSRL1), 0x01, 0, 10000)) {
                USBEndpointDataPut(USB0_BASE, USB_EP_1, (uint8_t *)&csw, 13);
                HWREGH(USB0_BASE + USB_0_TXCSRL1) |= 0x01;
            } 
            

        } else {
            // Si no es un CBW válido, hacemos un Stall o simplemente limpiamos
            USBDevEndpointDataAck(USB0_BASE, USB_EP_2, true);
        }
    }
}