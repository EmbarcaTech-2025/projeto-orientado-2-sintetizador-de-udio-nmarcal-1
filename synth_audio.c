// === INCLUDES ===
#include <stdio.h>            // Biblioteca padrão para entrada/saída (ex: printf)
#include <string.h>           // Biblioteca para manipulação de strings
#include "pico/stdlib.h"      // Biblioteca da Raspberry Pi Pico (GPIO, delays, etc)
#include "hardware/adc.h"     // Controle do ADC (Conversor Analógico-Digital)
#include "hardware/pwm.h"     // Controle de PWM (Modulação por Largura de Pulso)
#include "hardware/i2c.h"     // Controle do barramento I2C
#include "hardware/gpio.h"    // Controle direto dos pinos GPIO
#include "ssd1306.h"          // Biblioteca para controlar display OLED SSD1306

// === DEFINES ===
// Parâmetros de áudio
#define SAMPLE_RATE     22000    // Taxa de amostragem: 22 kHz
#define AUDIO_DURATION  2        // Duração da gravação: 2 segundos
#define BUFFER_SIZE     (SAMPLE_RATE * AUDIO_DURATION)  // Total de amostras armazenadas

// Pinos da placa
#define MIC_PIN         28    // Microfone analógico conectado ao GPIO28
#define PWM_PIN         10    // Saída PWM (áudio)
#define BUTTON_A        5     // Botão A (gravação)
#define BUTTON_B        6     // Botão B (reprodução)
#define LED_RED         13    // LED vermelho (indicador de gravação)
#define LED_GREEN       11    // LED verde (indicador de reprodução)
#define I2C_SDA         14    // I2C SDA (display OLED)
#define I2C_SCL         15    // I2C SCL (display OLED)
#define BUZZER_A_PIN    21    // Pino do buzzer (não usado ativamente)

// Parâmetros do áudio
#define DC_OFFSET       2047    // Offset para centralizar sinal em 12 bits
#define GAIN            2       // Ganho aplicado ao áudio
#define PWM_WRAP        4095    // Resolução do PWM: 12 bits
#define PWM_CLKDIV      1.0f    // Divisor de clock do PWM

// Filtro passa-alta
#define HPF_ALPHA       0.95f   // Constante do filtro passa-alta

// === VARIÁVEIS GLOBAIS ===
int16_t audio_buffer[BUFFER_SIZE];      // Buffer para armazenar o áudio
volatile uint32_t buffer_index = 0;     // Posição atual no buffer
volatile bool recording = false;        // Flag de gravação
volatile bool playing = false;          // Flag de reprodução
absolute_time_t recording_start_time;   // Tempo de início da gravação
struct repeating_timer timer;           // Timer de amostragem do ADC
uint pwm_audio_slice;                   // Slice de PWM usada para áudio

// === FUNÇÃO DE DISPLAY ===
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

// === SETUP DE ÁUDIO ===
void setup_audio() {
    adc_init();                           // Inicializa ADC
    adc_gpio_init(MIC_PIN);               // Configura pino do microfone
    adc_select_input(2);                  // Seleciona ADC input 2 (GPIO28)

    pwm_audio_slice = pwm_gpio_to_slice_num(PWM_PIN);   // Descobre slice do PWM

    pwm_config cfg = pwm_get_default_config();          // Configura PWM
    pwm_config_set_clkdiv(&cfg, PWM_CLKDIV);
    pwm_config_set_wrap(&cfg, PWM_WRAP);
    pwm_init(pwm_audio_slice, &cfg, false);             // Inicializa mas não ativa PWM

    // Configura PWM_PIN como GPIO normal
    gpio_init(PWM_PIN);
    gpio_set_dir(PWM_PIN, GPIO_OUT);
    gpio_put(PWM_PIN, 0);                               // Silêncio inicial

    // Inicializa pino do buzzer
    gpio_init(BUZZER_A_PIN);
    gpio_set_dir(BUZZER_A_PIN, GPIO_OUT);
    gpio_put(BUZZER_A_PIN, 0);
}

// === CALLBACK DE GRAVAÇÃO ===
bool record_audio_cb(struct repeating_timer *t) {
    static float hpf_prev_output = 0.0f;
    static int16_t hpf_prev_input = 0;

    if (!recording || buffer_index >= BUFFER_SIZE) return false;

    // Leitura ADC com oversampling (4x)
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

    // Clipping
    int16_t final_sample = (int16_t)hpf_output;
    if (final_sample > (PWM_WRAP / 2) - 1) final_sample = (PWM_WRAP / 2) - 1;
    if (final_sample < -(PWM_WRAP / 2)) final_sample = -(PWM_WRAP / 2);

    // Salva no buffer
    audio_buffer[buffer_index++] = final_sample;

    // Atualiza display a cada 1/10 de segundo
    if (buffer_index % (SAMPLE_RATE / 10) == 0) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Gravando %lu%%", (buffer_index * 100) / BUFFER_SIZE);
        display_message(msg, 0);
    }

    // Finaliza gravação se tempo acabou ou buffer cheio
    if (absolute_time_diff_us(recording_start_time, get_absolute_time()) >= (AUDIO_DURATION * 1000000) ||
        buffer_index >= BUFFER_SIZE) {
        recording = false;
        cancel_repeating_timer(&timer);
        gpio_put(LED_RED, 0);
        display_message("Gravacao OK!", 0);
        return false;
    }

    return true;
}

// === INICIAR GRAVAÇÃO ===
void start_recording() {
    if (recording || playing) return;

    buffer_index = 0;
    recording = true;
    recording_start_time = get_absolute_time();
    gpio_put(LED_RED, 1);
    display_message("Iniciando gravacao...", 0);

    add_repeating_timer_us(-1000000 / SAMPLE_RATE, record_audio_cb, NULL, &timer);
}

// === REPRODUZIR ÁUDIO ===
void play_audio() {
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    pwm_set_gpio_level(PWM_PIN, DC_OFFSET);
    pwm_set_enabled(pwm_audio_slice, true);

    gpio_put(LED_GREEN, 1);
    display_message("Tocando...", 0);

    uint32_t i = 0;
    absolute_time_t last = get_absolute_time();

    while (i < buffer_index && playing) {
        while (absolute_time_diff_us(last, get_absolute_time()) < (1000000 / SAMPLE_RATE)) {
            tight_loop_contents();
        }
        last = get_absolute_time();

        uint16_t pwm_value = (uint16_t)(audio_buffer[i++] + DC_OFFSET);
        if (pwm_value > PWM_WRAP) pwm_value = PWM_WRAP;
        pwm_set_gpio_level(PWM_PIN, pwm_value);

        if (i % (SAMPLE_RATE / 10) == 0) {
            char msg[32];
            snprintf(msg, sizeof(msg), "Tocando %lu%%", (i * 100) / buffer_index);
            display_message(msg, 1);
        }
    }

    // Pós-reprodução: silenciar e desligar PWM
    pwm_set_gpio_level(PWM_PIN, DC_OFFSET);
    pwm_set_enabled(pwm_audio_slice, false);
    gpio_init(PWM_PIN);
    gpio_set_dir(PWM_PIN, GPIO_OUT);
    gpio_put(PWM_PIN, 0);

    gpio_put(BUZZER_A_PIN, 0);
    gpio_put(LED_GREEN, 0);
    playing = false;
    display_message("Reproducao OK!", 1);
}

// === INICIAR REPRODUÇÃO ===
void start_playback() {
    if (playing || recording || buffer_index == 0) return;
    playing = true;
}

// === INICIALIZAR BOTÕES E LEDS ===
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

// === FUNÇÃO PRINCIPAL ===
int main() {
    stdio_init_all();   // Inicializa I/O padrão
    setup_audio();      // Configura áudio
    init_controls();    // Configura botões e LEDs

    // Inicializa I2C e OLED
    i2c_init(i2c1, 400000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init();
    display_message("Sistema pronto!", 1);

    // Loop principal
    while (true) {
        // Se botão A for pressionado, grava áudio
        if (!gpio_get(BUTTON_A)) {
            start_recording();
            sleep_ms(500);   // Debounce simples
        }

        // Se botão B for pressionado, toca áudio
        if (!gpio_get(BUTTON_B)) {
            start_playback();
            sleep_ms(500);   // Debounce simples
        }

        // Se estamos em reprodução, toca áudio
        if (playing) {
            play_audio();
        }

        sleep_ms(10);   // Delay de 10ms
    }

    return 0;
}
