#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "bitdoglab.pio.h"

#define NUM_PIXELS 25
#define OUT_PIN 7
#define BUTTON_A 5
#define BUTTON_B 6
#define LED_RED 13
#define LED_GREEN 11
#define LED_BLUE 12

// Variáveis globais
volatile int current_number = 0;
volatile bool button_a_pressed = false;
volatile bool button_b_pressed = false;
bool led_state = false;
uint32_t current_color = 0x00FF0000; // Vermelho inicial
int color_index = 0;

// Paleta de cores
const uint32_t color_palette[7] = {
    0x00FF0000, 0x0000FF00, 0x000000FF,
    0x00FFFF00, 0x00FF00FF, 0x0000FFFF,
    0x00FFFFFF
};

// Padrões numéricos corrigidos
const uint8_t number_patterns[10][25] = {
    {
        1, 1, 1, 1, 1,
        1, 0, 0, 0, 1,
        1, 0, 0, 0, 1,
        1, 0, 0, 0, 1,
        1, 1, 1, 1, 1  
    },// Número 0
    {
        0, 1, 1, 1, 0,  
        0, 0, 1, 0, 0,  
        0, 0, 1, 0, 0,  
        0, 1, 1, 0, 0,  
        0, 0, 1, 0, 0
    },   // Número 

    {
        1, 1, 1, 1, 1, 
        1, 0, 0, 0, 0,  
        1, 1, 1, 1, 1,  
        0, 0, 0, 0, 1,  
        1, 1, 1, 1, 1   
    }, // Número 2 

    {
        1, 1, 1, 1, 1,
        0, 0, 0, 0, 1,
        1, 1, 1, 1, 1,
        0, 0, 0, 0, 1,
        1, 1, 1, 1, 1  
    }, // Número 3

    {
        1, 0, 0, 0, 0,  
        0, 0, 0, 0, 1,  
        1, 1, 1, 1, 1,  
        1, 0, 0, 0, 1,  
        1, 0, 0, 0, 1   
    },  // Número 4

    {
        1, 1, 1, 1, 1,  
        0, 0, 0, 0, 1,  
        1, 1, 1, 1, 1,  
        1, 0, 0, 0, 0,  
        1, 1, 1, 1, 1   
    },  // Número 5

    {
        1, 1, 1, 1, 1,  
        1, 0, 0, 0, 1,  
        1, 1, 1, 1, 1,  
        1, 0, 0, 0, 0,  
        1, 1, 1, 1, 1   
    },  // Número 6

    {
        0, 0, 0, 0, 1, 
        0, 1, 0, 0, 0,  
        0, 0, 1, 0, 0,  
        0, 0, 0, 1, 0,  
        1, 1, 1, 1, 1
    },  // Número 7

    {
        1, 1, 1, 1, 1,  
        1, 0, 0, 0, 1, 
        1, 1, 1, 1, 1, 
        1, 0, 0, 0, 1,  
        1, 1, 1, 1, 1   
    },  // Número 8

    {
        1, 1, 1, 1, 1, 
        0, 0, 0, 0, 1, 
        1, 1, 1, 1, 1,  
        1, 0, 0, 0, 1,  
        1, 1, 1, 1, 1   
    }   // Número 9
};

// Função de callback para debounce
int64_t debounce_callback(alarm_id_t id, void *user_data) {
    uint gpio_num = (uint)(uintptr_t)user_data;
    if (!gpio_get(gpio_num)) {
        if (gpio_num == BUTTON_A) button_a_pressed = true;
        if (gpio_num == BUTTON_B) button_b_pressed = true;
    }
    return 0;
}

// Função única de callback para ambos os botões
void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == BUTTON_A || gpio == BUTTON_B) {
        static alarm_id_t debounce_alarm;
        cancel_alarm(debounce_alarm);
        debounce_alarm = add_alarm_in_ms(50, debounce_callback, (void*)(uintptr_t)gpio, true);
    }
}

// Timer para piscar LED vermelho
int64_t blink_callback(alarm_id_t id, void *user_data) {
    led_state = !led_state;
    gpio_put(LED_RED, led_state);
    return 100000; // 100ms para 5Hz
}

void display_number(int num, PIO pio, uint sm, uint32_t color) {
    num = (num % 10 + 10) % 10;
    
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 5; col++) {
            int index = row * 5 + col;
                
            uint32_t pixel_color = number_patterns[num][index] ? color : 0x000000;
            pio_sm_put_blocking(pio, sm, pixel_color << 8u); // Formato GRB
        }
    }
}

int main() {
    stdio_init_all();
    
    // Configuração do LED RGB
    gpio_init(LED_RED);
    gpio_init(LED_GREEN);
    gpio_init(LED_BLUE);
    gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_set_dir(LED_BLUE, GPIO_OUT);
    gpio_put(LED_GREEN, 0);
    gpio_put(LED_BLUE, 0);

    // Configuração correta dos botões
    gpio_init(BUTTON_A);
    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_pull_up(BUTTON_B);
    
    // Configuração única de interrupção para ambos os botões
    gpio_set_irq_enabled(BUTTON_A, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BUTTON_B, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_callback(gpio_callback);
    irq_set_enabled(IO_IRQ_BANK0, true);

    // Configuração da matriz de LEDs
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &bitdoglab_program);
    uint sm = pio_claim_unused_sm(pio, true);
    bitdoglab_program_init(pio, sm, offset, OUT_PIN);
    
    // Inicia o timer para piscar o LED
    add_alarm_in_ms(100, blink_callback, NULL, true);

    // Exibe o número inicial
    display_number(current_number, pio, sm, current_color);

    while (true) {
        if (button_a_pressed) {
            current_number = (current_number + 1) % 10;
            color_index = (color_index + 1) % 7;
            current_color = color_palette[color_index];
            display_number(current_number, pio, sm, current_color);
            button_a_pressed = false;
        }
        if (button_b_pressed) {
            current_number = (current_number - 1 + 10) % 10;
            color_index = (color_index - 1 + 7) % 7;
            current_color = color_palette[color_index];
            display_number(current_number, pio, sm, current_color);
            button_b_pressed = false;
        }
        
        tight_loop_contents();
    }
}
