#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "ssd1306.h"

// --- Configurações Básicas ---
#define SAMPLE_RATE     22000     // Taxa de amostragem de 22kHz
#define AUDIO_DURATION  2         // Duração de 2 segundos
#define BUFFER_SIZE     (SAMPLE_RATE * AUDIO_DURATION)

#define MIC_PIN         28        // Pino do microfone
#define PWM_PIN         10        // Pino de saída de áudio
#define BUTTON_A        5         // Botão A para gravação
#define BUTTON_B        6         // Botão B para reprodução
#define LED_RED         13        // LED vermelho (gravação)
#define LED_GREEN       11        // LED verde (reprodução)
#define I2C_SDA         14        // I2C SDA para OLED
#define I2C_SCL         15        // I2C SCL para OLED
#define BUZZER_A_PIN    21        // Pino do buzzer

// --- Parâmetros de Áudio ---
#define DC_OFFSET       2047      // Centro da escala de 12 bits
#define GAIN            2         // Ganho inicial
#define PWM_WRAP        4095      // Resolução do PWM (12 bits)
#define PWM_CLKDIV      1.0f      // Divisor de clock do PWM

// Filtro passa-alta
#define HPF_ALPHA       0.95f     

// Filtro passa-baixa na reprodução
#define LPF_COEFF       0.85f     

// Buffer de áudio
int16_t audio_buffer[BUFFER_SIZE];
volatile uint32_t buffer_index = 0;
volatile bool recording = false;
volatile bool playing = false;

absolute_time_t recording_start_time;
struct repeating_timer timer;

// Variável para armazenar a fatia do PWM
uint pwm_audio_slice;

// === Funções do Display OLED ===

// Desenha o visualizador de áudio no display
void draw_audio_visualizer(int16_t *samples, uint32_t current_index, uint32_t total_samples) {
    struct render_area area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&area);
    uint8_t buffer[ssd1306_buffer_length];
    memset(buffer, 0, sizeof(buffer)); // Limpa o buffer
    
    // Calcula o passo para caber no display
    uint32_t sample_step = total_samples / ssd1306_width;
    if (sample_step < 1) sample_step = 1;
    
    // Desenha cada coluna
    for (uint8_t x = 0; x < ssd1306_width; x++) {
        uint32_t sample_pos = (x * sample_step + current_index) % total_samples;
        int16_t sample = samples[sample_pos];
        
        // Normaliza a amostra para a altura do display
        uint8_t height = (abs(sample) * (ssd1306_height-1)) / (PWM_WRAP/2);
        if (height > ssd1306_height-1) height = ssd1306_height-1;
        
        // Desenha a coluna vertical
        for (uint8_t y = 0; y <= height; y++) {
            uint8_t page = y / 8;
            uint8_t bit_mask = 1 << (y % 8);
            buffer[x + page * ssd1306_width] |= bit_mask;
        }
    }
    
    render_on_display(buffer, &area);
}

// Mostra uma mensagem simples no display
void display_message(const char *msg, int line) {
    struct render_area area = {
        .start_column = 0,
        .end_column = ssd1306_width - 1,
        .start_page = 0,
        .end_page = ssd1306_n_pages - 1
    };
    calculate_render_area_buffer_length(&area);
    uint8_t buffer[ssd1306_buffer_length];
    memset(buffer, 0, sizeof(buffer));
    ssd1306_draw_string(buffer, 0, line * 8, (char *)msg);
    render_on_display(buffer, &area);
}

// --- Configuração de Áudio ---
void setup_audio() {
    adc_init();
    adc_gpio_init(MIC_PIN);
    adc_select_input(2); // ADC input 2 para GPIO28

    pwm_audio_slice = pwm_gpio_to_slice_num(PWM_PIN);

    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, PWM_CLKDIV);
    pwm_config_set_wrap(&cfg, PWM_WRAP);
    
    pwm_init(pwm_audio_slice, &cfg, false);
    
    // Configura o pino PWM_PIN como GPIO normal e coloca em nível baixo
    gpio_init(PWM_PIN);
    gpio_set_dir(PWM_PIN, GPIO_OUT);
    gpio_put(PWM_PIN, 0);

    gpio_init(BUZZER_A_PIN);
    gpio_set_dir(BUZZER_A_PIN, GPIO_OUT);
    gpio_put(BUZZER_A_PIN, 0);
}

// --- Callback de Gravação ---
bool record_audio_cb(struct repeating_timer *t) {
    static float hpf_prev_output = 0.0f;
    static int16_t hpf_prev_input = 0;

    if (!recording || buffer_index >= BUFFER_SIZE) return false;

    // Leitura do ADC com oversampling 4x
    uint32_t raw_sum = 0;
    for (int i = 0; i < 4; i++) {
        raw_sum += adc_read();
    }
    uint16_t raw_avg = raw_sum / 4;

    int16_t centered_sample = (int16_t)raw_avg - DC_OFFSET;
    centered_sample *= GAIN;

    // Filtro passa-alta
    float hpf_input = (float)centered_sample;
    float hpf_output = HPF_ALPHA * (hpf_prev_output + hpf_input - (float)hpf_prev_input);

    hpf_prev_output = hpf_output;
    hpf_prev_input = centered_sample;

    // Clipagem
    int16_t final_sample = (int16_t)hpf_output;
    if (final_sample > (PWM_WRAP / 2) - 1) final_sample = (PWM_WRAP / 2) - 1;
    if (final_sample < -(PWM_WRAP / 2)) final_sample = -(PWM_WRAP / 2);

    audio_buffer[buffer_index++] = final_sample;

    // Atualiza o visualizador
    draw_audio_visualizer(audio_buffer, buffer_index, BUFFER_SIZE);

    // Verifica se o tempo de gravação acabou
    if (absolute_time_diff_us(recording_start_time, get_absolute_time()) >= (AUDIO_DURATION * 1000000) ||
        buffer_index >= BUFFER_SIZE) {
        recording = false;
        cancel_repeating_timer(&timer);
        gpio_put(LED_RED, 0);
        return false;
    }

    return true;
}

// Inicia a gravação
void start_recording() {
    if (recording || playing) return;

    buffer_index = 0;
    recording = true;
    recording_start_time = get_absolute_time();
    gpio_put(LED_RED, 1);
    
    add_repeating_timer_us(-1000000 / SAMPLE_RATE, record_audio_cb, NULL, &timer);
}

// --- Reprodução de Áudio ---
void play_audio() {
    static float lpf_prev_output = 0.0f;
    lpf_prev_output = 0.0f;

    // Configura o pino PWM
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    pwm_set_gpio_level(PWM_PIN, DC_OFFSET); 
    pwm_set_enabled(pwm_audio_slice, true);

    gpio_put(LED_GREEN, 1);

    uint32_t i = 0;
    absolute_time_t last = get_absolute_time();

    while (i < buffer_index && playing) {
        while (absolute_time_diff_us(last, get_absolute_time()) < (1000000 / SAMPLE_RATE)) {
            tight_loop_contents();
        }
        last = get_absolute_time();

        // Processa a amostra
        float current_sample_float = (float)audio_buffer[i];
        float lpf_output = LPF_COEFF * current_sample_float + (1.0f - LPF_COEFF) * lpf_prev_output;
        lpf_prev_output = lpf_output;

        uint16_t pwm_value = (uint16_t)(lpf_output + DC_OFFSET);
        if (pwm_value > PWM_WRAP) pwm_value = PWM_WRAP;
        if (pwm_value < 0) pwm_value = 0;
        
        pwm_set_gpio_level(PWM_PIN, pwm_value);
        i++;

        // Atualiza o visualizador periodicamente
        if (i % 16 == 0) {
            draw_audio_visualizer(audio_buffer, i, buffer_index);
        }
    }

    // Finaliza a reprodução
    pwm_set_gpio_level(PWM_PIN, DC_OFFSET); 
    pwm_set_enabled(pwm_audio_slice, false);
    gpio_init(PWM_PIN);
    gpio_set_dir(PWM_PIN, GPIO_OUT);
    gpio_put(PWM_PIN, 0);

    gpio_put(BUZZER_A_PIN, 0);
    gpio_put(LED_GREEN, 0);
    playing = false;
}

// Inicia a reprodução
void start_playback() {
    if (playing || recording || buffer_index == 0) return;
    playing = true;
}

// --- Configuração dos Botões ---
void init_controls() {
    gpio_init(BUTTON_A);
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_pull_up(BUTTON_B);

    gpio_init(LED_RED);
    gpio_init(LED_GREEN);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
}

// --- Função Principal ---
int main() {
    stdio_init_all();
    setup_audio();
    init_controls();

    // Inicializa o display OLED
    i2c_init(i2c1, 400000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init();
    display_message("Pronto!", 1);

    while (true) {
        if (!gpio_get(BUTTON_A)) {
            start_recording();
            sleep_ms(500);
        }

        if (!gpio_get(BUTTON_B)) {
            start_playback();
            sleep_ms(500);
        }

        if (playing) {
            play_audio();
        }

        sleep_ms(10);
    }

    return 0;
}