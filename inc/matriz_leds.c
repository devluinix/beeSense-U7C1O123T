#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "matriz_leds.h"

// Arquivo .pio para controle da matriz
#include "pio_matrix.pio.h"

// Pino que realizará a comunicação do microcontrolador com a matriz
#define OUT_PIN 7

// Gera o binário que controla a cor de cada célula do LED
// rotina para definição da intensidade de cores do led
uint32_t gerar_binario_cor(double red, double green, double blue)
{
    unsigned char RED, GREEN, BLUE;
    RED = red * 255.0;
    GREEN = green * 255.0;
    BLUE = blue * 255.0;
    return (GREEN << 24) | (RED << 16) | (BLUE << 8);
}

uint configurar_matriz(PIO pio)
{
    bool ok;

    // Define o clock para 128 MHz, facilitando a divisão pelo clock
    ok = set_sys_clock_khz(128000, false);

    // Inicializa todos os códigos stdio padrão que estão ligados ao binário.
    stdio_init_all();

    printf("iniciando a transmissão PIO");
    if (ok)
        printf("clock set to %ld\n", clock_get_hz(clk_sys));

    // configurações da PIO
    uint offset = pio_add_program(pio, &pio_matrix_program);
    uint sm = pio_claim_unused_sm(pio, true);
    pio_matrix_program_init(pio, sm, offset, OUT_PIN);

    return sm;
}

void imprimir_desenho(Matriz_leds_config configuracao, PIO pio, uint sm)
{
    for (int contadorLinha = 4; contadorLinha >= 0; contadorLinha--)
    {
        if (contadorLinha % 2)
        {
            for (int contadorColuna = 0; contadorColuna < 5; contadorColuna++)
            {
                uint32_t valor_cor_binario = gerar_binario_cor(
                    configuracao[contadorLinha][contadorColuna].red,
                    configuracao[contadorLinha][contadorColuna].green,
                    configuracao[contadorLinha][contadorColuna].blue);

                pio_sm_put_blocking(pio, sm, valor_cor_binario);
            }
        }
        else
        {
            for (int contadorColuna = 4; contadorColuna >= 0; contadorColuna--)
            {
                uint32_t valor_cor_binario = gerar_binario_cor(
                    configuracao[contadorLinha][contadorColuna].red,
                    configuracao[contadorLinha][contadorColuna].green,
                    configuracao[contadorLinha][contadorColuna].blue);

                pio_sm_put_blocking(pio, sm, valor_cor_binario);
            }
        }
    }
}

RGB_cod obter_cor_por_parametro_RGB(int red, int green, int blue)
{
    RGB_cod cor_customizada = {red / 255.0, green / 255.0, blue / 255.0};

    return cor_customizada;
}

void actionMatriz(int key, PIO pio, uint sm)
{
    const RGB_cod red = {20, 0, 0};
    const RGB_cod blk = {0, 0, 0};
    if (key == ' ')
    {
        Matriz_leds_config frame = {
            {blk, blk, blk, blk, blk},
            {blk, blk, blk, blk, blk},
            {blk, blk, blk, blk, blk},
            {blk, blk, blk, blk, blk},
            {blk, blk, blk, blk, blk}};
        imprimir_desenho(frame, pio, sm);
    }
    else if (key == '1')
    {
        Matriz_leds_config frame = {
            {blk, blk, red, blk, blk},
            {blk, red, red, blk, blk},
            {blk, blk, red, blk, blk},
            {blk, blk, red, blk, blk},
            {blk, red, red, red, blk}};
        imprimir_desenho(frame, pio, sm);
    }
    else if (key == '2')
    {
        Matriz_leds_config frame = {
            {blk, red, red, red, blk},
            {blk, blk, blk, red, blk},
            {blk, red, red, blk, blk},
            {blk, red, blk, blk, blk},
            {blk, red, red, red, blk}};
        imprimir_desenho(frame, pio, sm);
    }
    else if (key == '3')
    {
        Matriz_leds_config frame = {
            {blk, red, red, red, blk},
            {blk, blk, blk, red, blk},
            {blk, blk, red, red, blk},
            {blk, blk, blk, red, blk},
            {blk, red, red, red, blk}};
        imprimir_desenho(frame, pio, sm);
    }
    else if (key == '4')
    {
        Matriz_leds_config frame = {
            {blk, red, blk, red, blk},
            {blk, red, blk, red, blk},
            {blk, red, red, red, blk},
            {blk, blk, blk, red, blk},
            {blk, blk, blk, red, blk}};
        imprimir_desenho(frame, pio, sm);
    }
    else if (key == '5')
    {
        Matriz_leds_config frame = {
            {blk, red, red, red, blk},
            {blk, red, blk, blk, blk},
            {blk, red, red, red, blk},
            {blk, blk, blk, red, blk},
            {blk, red, red, red, blk}};
        imprimir_desenho(frame, pio, sm);
    }
    else if (key == '6')
    {
        Matriz_leds_config frame = {
            {blk, red, red, red, blk},
            {blk, red, blk, blk, blk},
            {blk, red, red, red, blk},
            {blk, red, blk, red, blk},
            {blk, red, red, red, blk}};
        imprimir_desenho(frame, pio, sm);
    }
    else if (key == '7')
    {
        Matriz_leds_config frame = {
            {blk, red, red, red, blk},
            {blk, blk, blk, red, blk},
            {blk, blk, blk, red, blk},
            {blk, blk, blk, red, blk},
            {blk, blk, blk, red, blk}};
        imprimir_desenho(frame, pio, sm);
    }
    else if (key == '8')
    {
        Matriz_leds_config frame = {
            {blk, red, red, red, blk},
            {blk, red, blk, red, blk},
            {blk, red, red, red, blk},
            {blk, red, blk, red, blk},
            {blk, red, red, red, blk}};
        imprimir_desenho(frame, pio, sm);
    }
    else if (key == '9')
    {
        Matriz_leds_config frame = {
            {blk, red, red, red, blk},
            {blk, red, blk, red, blk},
            {blk, red, red, red, blk},
            {blk, blk, blk, red, blk},
            {blk, blk, blk, red, blk}};
        imprimir_desenho(frame, pio, sm);
    }
    else if (key == '0')
    {
        Matriz_leds_config frame = {
            {blk, red, red, red, blk},
            {blk, red, blk, red, blk},
            {blk, red, blk, red, blk},
            {blk, red, blk, red, blk},
            {blk, red, red, red, blk}};
        imprimir_desenho(frame, pio, sm);
    }
}

void clearMatriz(PIO pio, uint sm)
{
    Matriz_leds_config frame = {
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0}};
    imprimir_desenho(frame, pio, sm);
}

void actionMatrizPattern(bool pattern[5][5], PIO pio, uint sm)
{
    const RGB_cod red = {0.1, 0, 0};
    const RGB_cod orange = {0.2, 0.1, 0};
    const RGB_cod yellow = {0.3, 0.2, 0};
    const RGB_cod limon = {0.1, 0.3, 0};
    const RGB_cod green = {0, 0.4, 0};
    const RGB_cod blk = {0, 0, 0};

    Matriz_leds_config frame = {
        {pattern[0][0] ? green : blk, pattern[0][1] ? green : blk, pattern[0][2] ? green : blk, pattern[0][3] ? green : blk, pattern[0][4] ? green : blk},
        {pattern[1][0] ? limon : blk, pattern[1][1] ? limon : blk, pattern[1][2] ? limon : blk, pattern[1][3] ? limon : blk, pattern[1][4] ? limon : blk},
        {pattern[2][0] ? yellow : blk, pattern[2][1] ? yellow : blk, pattern[2][2] ? yellow : blk, pattern[2][3] ? yellow : blk, pattern[2][4] ? yellow : blk},
        {pattern[3][0] ? orange : blk, pattern[3][1] ? orange : blk, pattern[3][2] ? orange : blk, pattern[3][3] ? orange : blk, pattern[3][4] ? orange : blk},
        {pattern[4][0] ? red : blk, pattern[4][1] ? red : blk, pattern[4][2] ? red : blk, pattern[4][3] ? red : blk, pattern[4][4] ? red : blk}};

    imprimir_desenho(frame, pio, sm);
}