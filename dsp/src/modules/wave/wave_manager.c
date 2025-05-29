#include "wave_manager.h"
#include <stdint.h>
#include <stddef.h>  // Para NULL

/**
 * Versión alternativa que inicializa directamente sin usar variable global
 * @param target_wavtab Puntero al array de ondas que se va a inicializar
 * @return 0 si éxito, -1 si error
 */
int wavtab_init_standalone(fract32 (*target_wavtab)[WAVE_TAB_SIZE]) {
    if (target_wavtab == NULL) {
        return -1; // Error: puntero nulo
    }
    int shape,i;

    // Trabajar directamente con el puntero recibido
    for (shape = 0; shape < WAVE_SHAPE_NUM; shape++) {
        for ( i = 0; i < WAVE_TAB_SIZE; i++) {
            // Generar datos directamente en el array pasado
            target_wavtab[shape][i] = generate_wave_sample(shape, i);
        }
    }

    return 0;
}

// Función auxiliar para generar muestras (ejemplo)
static fract32 generate_wave_sample(int shape, int index) {
    // Implementa tu lógica de generación aquí
    // Por ejemplo, para una onda senoidal simple:
     //return (fract32)(sin(2.0 * M_PI * index / WAVE_TAB_SIZE) * 0x7FFFFFFF);
    return 0; // Placeholder
}