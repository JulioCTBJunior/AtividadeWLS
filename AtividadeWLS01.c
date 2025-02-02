#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/irq.h"
#include "ws2812.pio.h"

#define NUM_LEDS 25
#define LED_PIN 7
#define RED_LED_PIN 13
#define BUTTON_A 5
#define BUTTON_B 6

volatile int current_number = -1;  // Começa com a matriz desligada
volatile bool red_led_state = false;

// Mapa para reorganizar os LEDs na matriz 
int led_map[25] = {
    20, 21, 22, 23, 24,
    19, 18, 17, 16, 15,
    10, 11, 12, 13, 14,
    9, 8, 7, 6, 5,
    0, 1, 2, 3, 4
};

// Função para criar cor RGB
uint32_t rgb_color(uint8_t r, uint8_t g, uint8_t b) {
    return ((g << 16) | (r << 8) | b);
};

// Representação dos números de 0 a 9 (corrigido)
const uint8_t numbers[10][25] = {
        {1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 0
        {0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0}, // 1
        {1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1}, // 2
        {1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 3
        {1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}, // 4
        {1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 5
        {1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 6
        {1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}, // 7
        {1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1}, // 8
        {1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1}
};
// Função para exibir o número na matriz de LEDs
void display_number(PIO pio, uint sm, int number) {
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t led_state = (number >= 0) ? numbers[number][led_map[i]] : 0;
        uint32_t color = led_state ? rgb_color(50, 50, 50) : rgb_color(0, 0, 0); // Branco suave
        pio_sm_put_blocking(pio, sm, color << 8);
    }
};

// Callback para alternar o LED vermelho
bool toggle_red_led(struct repeating_timer *t) {
    red_led_state = !red_led_state;
    gpio_put(RED_LED_PIN, red_led_state);
    return true;
}

// Debounce
volatile uint32_t last_press_a = 0;
volatile uint32_t last_press_b = 0;

// Callback para os botões
void button_callback(uint gpio, uint32_t events) {
    uint32_t now = to_ms_since_boot(get_absolute_time());

    if (gpio == BUTTON_A && (now - last_press_a > 200)) {
        current_number = (current_number + 1) % 10;
        last_press_a = now;
    }
    if (gpio == BUTTON_B && (now - last_press_b > 200)) {
        current_number = (current_number - 1 + 10) % 10;
        last_press_b = now;
    }
}

int main() {
    stdio_init_all();

    // Configura LED vermelho
    gpio_init(RED_LED_PIN);
    gpio_set_dir(RED_LED_PIN, GPIO_OUT);

    // Configura botões
    gpio_init(BUTTON_A);
    gpio_set_dir(BUTTON_A, GPIO_IN);
    gpio_pull_up(BUTTON_A);
    gpio_set_irq_enabled_with_callback(BUTTON_A, GPIO_IRQ_EDGE_FALL, true, &button_callback);

    gpio_init(BUTTON_B);
    gpio_set_dir(BUTTON_B, GPIO_IN);
    gpio_pull_up(BUTTON_B);
    gpio_set_irq_enabled_with_callback(BUTTON_B, GPIO_IRQ_EDGE_FALL, true, &button_callback);

    // Inicializa o PIO para controlar os LEDs WS2812
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, LED_PIN, 800000, false);

    // Timer para piscar o LED vermelho
    struct repeating_timer timer;
    add_repeating_timer_ms(-100, toggle_red_led, NULL, &timer);

    // Loop principal
    while (1) {
        display_number(pio, sm, current_number);
        sleep_ms(50);
    }
    return 0;
}