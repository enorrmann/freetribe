/**
 * @file juno_chorus.h 
 * @brief Juno-Style Chorus Effect for Blackfin DSP
 * @author Your Name
 * @date 2025
 * 
 * Implementación de un efecto chorus similar al de los sintetizadores
 * Roland Juno, utilizando delay modulado con LFO senoidal.
 */

#ifndef JUNO_CHORUS_H
#define JUNO_CHORUS_H

#include "fix.h"
#include "fix16_fract.h"
#include "fract_math.h"
#include "types.h"


#ifdef __cplusplus
extern "C" {
#endif




/* ========================================================================== */
/*                          PROTOTIPOS DE FUNCIONES                          */
/* ========================================================================== */

/**
 * @brief Inicializa el chorus con valores por defecto
 * 
 * Inicializa todos los buffers, genera la tabla del LFO y configura
 * los parámetros por defecto estilo Juno.
 * 
 * @note Esta función debe llamarse una sola vez antes de usar el chorus
 */
void chorus_init(void);

/**
 * @brief Configura los parámetros del chorus
 * 
 * @param depth Profundidad de modulación (0x00000000 - 0x7FFFFFFF)
 * @param rate Velocidad del LFO (0x00000666 - 0x00199999)
 * @param feedback Realimentación (0x00000000 - 0x4CCCCCCC)
 * @param mix Mezcla wet/dry (0x00000000 - 0x7FFFFFFF)
 */
void chorus_set_params(fract32 depth, fract32 rate, fract32 feedback, fract32 mix);

/**
 * @brief Procesa una muestra a través del chorus
 * 
 * @param input Muestra de entrada en formato fract32
 * @return Muestra procesada con efecto chorus
 */

fract32 chorus_process_L(fract32 input);
fract32 chorus_process_R(fract32 input);



/**
 * @brief Macro para resetear el chorus (limpiar buffers)
 */
#define CHORUS_RESET() chorus_init()

#ifdef __cplusplus
}
#endif

#endif /* JUNO_CHORUS_H */