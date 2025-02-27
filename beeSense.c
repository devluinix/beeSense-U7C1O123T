#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/time.h"
#include "inc/ssd1306.h"
#include "inc/matriz_leds.h"
#include "math.h"

// I2C definições
#define I2C_PORT i2c1
#define SDA_PIN 14
#define SCL_PIN 15
#define SSD1306_ADDR 0x3C
#define LCD_WIDTH 128
#define LCD_HEIGHT 64

#define MATRIX_PIN 7

// Potenciômetro (GPIO26 = ADC0)
#define POT_ADC_TEMP 0
#define POT_ADC_UMID 1

// Botões
#define BUTTON_A 5
#define BUTTON_B 6
#define DEBOUNCE_MS 200

// LED (opcional, não usado no menu)
#define LED_RED 13
#define LED_GREEN 11
#define LED_BLUE 12

// JOY Y
#define JOY_Y 26

// JOY X
#define JOY_X 27

// Buzzer via PWM
#define BUZZER_A 21
volatile bool play_tone = false;
volatile uint beep_frequency = 0;
volatile uint beep_duration = 0;

// Estados do sistema
typedef enum
{
    STATE_WELCOME,
    STATE_MENU,
    STATE_CONFIRM,
    STATE_CONFIG
} SystemState;

volatile bool alarm_active = true;
volatile int simulation_mode = 0;
volatile int state = STATE_WELCOME;
volatile uint32_t last_button_a_time = 0;
volatile uint32_t last_button_b_time = 0;
volatile int especie_index = 0;
volatile int sensor_index = 0;
bool is_configuring = false;

#define NUM_especies 12

// Especies de Abelhas
typedef struct
{
    const char *nome;
    float min_temp;
    float max_temp;
    int unidade_ideal;
    float peso_mel_anual;
    float max_luz;
    const char *genero;
} Beeespecies;

const Beeespecies especies[NUM_especies] = {
    {"Africana", 30.0, 36.0, 65, 50.0, 5.0, "Apis Mellifera"},
    {"Irai", 26.0, 34.0, 70, 3.5, 2.8, "Frieseomelitta"},
    {"Limao", 26.0, 34.0, 70, 0.7, 1.4, "Lestrimelitta"},
    {"Tiuba", 26.0, 34.0, 70, 2.8, 2.8, "Melipona"},
    {"Mandacaia", 22.0, 32.0, 70, 3.5, 4.2, "Melipona"},
    {"Urucu", 26.0, 34.0, 70, 7.0, 4.2, "Melipona"},
    {"Tataira", 22.0, 32.0, 70, 1.4, 2.8, "Oxytrigona"},
    {"Mirim", 22.0, 32.0, 70, 0.7, 1.4, "Plebeia"},
    {"Jandaira", 26.0, 34.0, 70, 2.8, 4.2, "Scaptotrigona"},
    {"Bora", 26.0, 34.0, 70, 4.2, 4.2, "Tetragona"},
    {"Jatai", 22.0, 32.0, 70, 1.4, 2.8, "Tetragonisca"},
    {"Mandaguari", 22.0, 32.0, 70, 1.4, 2.8, "Trigona"}};

// Sensores Extras
typedef struct
{
    const char *nome;
    float min;
    float max;
    float value;
} Sensores;

#define NUM_SENSORES 4

volatile Sensores sensores[] = {
    {"Peso", 0.0, 60.0, 2.0},
    {"Luminosidade", 0.0, 100.0, 3.0},
    {"Gas VOC", 0.0, 15.0, 0.5},
    {"Vibracao", 0.0, 100.0, 50.0},
};

ssd1306_t ssd;

// Função tone usando PWM para gerar som no buzzer
void tone(uint buzzer_pin, uint frequency, uint duration_ms)
{
    gpio_set_function(buzzer_pin, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(buzzer_pin);
    uint channel = pwm_gpio_to_channel(buzzer_pin);
    float divider = 1.0f;
    float base_clk = 125000000.0f;
    uint32_t wrap = (uint32_t)(base_clk / (divider * frequency)) - 1;
    if (wrap > 65535)
    {
        divider = base_clk / (frequency * 65536);
        wrap = 65535;
    }
    pwm_set_clkdiv(slice_num, divider);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, channel, wrap / 2);
    pwm_set_enabled(slice_num, true);
    sleep_ms(duration_ms);
    pwm_set_enabled(slice_num, false);
    pwm_set_chan_level(slice_num, channel, 0);
    gpio_set_function(buzzer_pin, GPIO_FUNC_SIO); // Muda para digital
    gpio_put(buzzer_pin, 0);                      // Garante nível baixo
}

// Callback dos botões
void gpio_callback(uint gpio, uint32_t events)
{
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (gpio == BUTTON_A && (now - last_button_a_time >= DEBOUNCE_MS))
    {
        if (state == STATE_WELCOME)
        {
            beep_frequency = 500;
            beep_duration = 100;
            play_tone = true;
            state = STATE_MENU;
        }
        else if (state == STATE_MENU)
        {
            especie_index = (especie_index + 1) % NUM_especies;
            beep_frequency = 500;
            beep_duration = 5;
            play_tone = true;
        }
        else if (state == STATE_CONFIG)
        {
            sensor_index = (sensor_index + 1) % NUM_SENSORES;
            beep_frequency = 500;
            beep_duration = 5;
            play_tone = true;
        }
        else if (state == STATE_CONFIRM)
        {
            alarm_active = !alarm_active;
        }
        last_button_a_time = now;
    }
    if (gpio == BUTTON_B && (now - last_button_b_time >= DEBOUNCE_MS))
    {
        beep_duration = 5;
        if (state == STATE_MENU)
        {
            beep_duration = 200;
            state = STATE_CONFIRM;
        }
        else if (state == STATE_CONFIRM)
        {
            beep_duration = 200;
            state = STATE_CONFIG;
            // simulation_mode = (simulation_mode + 1) % 2;
        }
        else if (state == STATE_CONFIG)
        {
            beep_duration = 200;
            state = STATE_CONFIRM;
        }
        beep_frequency = (state == STATE_MENU) ? 800 : 1000;
        play_tone = true;
        last_button_b_time = now;
    }
}

int main()
{
    stdio_init_all();

    // Inicializa I2C e Display
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);
    ssd1306_init(&ssd, LCD_WIDTH, LCD_HEIGHT, false, SSD1306_ADDR, I2C_PORT);
    ssd1306_config(&ssd);

    // Configura LED
    gpio_init(LED_RED);
    gpio_init(LED_GREEN);
    gpio_init(LED_BLUE);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_set_dir(LED_BLUE, GPIO_OUT);
    gpio_put(LED_RED, false);
    gpio_put(LED_GREEN, false);
    gpio_put(LED_BLUE, false);

    gpio_set_function(LED_RED, GPIO_FUNC_PWM);
    gpio_set_function(LED_GREEN, GPIO_FUNC_PWM);
    gpio_set_function(LED_BLUE, GPIO_FUNC_PWM);

    uint slice_num_red = pwm_gpio_to_slice_num(LED_RED);
    uint channel_red = pwm_gpio_to_channel(LED_RED);
    pwm_set_enabled(slice_num_red, true);

    uint slice_num_green = pwm_gpio_to_slice_num(LED_GREEN);
    uint channel_green = pwm_gpio_to_channel(LED_GREEN);
    pwm_set_enabled(slice_num_green, true);

    uint slice_num_blue = pwm_gpio_to_slice_num(LED_BLUE);
    uint channel_blue = pwm_gpio_to_channel(LED_BLUE);
    pwm_set_enabled(slice_num_blue, true);

    // Configura ADC JOY
    adc_init();
    adc_gpio_init(JOY_Y);
    adc_gpio_init(JOY_X);

    // Configura botões A e B
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_FALL, true);

    // Configura buzzer para PWM
    gpio_set_function(BUZZER_A, GPIO_FUNC_PWM);

    // Configura matrix de leds
    PIO pio = pio0;
    uint sm = configurar_matriz(pio);
    clearMatriz(pio, sm);

    uint32_t loop_counter = 0;
    while (true)
    {
        loop_counter++;
        if (loop_counter >= 10)
            loop_counter = 0;

        // Leitura do potenciômetro (valor -6 ate 45)
        adc_select_input(POT_ADC_TEMP);
        uint16_t pot_val = adc_read();
        float temp = -6.0f + (pot_val * (45.0f + 6.0f)) / 4095.0f;
        float valor_sensor = sensores[sensor_index].min + (pot_val * (sensores[sensor_index].max + sensores[sensor_index].min)) / 4095.0f;

        adc_select_input(POT_ADC_UMID);
        uint16_t umid_val = adc_read();
        float umid = umid_val * (100.0f / 4095.0f);

        float red = 0;
        float green = 0;
        float blue = 0;

        float final_ratio = 0.0f;

        ssd1306_fill(&ssd, false);
        if (state == STATE_WELCOME)
        {
            ssd1306_draw_string(&ssd, "   Bem-vindo   ", 3, 10);
            ssd1306_draw_string(&ssd, "-", 0, 15);
            ssd1306_draw_string(&ssd, "-", 119, 15);
            ssd1306_draw_string(&ssd, "   Bee Sense    ", 3, 20);
            ssd1306_draw_string(&ssd, "  Pressione A", 3, 40);
        }
        else if (state == STATE_MENU)
        {
            ssd1306_draw_string(&ssd, "Especie:", 0, 0);
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "%d: %s", especie_index + 1, especies[especie_index].nome);
            ssd1306_draw_string(&ssd, buffer, 0, 20);
            ssd1306_draw_string(&ssd, "A: Proximo", 0, 40);
            ssd1306_draw_string(&ssd, "B: Selecionar", 0, 50);
        }
        else if (state == STATE_CONFIG)
        {
            is_configuring = true;
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "Sensor: %.1f", valor_sensor);
            ssd1306_draw_string(&ssd, buffer, 0, 0);
            snprintf(buffer, sizeof(buffer), "%d: %s", sensor_index + 1, sensores[sensor_index].nome);
            ssd1306_draw_string(&ssd, buffer, 0, 20);
            ssd1306_draw_string(&ssd, "A: Proximo", 0, 40);
            ssd1306_draw_string(&ssd, "B: Selecionar", 0, 50);
        }
        else if (state == STATE_CONFIRM)
        {
            if (is_configuring)
            {
                is_configuring = false;
                sensores[sensor_index].value = valor_sensor;
            }
            // LEDs indicam estado do alarme

            if (alarm_active)
            {
                // Valores ideais
                float ideal_umid = 70.0f;
                float ideal_temp = (especies[especie_index].min_temp + especies[especie_index].max_temp) / 2.0f;

                // Índice de temperatura (assimétrico)
                // Se temp <= ideal: mapeia de ideal a (ideal -15) para índice de 1.0 a 0.0
                // Se temp > ideal:  mapeia de ideal a (ideal +3) para índice de 1.0 a 0.0
                float T_ratio;
                if (temp <= ideal_temp)
                {
                    T_ratio = 1.0f + (temp - ideal_temp) / 15.0f;
                }
                else
                {
                    T_ratio = 1.0f - (temp - ideal_temp) / 3.0f;
                }
                if (T_ratio < 0)
                    T_ratio = 0;
                if (T_ratio > 1)
                    T_ratio = 1;

                // Índice de umidade (simétrico: 70 ±25)
                float H_ratio = 1.0f - (fabs(umid - ideal_umid) / 25.0f);
                if (H_ratio < 0)
                    H_ratio = 0;
                if (H_ratio > 1)
                    H_ratio = 1;

                // Combinação ponderada: temperatura tem mais peso
                final_ratio = (0.85f * T_ratio + 0.15f * H_ratio) / (0.85f + 0.15f);

                // final_ratio vai de 0 a 1

                // Degradê de cor:  final_ratio = 1 -> verde; = 0 -> vermelho; intermediário = amarelo
                green = final_ratio * 10.0f;
                red = (1.0f - final_ratio) * 10.0f;
            }

            if (simulation_mode == 0)
            {
                // Atualiza display
                char info[32];
                snprintf(info, sizeof(info), "Temp : %.1f C", temp);
                ssd1306_draw_string(&ssd, info, 0, 0);
                snprintf(info, sizeof(info), "Umid : %.1f %%", umid);
                ssd1306_draw_string(&ssd, info, 0, 10);
                snprintf(info, sizeof(info), "Luz  : %.1f %%", sensores[1].value);
                ssd1306_draw_string(&ssd, info, 0, 20);
                snprintf(info, sizeof(info), "VOC  : %.1f ppm", sensores[2].value);
                ssd1306_draw_string(&ssd, info, 0, 30);
                snprintf(info, sizeof(info), "Peso : %.1f Kg", sensores[0].value);
                ssd1306_draw_string(&ssd, info, 0, 40);
                snprintf(info, sizeof(info), "Alarm: %s", alarm_active ? "ON" : "OFF");
                ssd1306_draw_string(&ssd, info, 0, 55);
                // snprintf(info, sizeof(info), "Mode  : %d", simulation_mode);
                // ssd1306_draw_string(&ssd, info, 0, 60);

                // Matriz 5x5 varia conforme simulation_mode
                bool matrix_pattern[5][5];
                if (!alarm_active)
                {
                    // LIMPANDO MATRIZ
                    for (int i = 0; i < 5; i++)
                        for (int j = 0; j < 5; j++)
                            matrix_pattern[i][j] = false;
                }
                else
                {

                    // DEFININDO INDICADORES DA MATRIZ
                    // 0 Peso
                    // 1 Luminosidade
                    // 2 Gas VOC"
                    // 3 Vibracao
                    for (int i = 0; i < 5; i++)
                        for (int j = 0; j < 5; j++)
                            matrix_pattern[i][j] = false;

                    int peso_ideal = (int)((sensores[0].value - 0) / (especies[especie_index].peso_mel_anual - 0) * 4.0f);
                    matrix_pattern[4 - peso_ideal][0] = true;
                    int luminosidade_ideal = (int)((sensores[1].max - especies[especie_index].max_luz) / (sensores[1].max - sensores[1].min) * 4.0f);
                    matrix_pattern[4 - luminosidade_ideal][1] = true;
                    int voc_ideal = (int)((8.0f - sensores[2].value) / (8.0f - 0) * 4.0f);
                    matrix_pattern[4 - voc_ideal][2] = true;
                    int vibracao_ideal = (int)((sensores[3].value - 0) / (100.0f - 0) * 4.0f);
                    matrix_pattern[4 - vibracao_ideal][3] = true;

                    int final_ratio_index = (int)(final_ratio * 4.0f);
                    matrix_pattern[4 - final_ratio_index][4] = true;
                }
                actionMatrizPattern(matrix_pattern, pio, sm);
            }
            else if (simulation_mode == 1)
            {
                ssd1306_draw_string(&ssd, especies[especie_index].nome, 15, 0);
                ssd1306_draw_char(&ssd, '>', 0, 0);
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%s", especies[especie_index].genero);
                ssd1306_draw_string(&ssd, buffer, 0, 10);
                snprintf(buffer, sizeof(buffer), "Max: %.1f C", especies[especie_index].max_temp);
                ssd1306_draw_string(&ssd, buffer, 0, 30);
                snprintf(buffer, sizeof(buffer), "Min: %.1f C", especies[especie_index].min_temp);
                ssd1306_draw_string(&ssd, buffer, 0, 40);
                snprintf(buffer, sizeof(buffer), "Peso: %.1f", especies[especie_index].peso_mel_anual);
                ssd1306_draw_string(&ssd, buffer, 0, 50);
            }
        }

        if (play_tone)
        {
            tone(BUZZER_A, beep_frequency, beep_duration);
            play_tone = false;
        }

        pwm_set_gpio_level(LED_RED, red / 255.0 * 65535);
        pwm_set_gpio_level(LED_GREEN, green / 255.0 * 65535);
        pwm_set_gpio_level(LED_BLUE, blue / 255.0 * 65535);

        if (loop_counter == 0)
        {
            printf("{ \"temp\": %.1f, \"umid\": %.1f, \"peso\": %.1f, \"luz\": %.1f, \"voc\": %.1f, \"vibra\": %.1f }\n",
                   temp, umid, sensores[0].value, sensores[1].value, sensores[2].value, sensores[3].value);
        }

        ssd1306_send_data(&ssd);
        sleep_ms(100);
    }
    return 0;
}
