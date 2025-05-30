#include "wave_lookup.h"
#include "types.h"
#include "custom_aleph_monovoice.h"

/**
 * @brief Lee una muestra de la tabla de ondas usando interpolación lineal
 * @param p Fase actual (32-bit fractional, rango completo)
 * @param dp Delta de fase (incremento por muestra) - usado para antialiasing futuro
 * @param wave_shape Índice de la forma de onda (0 a WAVE_SHAPE_NUM-1)
 * @return Muestra interpolada de 16-bit fractional
 */
fract16 wavetable_lookup(fract32 p, fract32 dp, uint8_t wave_shape) {
    // Limitar el índice de forma de onda
    if (wave_shape >= WAVE_SHAPE_NUM) {
        wave_shape = 0;
    }
    
    // Convertir fase de 32-bit a índice de tabla
    // p va de FR32_MIN a FR32_MAX, necesitamos mapear a 0-(WAVE_TAB_SIZE-1)
    // Desplazamos para obtener los bits más significativos
    uint32_t phase_norm = (uint32_t)(p + FR32_MAX); // Normalizar a rango positivo
    
    // Calcular índice base (parte entera)
    uint32_t index = (phase_norm >> (32 - 10)); // 10 bits para 1024 entradas
    index &= (WAVE_TAB_SIZE - 1); // Asegurar que esté en rango
    
    // Calcular índice siguiente (con wrap-around)
    uint32_t next_index = (index + 1) & (WAVE_TAB_SIZE - 1);
    
    // Obtener fracción para interpolación (bits restantes)
    uint32_t frac_mask = (1 << (32 - 10)) - 1;
    fract32 fraction = (phase_norm & frac_mask) << 10; // Normalizar fracción
    
    // Leer muestras de la tabla
    fract32 sample1 = wavtab[wave_shape][index];
    fract32 sample2 = wavtab[wave_shape][next_index];
    
    // Interpolación lineal: sample1 + (sample2 - sample1) * fraction
    fract32 diff = sub_fr1x32(sample2, sample1);
    fract32 interpolated = add_fr1x32(sample1, mult_fr1x32(diff, fraction));
    
    // Convertir a 16-bit y retornar
    return (fract16)shr_fr1x32(interpolated, 16);
}

/**
 * @brief Versión simplificada sin interpolación (más rápida, menos calidad)
 * @param p Fase actual
 * @param dp Delta de fase (no usado en esta versión)
 * @param wave_shape Índice de la forma de onda
 * @return Muestra directa de 16-bit fractional
 */
fract16 wavetable_lookup_simple(fract32 p, fract32 dp, uint8_t wave_shape) {
    // Limitar el índice de forma de onda
    if (wave_shape >= WAVE_SHAPE_NUM) {
        wave_shape = 0;
    }
    
    // Convertir fase a índice
    uint32_t phase_norm = (uint32_t)(p + FR32_MAX);
    uint32_t index = (phase_norm >> (32 - 10)) & (WAVE_TAB_SIZE - 1);
    
    // Leer directamente de la tabla
    fract32 sample = wavtab[wave_shape][index];
    
    // Convertir a 16-bit y retornar
    return (fract16)shr_fr1x32(sample, 16);
}

/**
 * @brief Versión con morfing entre dos formas de onda
 * @param p Fase actual
 * @param dp Delta de fase
 * @param wave_shape1 Primera forma de onda
 * @param wave_shape2 Segunda forma de onda
 * @param morph_amount Cantidad de morfing (0 = solo shape1, FR16_MAX = solo shape2)
 * @return Muestra mezclada de 16-bit fractional
 */
fract16 wavetable_morph(fract32 p, fract32 dp, uint8_t wave_shape1, uint8_t wave_shape2, fract16 morph_amount) {
    // Obtener muestras de ambas formas de onda
    uint32_t sample1 = wavetable_lookup(p, dp, wave_shape1);
    uint32_t sample2 = wavetable_lookup(p, dp, wave_shape2);
    
    // Mezclar las muestras
    uint32_t diff = sub_fr1x16(sample2, sample1);
    uint32_t morphed = add_fr1x16(sample1, multr_fr1x16(diff, morph_amount));
    
    return morphed;
}