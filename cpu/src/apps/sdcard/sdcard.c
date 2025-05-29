#include "csl_spi.h"     // El header SPI que proporcionaste
#include "csl_gpio.h"  // El header GPIO que proporcionaste
#include <stdint.h>   // Para tipos como uint8_t, uint32_t
#include "hw_syscfg0_AM1808.h"
#include "hw_syscfg1_AM1808.h"

// ------ Declaración de ft_print (como la usaríamos) ------
// Necesitarás implementar esta función en tu sistema.
void ft_print(char *text);
// Para simplificar, la defino aquí para que el código compile,
// pero DEBES reemplazarla con tu implementación real.
#include <stdio.h> // Solo para este ejemplo de ft_print
void ft_print(char *text) {
    printf("%s", text); // Implementación de ejemplo
}
// ------ Fin de la declaración de ft_print ------

// ------ Definiciones y Constantes para la Tarjeta SD (Modo SPI) ------

// --- ¡CONFIGURACIÓN ESPEspecifica PARA AM1802 Y TU PLACA! ---
#define SDCARD_SPI_BASE_ADDR    0x01C41000 // SPI0 Base Address
#define SDCARD_GPIO_BASE_ADDR   0x01E26000 // GPIO Global Base Address
#define SDCARD_CS_GPIO_PIN_NUM  37 // EJEMPLO: GP2[5] -> (2 * 16) + 5 = 37. ¡AJUSTA ESTO!
#define MODULE_CLK_FREQ         150000000  // EJEMPLO: 150 MHz. ¡VERIFICA ESTO!
#define SPI_INIT_SPEED          400000
#define SPI_NORMAL_SPEED        25000000
// --- FIN DE LA CONFIGURACIÓN ESPECÍFICA ---

// Base Address para el módulo SYSCFG0 del AM1802
#define SYSCFG0_CTRL_BASE_ADDR (0x01C14000u)

// ¡DEBES AJUSTAR ESTE PIN!
// Número de pin GPIO absoluto (0-143) para el Chip Select (CS) de la SD.
// Ejemplo: si CS está en GP2[5] (Banco 2, Pin 5), pinNumber = (2 * 16) + 5 = 37
#define SDCARD_CS_GPIO_PIN_NUM  37 // ¡EJEMPLO! Debes cambiarlo según tu hardware.

// Frecuencia del reloj del módulo SPI (entrada al prescaler del SPI).
// ¡DEBES VERIFICAR ESTO! Depende de la configuración PRCM de tu AM1802.
// Comunes son 100MHz o 150MHz para los SYSCLKs que alimentan periféricos.
#define MODULE_CLK_FREQ         150000000  // Ejemplo: 150 MHz

// Velocidades SPI
#define SPI_INIT_SPEED          400000     // Velocidad SPI para inicialización (<= 400 kHz)
#define SPI_NORMAL_SPEED        25000000   // Velocidad SPI para operación normal (ej. 25 MHz)
                                           // Máximo para AM1802 SPI es ~50MHz, pero la tarjeta SD también tiene límites.
// --- FIN DE LA CONFIGURACIÓN ESPECÍFICA ---


// Comandos de la tarjeta SD (Modo SPI)
#define CMD0    (0)         // GO_IDLE_STATE
#define CMD8    (8)         // SEND_IF_COND
#define CMD9    (9)         // SEND_CSD
#define CMD10   (10)        // SEND_CID
#define CMD12   (12)        // STOP_TRANSMISSION
#define CMD13   (13)        // SEND_STATUS
#define CMD16   (16)        // SET_BLOCKLEN
#define CMD17   (17)        // READ_SINGLE_BLOCK
#define CMD18   (18)        // READ_MULTIPLE_BLOCK
#define CMD24   (24)        // WRITE_BLOCK
#define CMD25   (25)        // WRITE_MULTIPLE_BLOCK
#define CMD55   (55)        // APP_CMD (prefijo para ACMDs)
#define CMD58   (58)        // READ_OCR
#define ACMD41  (41)        // SD_SEND_OP_COND (APP_CMD debe preceder)

// Tipos de Tarjeta (simplificado)
#define SD_CARD_TYPE_UNKNOWN    0
#define SD_CARD_TYPE_SD1        1 // SDSC v1
#define SD_CARD_TYPE_SD2_SC     2 // SDSC v2
#define SD_CARD_TYPE_SD2_HCXC   3 // SDHC/SDXC v2

// Respuestas de la SD
#define R1_IDLE_STATE           (1 << 0)
#define R1_ERASE_RESET          (1 << 1)
#define R1_ILLEGAL_COMMAND      (1 << 2)
#define R1_COM_CRC_ERROR        (1 << 3)
#define R1_ERASE_SEQUENCE_ERROR (1 << 4)
#define R1_ADDRESS_ERROR        (1 << 5)
#define R1_PARAMETER_ERROR      (1 << 6)
// Para CMD8 (R7 response)
#define R7_VOLTAGE_ACCEPTED     (0x01) // Voltaje 2.7-3.6V
#define R7_ECHO_PATTERN         (0xAA) // Patrón de chequeo para CMD8

// Byte de inicio para un token de datos o error
#define SD_TOKEN_START_BLOCK_READ       0xFE
#define SD_TOKEN_START_BLOCK_WRITE      0xFE
#define SD_TOKEN_START_MULTI_BLOCK_WRITE 0xFC
#define SD_TOKEN_STOP_MULTI_BLOCK_WRITE 0xFD

// Máximos reintentos para comandos y esperas
#define SD_CMD_MAX_RETRIES      250
#define SD_INIT_MAX_RETRIES     2000 // Aumentado para algunas tarjetas lentas
#define SD_READ_TOKEN_RETRIES   2000
#define SD_WRITE_BUSY_RETRIES   50000

// Variable global para el tipo de tarjeta detectada
uint8_t g_sdCardType = SD_CARD_TYPE_UNKNOWN;


// Helper para leer/escribir registros de 32 bits (simplificado)
static inline void write_reg32(uint32_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
}
static inline uint32_t read_reg32(uint32_t addr) {
    return *(volatile uint32_t *)addr;
}

/**
 * @brief Configura la multiplexación de pines para SPI0 y un pin GPIO de ejemplo para CS.
 *
 * ¡MUY IMPORTANTE! Esta función DEBE ser adaptada a tu hardware específico,
 * especialmente para el SDCARD_CS_GPIO_PIN_NUM.
 * Los pines y modos específicos para SPI0 se basan en el AM1802 TRM (spruh84c.pdf),
 * asumiendo una configuración común. Verifica tu esquemático y el TRM.
 *
 * Esta función debe llamarse ANTES de inicializar los drivers SPI o GPIO.
 */
void setup_pin_muxing(void) {
    volatile uint32_t temp_reg_val;
    char msg[128];

    ft_print("Configurando Pin Muxing (SYSCFG0)...\n");

    // --- Configuración para SPI0 ---
    // Según el AM1802 TRM (spruh84c.pdf, Tabla 14-17, p. 583), los pines
    // SPI0_CLK, SPI0_SOMI[0], y SPI0_SIMO[0] están en PINMUX10.
    // - SPI0_CLK (Pin E16) -> PINMUX10[31:28], MODO 0x1
    // - SPI0_SOMI[0] (Pin C15) -> PINMUX10[23:20], MODO 0x1
    // - SPI0_SIMO[0] (Pin D15) -> PINMUX10[19:16], MODO 0x1

    temp_reg_val = read_reg32(SYSCFG0_CTRL_BASE_ADDR + SYSCFG0_PINMUX10);

    // Limpiar los bits actuales para SPI0_CLK
    temp_reg_val &= ~SYSCFG_PINMUX10_PINMUX10_31_28_MASK;
    // Establecer el modo para SPI0_CLK (Modo 0x1)
    temp_reg_val |= (0x1 << SYSCFG_PINMUX10_PINMUX10_31_28_SHIFT);

    // Limpiar los bits actuales para SPI0_SOMI[0]
    temp_reg_val &= ~SYSCFG_PINMUX10_PINMUX10_23_20_MASK;
    // Establecer el modo para SPI0_SOMI[0] (Modo 0x1)
    temp_reg_val |= (0x1 << SYSCFG_PINMUX10_PINMUX10_23_20_SHIFT);

    // Limpiar los bits actuales para SPI0_SIMO[0]
    temp_reg_val &= ~SYSCFG_PINMUX10_PINMUX10_19_16_MASK;
    // Establecer el modo para SPI0_SIMO[0] (Modo 0x1)
    temp_reg_val |= (0x1 << SYSCFG_PINMUX10_PINMUX10_19_16_SHIFT);

    write_reg32(SYSCFG0_CTRL_BASE_ADDR + SYSCFG0_PINMUX10, temp_reg_val);
    ft_print("PINMUX10 para SPI0 (CLK, SOMI, SIMO) configurado.\n");


    // --- Configuración para el pin GPIO de Chip Select (CS) ---
    // ESTO ES UN EJEMPLO y DEBE SER ADAPTADO para tu SDCARD_CS_GPIO_PIN_NUM específico.
    // Asumiremos que SDCARD_CS_GPIO_PIN_NUM = 37, que corresponde a GP2[5].
    // Según AM1802 TRM (Tabla 14-14, p. 577), GP2[5] (Pin B13) está en PINMUX6[7:4].
    // El modo GPIO para GP2[5] es 0x0.

    if (SDCARD_CS_GPIO_PIN_NUM == 37) { // Ejemplo para GP2[5]
        sprintf(msg, "Configurando PINMUX6 para GP2[5] (pin GPIO %d) como CS...\n", SDCARD_CS_GPIO_PIN_NUM);
        ft_print(msg);

        temp_reg_val = read_reg32(SYSCFG0_CTRL_BASE_ADDR + SYSCFG0_PINMUX6);
        // Limpiar los bits actuales para GP2[5] (PINMUX6[7:4])
        temp_reg_val &= ~SYSCFG_PINMUX6_PINMUX6_7_4_MASK;
        // Establecer el modo para GP2[5] a GPIO (Modo 0x0)
        temp_reg_val |= (0x0 << SYSCFG_PINMUX6_PINMUX6_7_4_SHIFT);
        write_reg32(SYSCFG0_CTRL_BASE_ADDR + SYSCFG0_PINMUX6, temp_reg_val);

        sprintf(msg, "PINMUX6 para GP2[5] (pin GPIO %d) configurado como CS.\n", SDCARD_CS_GPIO_PIN_NUM);
        ft_print(msg);
    } else if (SDCARD_CS_GPIO_PIN_NUM == 7) { // Ejemplo anterior para GP0[7] (Pin R2)
        // GP0[7] está en PINMUX1[31:28], Modo 0x0 para GPIO (TRM Tabla 14-9, p.562)
        sprintf(msg, "Configurando PINMUX1 para GP0[7] (pin GPIO %d) como CS...\n", SDCARD_CS_GPIO_PIN_NUM);
        ft_print(msg);
        temp_reg_val = read_reg32(SYSCFG0_CTRL_BASE_ADDR + SYSCFG0_PINMUX1);
        temp_reg_val &= ~SYSCFG_PINMUX1_PINMUX1_31_28_MASK;
        temp_reg_val |= (0x0 << SYSCFG_PINMUX1_PINMUX1_31_28_SHIFT); // Modo 0x0 para gp0[7]
        write_reg32(SYSCFG0_CTRL_BASE_ADDR + SYSCFG0_PINMUX1, temp_reg_val);
        sprintf(msg, "PINMUX1 para GP0[7] (pin GPIO %d) configurado como CS.\n", SDCARD_CS_GPIO_PIN_NUM);
        ft_print(msg);
    }
    else {
        ft_print("*********************************************************************\n");
        sprintf(msg, "ADVERTENCIA: La configuración de PINMUX para tu SDCARD_CS_GPIO_PIN_NUM (%d)\n", SDCARD_CS_GPIO_PIN_NUM);
        ft_print(msg);
        ft_print("             NO está completamente implementada en este ejemplo.\n");
        ft_print("             Debes consultar el TRM AM1802 (Cap. 14) para encontrar\n");
        ft_print("             el registro PINMUX, los bits y el modo correcto para tu pin.\n");
        ft_print("*********************************************************************\n");
    }
    ft_print("Configuración de Pin Muxing completada (revisa los mensajes).\n");
}

// ------ Funciones Auxiliares GPIO y SPI para SD ------

static void sd_cs_gpio_init(unsigned int gpioBaseAddr, unsigned int csPin) {
    // Configurar el pin CS como salida
    // Nota: La multiplexación de este pin a modo GPIO debe estar hecha
    // previamente en los registros PINMUX del módulo SYSCFG del AM1802.
    GPIODirModeSet(gpioBaseAddr, csPin, GPIO_DIR_OUTPUT);
    // Inicialmente, CS debe estar alto (desactivado)
    GPIOPinWrite(gpioBaseAddr, csPin, GPIO_PIN_HIGH);
}

static void sd_cs_assert(unsigned int gpioBaseAddr, unsigned int csPin) {
    GPIOPinWrite(gpioBaseAddr, csPin, GPIO_PIN_LOW);
}

static void sd_cs_deassert(unsigned int gpioBaseAddr, unsigned int csPin) {
    GPIOPinWrite(gpioBaseAddr, csPin, GPIO_PIN_HIGH);
}

// Envía un byte por SPI y recibe un byte
static uint8_t spi_sd_transfer(unsigned int spiBaseAddr, uint8_t data) {
    SPITransmitData1(spiBaseAddr, (unsigned int)data);
    return (uint8_t)SPIDataReceive(spiBaseAddr);
}

// Envía bytes dummy (0xFF) para dar ciclos de reloj a la SD.
static void spi_sd_send_clocks(unsigned int spiBaseAddr, unsigned int count) {
    for (unsigned int i = 0; i < count; ++i) {
        spi_sd_transfer(spiBaseAddr, 0xFF);
    }
}

// Recibe múltiples bytes de la SD
static void spi_sd_receive_multi(unsigned int spiBaseAddr, uint8_t *buffer, unsigned int count) {
    for (unsigned int i = 0; i < count; ++i) {
        buffer[i] = spi_sd_transfer(spiBaseAddr, 0xFF);
    }
}

// Envía un comando a la tarjeta SD
static uint8_t sd_send_command_raw(unsigned int spiBaseAddr, unsigned int gpioBaseAddr, unsigned int csPin,
                               uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t *response_buffer, uint8_t response_len) {
    uint8_t r1_response;
    int retry = 0;

    sd_cs_assert(gpioBaseAddr, csPin);
    spi_sd_transfer(spiBaseAddr, 0xFF); // Byte dummy antes del comando

    spi_sd_transfer(spiBaseAddr, (cmd | 0x40)); // Comando
    spi_sd_transfer(spiBaseAddr, (uint8_t)(arg >> 24)); // Arg byte 3
    spi_sd_transfer(spiBaseAddr, (uint8_t)(arg >> 16)); // Arg byte 2
    spi_sd_transfer(spiBaseAddr, (uint8_t)(arg >> 8));  // Arg byte 1
    spi_sd_transfer(spiBaseAddr, (uint8_t)(arg));       // Arg byte 0
    spi_sd_transfer(spiBaseAddr, crc);                  // CRC

    while ((r1_response = spi_sd_transfer(spiBaseAddr, 0xFF)) == 0xFF) {
        if (retry++ > SD_CMD_MAX_RETRIES) {
            ft_print("Error: Timeout esperando respuesta R1 de SD\n");
            sd_cs_deassert(gpioBaseAddr, csPin); // Asegurar CS alto en error
            return 0xFF;
        }
    }

    if (response_buffer && response_len > 1) {
        response_buffer[0] = r1_response;
        for (uint8_t i = 1; i < response_len; ++i) {
            response_buffer[i] = spi_sd_transfer(spiBaseAddr, 0xFF);
        }
    }
    
    if (cmd == CMD12) { // Byte de "stuff" extra para CMD12
        spi_sd_transfer(spiBaseAddr, 0xFF); 
    }
    // El CS se gestiona por la función llamadora después de esto.
    return r1_response;
}

// ------ Funciones Principales del Driver SD ------

int sd_set_spi_speed(unsigned int spiBaseAddr, unsigned int speed) {
    char msg[64]; // Para ft_print
    // La función SPIClkConfigure del CSL se encarga de calcular el prescaler.
    // Usamos SPI_DATA_FORMAT0 consistentemente.
    SPIClkConfigure(spiBaseAddr, MODULE_CLK_FREQ, speed, SPI_DATA_FORMAT0);
    
    // No hay forma estándar de verificar la velocidad real sin leer registros de prescaler
    // directamente, lo cual no es parte de este CSL de alto nivel.
    // ft_print simple para indicar la velocidad solicitada.
    ft_print("Velocidad SPI solicitada: aprox. ");
    // Convertir speed a cadena simple (evitar sprintf si es posible en embedded)
    // Esta es una forma muy básica, para ft_print.
    if (speed >= 1000000) {
        sprintf(msg, "%u MHz\n", speed / 1000000);
    } else if (speed >= 1000) {
        sprintf(msg, "%u kHz\n", speed / 1000);
    } else {
        sprintf(msg, "%u Hz\n", speed);
    }
    ft_print(msg);
    return 0;
}

int sd_init(unsigned int spiBaseAddr, unsigned int gpioBaseAddr, unsigned int csPin) {
    uint8_t r1_response;
    uint8_t cmd_response_buf[5]; // Para R1, R3 (R1 + 4 bytes), R7 (R1 + 4 bytes)
    char msg[128];

    ft_print("--------------------------------------------------\n");
    ft_print("IMPORTANTE: Asegúrese de que la multiplexación de pines (PINMUX)\n");
    ft_print("para SPI0 (CLK, SIMO, SOMI) y el pin GPIO para CS\n");
    ft_print("esté configurada correctamente en los registros SYSCFG del AM1802.\n");
    ft_print("--------------------------------------------------\n");

    ft_print("Iniciando GPIO para CS de SD...\n");
    sd_cs_gpio_init(gpioBaseAddr, csPin);

    ft_print("Iniciando configuración SPI para SD...\n");
    SPIReset(spiBaseAddr);
    SPIOutOfReset(spiBaseAddr);
    SPIModeConfigure(spiBaseAddr, SPI_MASTER_MODE);
    SPIConfigClkFormat(spiBaseAddr, SPI_CLK_POL_LOW | SPI_CLK_INPHASE, SPI_DATA_FORMAT0); // Modo SPI 0
    SPIDat1Config(spiBaseAddr, 0, SDCARD_CS_NUM); // SDCARD_CS_NUM es el CS# del HW SPI, aquí 0. Sin CSHOLD.
    SPIEnable(spiBaseAddr);
    ft_print("SPI Habilitado.\n");

    sd_set_spi_speed(spiBaseAddr, SPI_INIT_SPEED);

    ft_print("Enviando >74 ciclos de reloj iniciales con CS alto...\n");
    sd_cs_deassert(gpioBaseAddr, csPin);
    spi_sd_send_clocks(spiBaseAddr, 10); // 80 ciclos

    ft_print("Enviando CMD0 (GO_IDLE_STATE)...\n");
    r1_response = sd_send_command_raw(spiBaseAddr, gpioBaseAddr, csPin, CMD0, 0x00000000, 0x95, NULL, 0);
    sd_cs_deassert(gpioBaseAddr, csPin);
    if (r1_response == R1_IDLE_STATE) {
        ft_print("CMD0 OK, Tarjeta en estado Idle.\n");
    } else {
        sprintf(msg, "Error en CMD0. Respuesta: 0x%02X\n", r1_response); ft_print(msg); return -1;
    }
    g_sdCardType = SD_CARD_TYPE_SD1;

    ft_print("Enviando CMD8 (SEND_IF_COND)...\n");
    r1_response = sd_send_command_raw(spiBaseAddr, gpioBaseAddr, csPin, CMD8, 0x000001AA, 0x87, cmd_response_buf, 5);
    sd_cs_deassert(gpioBaseAddr, csPin);
    if (r1_response == R1_IDLE_STATE) {
        if (cmd_response_buf[3] == R7_VOLTAGE_ACCEPTED && cmd_response_buf[4] == R7_ECHO_PATTERN) {
            ft_print("CMD8 OK. Tarjeta es SD Ver2.0+.\n");
            g_sdCardType = SD_CARD_TYPE_SD2_SC;
        } else {
            sprintf(msg, "CMD8 OK pero patrón no coincide. R_bytes: %02X %02X. Asumiendo SD Ver1.x.\n", cmd_response_buf[3], cmd_response_buf[4]);
            ft_print(msg);
            // g_sdCardType sigue siendo SD_CARD_TYPE_SD1
        }
    } else if (r1_response & R1_ILLEGAL_COMMAND) {
        ft_print("CMD8 no soportado. Tarjeta es SD Ver1.x o MMC.\n");
        // g_sdCardType sigue siendo SD_CARD_TYPE_SD1
    } else {
        sprintf(msg, "Error en CMD8. Respuesta: 0x%02X. Asumiendo SD Ver1.x.\n", r1_response); ft_print(msg);
        // g_sdCardType sigue siendo SD_CARD_TYPE_SD1
    }

    ft_print("Iniciando bucle de inicialización (CMD55/ACMD41)...\n");
    int retries = SD_INIT_MAX_RETRIES;
    uint32_t acmd41_arg = (g_sdCardType >= SD_CARD_TYPE_SD2_SC) ? 0x40000000 : 0x00000000; // HCS bit
    uint8_t acmd41_crc = (acmd41_arg == 0x40000000) ? 0x77 : 0xE5;

    do {
        r1_response = sd_send_command_raw(spiBaseAddr, gpioBaseAddr, csPin, CMD55, 0x00000000, 0x65, NULL, 0); // RCA=0 para CMD55
        sd_cs_deassert(gpioBaseAddr, csPin);
        if (r1_response > R1_IDLE_STATE && r1_response != 0x00) { // Error si no es IDLE o READY
            sprintf(msg, "Error en CMD55. Respuesta: 0x%02X\n", r1_response); ft_print(msg); return -3;
        }

        r1_response = sd_send_command_raw(spiBaseAddr, gpioBaseAddr, csPin, ACMD41, acmd41_arg, acmd41_crc, NULL, 0);
        sd_cs_deassert(gpioBaseAddr, csPin);

        if (r1_response == 0x00) { ft_print("ACMD41 OK. Tarjeta inicializada.\n"); break; }
        if (!(r1_response & R1_IDLE_STATE)) {
            sprintf(msg, "Error en ACMD41. Respuesta: 0x%02X\n", r1_response); ft_print(msg); return -4;
        }
        for(volatile int d = 0; d < 10000; d++); // Pequeña demora
    } while (retries-- > 0);

    if (retries <= 0) { ft_print("Error: Timeout en ACMD41.\n"); return -5; }

    if (g_sdCardType == SD_CARD_TYPE_SD2_SC) { // Si era candidata a v2 (CMD8 fue aceptado)
        ft_print("Enviando CMD58 (READ_OCR) para verificar SDHC/XC...\n");
        r1_response = sd_send_command_raw(spiBaseAddr, gpioBaseAddr, csPin, CMD58, 0x00000000, 0xFD, cmd_response_buf, 5);
        sd_cs_deassert(gpioBaseAddr, csPin);
        if (r1_response == 0x00) {
            if (cmd_response_buf[1] & 0x40) { // CCS bit en OCR[31:24] (response_buf[1])
                ft_print("CMD58 OK. Tarjeta es SDHC/SDXC (Alta Capacidad).\n");
                g_sdCardType = SD_CARD_TYPE_SD2_HCXC;
            } else {
                ft_print("CMD58 OK. Tarjeta es SDSC Ver2.0 (Capacidad Estándar).\n");
                // g_sdCardType ya es SD_CARD_TYPE_SD2_SC
            }
        } else {
            sprintf(msg, "Error en CMD58. Respuesta: 0x%02X. Se asume SDSC V2.\n", r1_response); ft_print(msg);
        }
    }
    
    // CMD16 (SET_BLOCKLEN) a 512 es implícito para SDHC/SDXC y a menudo el default para SDSC.
    // No es estrictamente necesario enviarlo para la mayoría de las tarjetas modernas.
    // Si se requiere:
    // ft_print("Enviando CMD16 (SET_BLOCKLEN) a 512 bytes...\n");
    // r1_response = sd_send_command_raw(spiBaseAddr, gpioBaseAddr, csPin, CMD16, 512, 0xFF, NULL, 0);
    // sd_cs_deassert(gpioBaseAddr, csPin);
    // if (r1_response == 0x00) { ft_print("CMD16 OK.\n"); }
    // else { sprintf(msg, "Advertencia: CMD16 falló. R: 0x%02X.\n", r1_response); ft_print(msg); }


    ft_print("Aumentando velocidad SPI para operación normal...\n");
    sd_set_spi_speed(spiBaseAddr, SPI_NORMAL_SPEED);

    ft_print("Inicialización de tarjeta SD completada.\n");
    switch(g_sdCardType) {
        case SD_CARD_TYPE_SD1: ft_print("Tipo de tarjeta: SD Ver1.x (SDSC)\n"); break;
        case SD_CARD_TYPE_SD2_SC: ft_print("Tipo de tarjeta: SD Ver2.0 (SDSC)\n"); break;
        case SD_CARD_TYPE_SD2_HCXC: ft_print("Tipo de tarjeta: SD Ver2.0 (SDHC/SDXC)\n"); break;
        default: ft_print("Tipo de tarjeta: Desconocido o error en detección.\n"); break;
    }
    return 0; // Éxito
}

int sd_read_single_block(unsigned int spiBaseAddr, unsigned int gpioBaseAddr, unsigned int csPin,
                         uint32_t blockAddress, uint8_t *buffer) {
    uint8_t r1_response;
    uint8_t token;
    int retry = SD_READ_TOKEN_RETRIES;
    char msg[64];

    uint32_t address_arg = blockAddress;
    if (g_sdCardType != SD_CARD_TYPE_SD2_HCXC) { // Para SDSC, la dirección es en bytes
        address_arg *= 512;
    }

    sprintf(msg, "Enviando CMD17 (READ_SINGLE_BLOCK) para dir/bloque 0x%08X...\n", (unsigned int)blockAddress);
    ft_print(msg);

    r1_response = sd_send_command_raw(spiBaseAddr, gpioBaseAddr, csPin, CMD17, address_arg, 0xFF, NULL, 0);
    // CS se mantiene bajo por sd_send_command_raw si va bien, para esperar token.
    // No, sd_send_command_raw NO mantiene CS bajo. Hay que hacerlo explícito.
    // Corrección: sd_cs_assert está al inicio de sd_send_command_raw.
    // El CS se liberará por el llamador o al final de esta función.

    if (r1_response != 0x00) {
        sprintf(msg, "Error en CMD17. Respuesta R1: 0x%02X\n", r1_response); ft_print(msg);
        sd_cs_deassert(gpioBaseAddr, csPin); // Liberar CS en error
        return -1;
    }
    ft_print("CMD17 OK. Esperando token de datos...\n");

    while ((token = spi_sd_transfer(spiBaseAddr, 0xFF)) == 0xFF) {
        if (retry-- <= 0) {
            ft_print("Error: Timeout esperando token de datos en CMD17.\n");
            sd_cs_deassert(gpioBaseAddr, csPin);
            return -2;
        }
    }

    if (token == SD_TOKEN_START_BLOCK_READ) {
        ft_print("Token de datos (0xFE) recibido. Leyendo 512 bytes...\n");
        spi_sd_receive_multi(spiBaseAddr, buffer, 512);
        spi_sd_send_clocks(spiBaseAddr, 2); // Leer y descartar 2 bytes de CRC
        ft_print("Lectura de bloque completada.\n");
    } else {
        sprintf(msg, "Error: Token inesperado (0x%02X) en lugar de 0xFE.\n", token); ft_print(msg);
        sd_cs_deassert(gpioBaseAddr, csPin); // Liberar CS en error
        return -3;
    }

    sd_cs_deassert(gpioBaseAddr, csPin); // Liberar CS después de la lectura exitosa
    return 0; // Éxito
}

// --- Función de ejemplo para probar el driver ---
void sd_card_test_main() {
    char msg[128];
    ft_print("\n--- Inicio de Prueba de Tarjeta SD (AM1802) ---\n");

    int init_status = sd_init(SDCARD_SPI_BASE_ADDR, SDCARD_GPIO_BASE_ADDR, SDCARD_CS_GPIO_PIN_NUM);

    if (init_status == 0) {
        ft_print("Tarjeta SD inicializada con éxito.\n");
        sprintf(msg, "Tipo de tarjeta detectado global: %d\n", g_sdCardType); ft_print(msg);

        uint8_t block_buffer[512];
        ft_print("Intentando leer el bloque 0...\n");
        int read_status = sd_read_single_block(SDCARD_SPI_BASE_ADDR, SDCARD_GPIO_BASE_ADDR, SDCARD_CS_GPIO_PIN_NUM, 0, block_buffer);

        if (read_status == 0) {
            ft_print("Bloque 0 leído con éxito.\n");
            ft_print("Primeros 16 bytes del bloque 0:\n");
            for (int i = 0; i < 16; ++i) {
                sprintf(msg, "0x%02X ", block_buffer[i]); ft_print(msg);
            }
            ft_print("\n");

            if (block_buffer[510] == 0x55 && block_buffer[511] == 0xAA) {
                ft_print("Firma MBR (0x55AA) encontrada al final del bloque 0.\n");
            } else {
                ft_print("Advertencia: Firma MBR (0x55AA) NO encontrada.\n");
                sprintf(msg, "Bytes 510, 511: 0x%02X, 0x%02X\n", block_buffer[510], block_buffer[511]); ft_print(msg);
            }
        } else {
            sprintf(msg, "Error al leer el bloque 0. Código: %d\n", read_status); ft_print(msg);
        }
    } else {
        sprintf(msg, "Fallo en la inicialización de la tarjeta SD. Código de error: %d\n", init_status); ft_print(msg);
    }
    ft_print("--- Fin de Prueba de Tarjeta SD ---\n");
}

// Ejemplo de cómo llamarías a esto desde tu main:
int main() {
    // Aquí iría la inicialización de bajo nivel de tu placa AM1802:
    // 1. Configuración de PLLs y relojes del sistema (PRCM).
    //    Esto asegura que MODULE_CLK_FREQ sea el valor correcto.

    // 2. MUY IMPORTANTE: Configuración de la multiplexación de pines (PINMUX)
    setup_pin_muxing(); // <--- ¡LLAMAR ANTES DE USAR SPI o GPIO!

    // Una vez que la placa base está lista...
    // sd_card_test_main(); // Llama a la función de prueba del driver SD

    while(1) {
        // Bucle principal de tu aplicación
    }
    return 0;
}