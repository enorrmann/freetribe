
#include "sync.h"
#include "per_gpio.h"




    static int detections = 0;
    static int total_detection_timer = 0;
    static int sync_out_timer = 0;

char* float_to_char(float value) {
    static char str_buf[20];
    int32_t int_part = (int32_t)value;
    int32_t frac_part;

    // obtener parte fraccionaria positiva con 3 decimales
    float frac = (value - (float)int_part);
    if (frac < 0) frac = -frac;
    frac_part = (int32_t)(frac * 1000.0f);

    // convertir cada parte
    char int_buf[12], frac_buf[6];
    itoa(int_part, int_buf, 10);
    itoa(frac_part, frac_buf, 10);

    // concatenar manualmente con '.'
    char *p = str_buf;
    char *s = int_buf;
    while (*s) *p++ = *s++;
    *p++ = '.';
    s = frac_buf;
    while (*s) *p++ = *s++;
    *p = '\0';

    return str_buf;
}

char* int_to_char(int32_t value) {
    // buffer estático para almacenar el resultado (-2147483648 = 11 chars + '\0')
    static char str_buf[12];

    // conversión usando itoa, base 10 (decimal)
    itoa(value, str_buf, 10);

    // devolver puntero al buffer resultante
    return str_buf;
}
void send_sync_out(uint16_t bpm, uint8_t ppqn) {
    // salida del pulso

    // variables estáticas (persisten entre llamadas)
    static uint32_t count = 0;
    static uint32_t pulse_period_ms = 0;
    static uint8_t last_ppqn = 0;
    static uint16_t last_bpm = 0;

    // recalcular el periodo solo si cambian bpm o ppqn
    if (ppqn != last_ppqn || bpm != last_bpm) {
        // duración de una negra en ms = 60000 / bpm
        // periodo entre pulsos = (60000 / bpm) / ppqn
        pulse_period_ms = 60000 / (bpm * ppqn);
        last_ppqn = ppqn;
        last_bpm = bpm;
    }

    // incrementar el contador cada llamada (asumiendo 1 ms por tick)
    count++;

    if (count == pulse_period_ms- PULSE_LENGHT) {
        per_gpio_set(rising_edge_out_bank, rising_edge_out_pin, 1); 
    }
    
    if (count == pulse_period_ms) { 
        per_gpio_set(rising_edge_out_bank, rising_edge_out_pin, 0); 
        count = 0;
    }
}

void send_sync_out2(uint16_t pulse_period_ms) {

    // variables estáticas (persisten entre llamadas)
    static uint32_t count = 0;
    count++;
    
    if (count == pulse_period_ms- PULSE_LENGHT) {
        per_gpio_set(rising_edge_out_bank, rising_edge_out_pin, 1); 
    }
    
    if (count == pulse_period_ms) { 
        per_gpio_set(rising_edge_out_bank, rising_edge_out_pin, 0); 
        count = 0;
    }
    
}


void poll_sync_gpio() {
    static int last_detection_timer = 0;
    static int pulses = 0; // para contar cuántos pulsos ya imprimimos
    last_detection_timer++;


    static int cur_value = 0;
    int value = per_gpio_get(rising_edge_bank, rising_edge_pin);

    if (cur_value != value && last_detection_timer > PULSE_LENGHT) { // anti-rebote
        cur_value = value;
        if (value == 1) { // flanco ascendente
            detections++;
            total_detection_timer += last_detection_timer;
            last_detection_timer = 0;

            // solo imprimir "pulse" según el ppqn elegido
            /*if (detections % ppqn == 0) {
                pulses++;
                //ft_print("pulse ");
                //ft_print(int_to_char(pulses));
                //ft_print(" detections: ");
                //ft_print(int_to_char(detections));
                //ft_print(" bpm: ");
                  // calcular promedio de ms por pulso
            }*/
        }
    }
}

void print_bpm(){
                float avg_ms_per_pulse = (float)total_detection_timer / detections;

                // convertir a BPM reales
                float bpm = 60000.0f / (avg_ms_per_pulse * SYNC_PPQN);
                ft_print(float_to_char(bpm));
                ft_print("\n");
                // reset stats
                total_detection_timer = 0;
                detections = 0;

}

void send_sync_out_pulse_start(){
    per_gpio_set(rising_edge_out_bank, rising_edge_out_pin, 1);
    sync_out_timer = PULSE_LENGHT;
}

void check_sync_out_pulse_end(){
    if (sync_out_timer > 0) {
        sync_out_timer--;
        if (sync_out_timer == 0) {
            per_gpio_set(rising_edge_out_bank, rising_edge_out_pin, 0);
        }
    }

}