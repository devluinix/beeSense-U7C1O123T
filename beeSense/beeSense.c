#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/time.h"
#include "inc/ssd1306.h"
#include "math.h"

#define I2C_PORT i2c1
#define SDA_PIN 14
#define SCL_PIN 15
#define SSD1306_ADDR 0x3C
#define LCD_WIDTH 128
#define LCD_HEIGHT 64

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
    STATE_CONFIRM
} SystemState;

volatile bool alarm_active = true;
volatile int simulation_mode = 0;
volatile int state = STATE_WELCOME;
volatile uint32_t last_button_a_time = 0;
volatile uint32_t last_button_b_time = 0;
volatile int menu_index = 0;

#define NUM_SPECIES 12

typedef struct
{
    const char *nome;
    float min_temp;
    float max_temp;
    int unidade;
    float peso_mel_anual;
    const char *genero;
} BeeSpecies;

const BeeSpecies species[NUM_SPECIES] = {
    {"Africana", 30.0, 36.0, 65, 50.0, "Apis Mellifera"},
    {"Irai", 26.0, 34.0, 70, 3.5, "Frieseomelitta"},
    {"Limao", 26.0, 34.0, 70, 0.7, "Lestrimelitta"},
    {"Tiuba", 26.0, 34.0, 70, 2.8, "Melipona"},
    {"Mandacaia", 22.0, 32.0, 70, 3.5, "Melipona"},
    {"Urucu", 26.0, 34.0, 70, 7.0, "Melipona"},
    {"Tataira", 22.0, 32.0, 70, 1.4, "Oxytrigona"},
    {"Mirim", 22.0, 32.0, 70, 0.7, "Plebeia"},
    {"Jandaira", 26.0, 34.0, 70, 2.8, "Scaptotrigona"},
    {"Bora", 26.0, 34.0, 70, 4.2, "Tetragona"},
    {"Jatai", 22.0, 32.0, 70, 1.4, "Tetragonisca"},
    {"Mandaguari", 22.0, 32.0, 70, 1.4, "Trigona"}};

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
            menu_index = (menu_index + 1) % NUM_SPECIES;
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
            simulation_mode = (simulation_mode + 1) % 2;
        }
        beep_frequency = (state == STATE_MENU) ? 800 : 1000;
        play_tone = true;
        last_button_b_time = now;
    }
}

// Desenha matriz 5x5; cada célula: 4x4 pixels com espaçamento de 1
void draw_matrix(uint8_t x, uint8_t y, bool pattern[5][5])
{
    for (int i = 0; i < 5; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            uint8_t cell_x = x + j * 5;
            uint8_t cell_y = y + i * 5;
            for (uint8_t dx = 0; dx < 4; dx++)
            {
                for (uint8_t dy = 0; dy < 4; dy++)
                {
                    ssd1306_pixel(&ssd, cell_x + dx, cell_y + dy, pattern[i][j]);
                }
            }
        }
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

        adc_select_input(POT_ADC_UMID);
        uint16_t umid_val = adc_read();
        float umid = umid_val * (100.0f / 4095.0f);

        float red = 0;
        float green = 0;
        float blue = 0;

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
            snprintf(buffer, sizeof(buffer), "%d: %s", menu_index + 1, species[menu_index].nome);
            ssd1306_draw_string(&ssd, buffer, 0, 20);
            ssd1306_draw_string(&ssd, "A: Proximo", 0, 40);
            ssd1306_draw_string(&ssd, "B: Selecionar", 0, 50);
        }
        else if (state == STATE_CONFIRM)
        {
            // LEDs indicam estado do alarme

            if (alarm_active)
            {
                // Valores ideais
                float ideal_umid = 70.0f;
                float ideal_temp = (species[menu_index].min_temp + species[menu_index].max_temp) / 2.0f;

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
                float final_ratio = (0.85f * T_ratio + 0.15f * H_ratio) / (0.85f + 0.15f);

                // Degradê de cor:  final_ratio = 1 -> verde; = 0 -> vermelho; intermediário = amarelo
                green = final_ratio * 10.0f;
                red = (1.0f - final_ratio) * 10.0f;
            }

            if (simulation_mode == 0)
            {
                // Atualiza display
                char info[32];
                snprintf(info, sizeof(info), "Temp: %.1f C", temp);
                ssd1306_draw_string(&ssd, info, 0, 0);
                snprintf(info, sizeof(info), "Umid: %.1f %%", umid);
                ssd1306_draw_string(&ssd, info, 0, 10);
                snprintf(info, sizeof(info), "Alarm: %s", alarm_active ? "ON" : "OFF");
                ssd1306_draw_string(&ssd, info, 0, 20);
                snprintf(info, sizeof(info), "Mode : %d", simulation_mode);
                ssd1306_draw_string(&ssd, info, 0, 30);

                // Matriz 5x5 varia conforme simulation_mode
                bool matrix_pattern[5][5];
                if (!alarm_active)
                {
                    for (int i = 0; i < 5; i++)
                        for (int j = 0; j < 5; j++)
                            matrix_pattern[i][j] = (i == j);
                }
                else
                {
                    bool on = loop_counter >= 5;
                    for (int i = 0; i < 5; i++)
                        for (int j = 0; j < 5; j++)
                            matrix_pattern[i][j] = on;
                }
                draw_matrix(90, 30, matrix_pattern);
            }
            else if (simulation_mode == 1)
            {
                ssd1306_draw_string(&ssd, species[menu_index].nome, 15, 0);
                ssd1306_draw_char(&ssd, '>', 0, 0);
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "%s", species[menu_index].genero);
                ssd1306_draw_string(&ssd, buffer, 0, 10);
                snprintf(buffer, sizeof(buffer), "Max: %.1f C", species[menu_index].max_temp);
                ssd1306_draw_string(&ssd, buffer, 0, 30);
                snprintf(buffer, sizeof(buffer), "Min: %.1f C", species[menu_index].min_temp);
                ssd1306_draw_string(&ssd, buffer, 0, 40);
                snprintf(buffer, sizeof(buffer), "Peso: %.1f", species[menu_index].peso_mel_anual);
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

        ssd1306_send_data(&ssd);
        sleep_ms(100);
    }
    return 0;
}
