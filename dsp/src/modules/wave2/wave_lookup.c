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
    
    // Trabajar directamente con p sin normalización adicional
    // p ya está en el rango correcto de fract32
    
    // Usar los 10 bits más significativos para el índice (sin contar el bit de signo)
    uint32_t phase_uint = (uint32_t)p;
    uint32_t index = (phase_uint >> (32 - 10)) & (WAVE_TAB_SIZE - 1);
    
    // Calcular índice siguiente (con wrap-around)
    uint32_t next_index = (index + 1) & (WAVE_TAB_SIZE - 1);
    
    // Obtener fracción para interpolación
    // Usar los bits restantes después del índice
    uint32_t frac_bits = phase_uint << 10; // Desplazar hacia la izquierda para obtener fracción
    fract32 fraction = (fract32)frac_bits; // Ya está en formato fract32
    
    // Leer muestras de la tabla
    fract32 sample1 = wavtab[wave_shape][index];
    fract32 sample2 = wavtab[wave_shape][next_index];
    
    // Interpolación lineal: sample1 + (sample2 - sample1) * fraction
    fract32 diff = sub_fr1x32(sample2, sample1);
    fract32 interpolated = add_fr1x32(sample1, mult_fr1x32x32(diff, fraction));
    
    // Convertir a 16-bit de manera más precisa
    return (fract16)(interpolated >> 16);
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
 * @param morph_amount Cantidad de morfing int32 (0 = solo shape1, 255 = solo shape2)
 * @return Muestra mezclada de 16-bit fractional
 */
fract16 wavetable_morph(fract32 p, fract32 dp, uint8_t wave_shape1, uint8_t wave_shape2, fract32 morph_amount) {
    // Obtener muestras de ambas formas de onda
    fract16 sample1 = wavetable_lookup(p, dp, wave_shape1);
    fract16 sample2 = wavetable_lookup(p, dp, wave_shape2);
    
    // Convertir morph_amount (int32_t rango 0-255) a fract16
    // Escalar de [0,255] a [0,FR16_MAX] con saturación
    fract16 morph_fract16;
    
    if (morph_amount <= 0) {
        morph_fract16 = 0;
    } else if (morph_amount >= 255) {
        morph_fract16 = FR16_MAX;
    } else {
        // Escalar: (morph_amount * FR16_MAX) / 255
        morph_fract16 = (fract16)((morph_amount * FR16_MAX) / 255);
    }
    
    // Mezclar las muestras usando funciones fract16
    fract16 diff = sub_fr1x16(sample2, sample1);
    fract16 morphed = add_fr1x16(sample1, mult_fr1x16(diff, morph_fract16));
    
    return morphed;
}