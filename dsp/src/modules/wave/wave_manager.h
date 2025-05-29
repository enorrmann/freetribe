#include "osc_waves.h"
#include <stdint.h>
#include <stddef.h>  // Para NULL

/**
 * Versión alternativa que inicializa directamente sin usar variable global
 * @param target_wavtab Puntero al array de ondas que se va a inicializar
 * @return 0 si éxito, -1 si error
 */
int wavtab_init_standalone(fract32 (*target_wavtab)[WAVE_TAB_SIZE]) ;

// Función auxiliar para generar muestras (ejemplo)
static fract32 generate_wave_sample(int shape, int index) ;