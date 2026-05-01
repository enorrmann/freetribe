#ifndef USB_DESCRIPTORS_H
#define USB_DESCRIPTORS_H

#include <stdint.h>

#define USB_MAX_PACKET_SIZE    64

const uint8_t deviceDescriptor[] = {18, 1, 0x10, 0x01, 0x02, 0x00, 0x00, 64, 0x1C, 0x1C, 0x10, 0x00, 0x00, 0x02, 1, 2, 3, 1};
const uint8_t devQualDescriptor[] = {10, 6, 0x00, 0x02, 0x02, 0x00, 0x00, 64, 1, 0};
const uint8_t configDescriptor[] = {
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

const uint8_t string0[] = {4, 3, 0x09, 0x04};
const uint8_t string1[] = {20, 3, 'F', 0, 'r', 0, 'e', 0, 'e', 0, 't', 0, 'r', 0, 'i', 0, 'b', 0, 'e', 0};
const uint8_t string2[] = {16, 3, 'C', 0, 'D', 0, 'C', 0, ' ', 0, 'A', 0, 'C', 0, 'M', 0};
const uint8_t string3[] = {10, 3, '1', 0, '2', 0, '3', 0, '4', 0};

const uint8_t *const strings[] = {string0, string1, string2, string3};
const uint8_t stringLens[] = {sizeof(string0), sizeof(string1), sizeof(string2), sizeof(string3)};

/*
static const uint8_t configDescriptor[] = {
    // --- Configuration Descriptor --- //
    9,                              // Tamaño del descriptor en bytes.
    0x02,                           // Tipo de descriptor: Configuración.
    67, 0,                          // Longitud total (Configuration + Interfaces + Endpoints).
    2,                              // Cantidad de interfaces disponibles (Control y Datos).
    1,                              // Valor para seleccionar esta configuración.
    0,                              // Índice del string que describe esta configuración (0 = ninguno).
    0xC0,                           // Atributos: Alimentado por el dispositivo (Self-powered).
    50,                             // Consumo máximo de energía (50 * 2mA = 100mA).

    // --- Interface 0: CDC Control --- //
    9,                              // Tamaño del descriptor de interfaz.
    0x04,                           // Tipo de descriptor: Interfaz.
    0,                              // Número de interfaz (0).
    0,                              // Ajuste alternativo (usado para diferentes anchos de banda).
    1,                              // Número de endpoints en esta interfaz (solo interrupción).
    0x02,                           // Clase de interfaz: CDC (Communications and CDC Control).
    0x02,                           // Subclase: ACM (Abstract Control Model).
    0x01,                           // Protocolo: Comandos AT comunes (V.25ter).
    0,                              // Índice del string para esta interfaz.

    // --- CDC Functional Descriptors (Configuración específica del módem/puerto) --- //
    5, 0x24, 0x00, 0x10, 0x01,      // Header: Especifica la versión de la clase CDC (1.10).
    5, 0x24, 0x01, 0x00, 1,         // Call Management: Cómo se manejan las llamadas (el dispositivo no las maneja).
    4, 0x24, 0x02, 0x02,            // ACM: Soporta envío/recepción de comandos y estado de línea.
    5, 0x24, 0x06, 0, 1,            // Union: Vincula la Interfaz 0 (Maestra) con la Interfaz 1 (Esclava).

    // --- Endpoint 3: Interrupt IN (Status) --- //
    7,                              // Tamaño del descriptor de endpoint.
    0x05,                           // Tipo de descriptor: Endpoint.
    0x83,                           // Dirección: Endpoint 3, dirección IN (Hacia la PC).
    0x03,                           // Atributo: Tipo de transferencia Interrupción.
    8, 0,                           // Tamaño máximo de paquete (8 bytes).
    255,                            // Intervalo de consulta (Polling) en milisegundos.

    // --- Interface 1: CDC Data --- //
    9,                              // Tamaño del descriptor de interfaz.
    0x04,                           // Tipo de descriptor: Interfaz.
    1,                              // Número de interfaz (1).
    0,                              // Ajuste alternativo.
    2,                              // Número de endpoints en esta interfaz (Bulk IN y Bulk OUT).
    0x0A,                           // Clase de interfaz: Datos (CDC Data).
    0x00,                           // Subclase (no se usa para datos).
    0x00,                           // Protocolo (no se usa para datos).
    0,                              // Índice del string para esta interfaz.

    // --- Endpoint 2: Bulk OUT (Hacia el dispositivo) --- //
    7,                              // Tamaño del descriptor de endpoint.
    0x05,                           // Tipo de descriptor: Endpoint.
    0x02,                           // Dirección: Endpoint 2, dirección OUT (Desde la PC).
    0x02,                           // Atributo: Tipo de transferencia Bulk (Masiva).
    64, 0,                          // Tamaño máximo de paquete (64 bytes).
    0,                              // Intervalo de consulta (no aplica para transferencias Bulk).

    // --- Endpoint 1: Bulk IN (Hacia la PC) --- //
    7,                              // Tamaño del descriptor de endpoint.
    0x05,                           // Tipo de descriptor: Endpoint.
    0x81,                           // Dirección: Endpoint 1, dirección IN (Hacia la PC).
    0x02,                           // Atributo: Tipo de transferencia Bulk (Masiva).
    64, 0,                          // Tamaño máximo de paquete (64 bytes).
    0                               // Intervalo de consulta (no aplica para transferencias Bulk).
};
*/

#endif // USB_DESCRIPTORS_H