#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "include/ssd1306.h"

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define SSD1306_ADDRESS 0x3C

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

#define MAX_BALLS 10
#define PIN_ROWS 5
#define BALL_RADIUS 2
#define BIN_COUNT 6
#define BIN_WIDTH (DISPLAY_WIDTH / BIN_COUNT)
#define HISTOGRAM_HEIGHT 20
#define NEW_BALL_INTERVAL 10

uint8_t display_buffer[DISPLAY_WIDTH * DISPLAY_HEIGHT / 8];

typedef struct {
    int x;
    int y;
    bool active;
} Ball;

Ball balls[MAX_BALLS];

int bins[BIN_COUNT] = {0};

int total_balls = 0;

void draw_ball(uint8_t *buffer, int x, int y, int radius) {
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx*dx + dy*dy <= radius*radius) {
                if (x+dx >= 0 && x+dx < DISPLAY_WIDTH && y+dy >= 0 && y+dy < DISPLAY_HEIGHT) {
                    ssd1306_set_pixel(buffer, x+dx, y+dy, true);
                }
            }
        }
    }
}

void clear_buffer(uint8_t *buffer) {
    for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT / 8; i++) {
        buffer[i] = 0;
    }
}

void init_ball(Ball *ball) {
    ball->x = DISPLAY_WIDTH / 2;
    ball->y = 0;
    ball->active = true;
}

int random_direction() {
    return (rand() % 2) ? -1 : 1;
}

void draw_pins(uint8_t *buffer) {
    int pin_spacing = 16;
    
    for (int row = 0; row < PIN_ROWS; row++) {
        int pins_in_row = row + 1;
        
        int start_x = (DISPLAY_WIDTH - (pins_in_row - 1) * pin_spacing) / 2;
        
        for (int col = 0; col < pins_in_row; col++) {
            int x = start_x + col * pin_spacing;
            int y = (row + 1) * 8;
            draw_ball(buffer, x, y, 1);
        }
    }
}

void update_ball(Ball *ball) {
    if (!ball->active) return;

    int pin_row_y = ((ball->y + 4) / 8) * 8;
    
    if (abs(ball->y - pin_row_y) < 2 && ball->y < PIN_ROWS * 8) {
        int row = pin_row_y / 8 - 1;
        if (row >= 0 && row < PIN_ROWS) {
            int pins_in_row = row + 1;
            int pin_spacing = 16;
            int start_x = (DISPLAY_WIDTH - (pins_in_row - 1) * pin_spacing) / 2;
            
            ball->x += random_direction() * 4;
            
            if (ball->x < BALL_RADIUS) ball->x = BALL_RADIUS;
            if (ball->x >= DISPLAY_WIDTH - BALL_RADIUS) ball->x = DISPLAY_WIDTH - BALL_RADIUS - 1;
        }
    }

    ball->y += 1;
    if (ball->y >= DISPLAY_HEIGHT - HISTOGRAM_HEIGHT - BALL_RADIUS) {
        int bin = ball->x / BIN_WIDTH;
        if (bin < 0) bin = 0;
        if (bin >= BIN_COUNT) bin = BIN_COUNT - 1;
        
        bins[bin]++;
        
        total_balls++;
        
        ball->active = false;
    }
}

void draw_histogram(uint8_t *buffer) {
    int max_count = 1;  // Evita divisão por zero
    
    for (int i = 0; i < BIN_COUNT; i++) {
        if (bins[i] > max_count) max_count = bins[i];
    }
    
    for (int i = 0; i < BIN_COUNT; i++) {
        int height = (bins[i] * HISTOGRAM_HEIGHT) / max_count;
        if (height > 0) {
            for (int h = 0; h < height; h++) {
                for (int w = 0; w < BIN_WIDTH-1; w++) {
                    int x = i * BIN_WIDTH + w;
                    int y = DISPLAY_HEIGHT - h - 1;
                    ssd1306_set_pixel(buffer, x, y, true);
                }
            }
        }
    }
}

void draw_counter(uint8_t *buffer) {
    char counter_text[20];
    snprintf(counter_text, sizeof(counter_text), "%d", total_balls);
    ssd1306_draw_string(buffer, 0, 0, counter_text);
}

void draw_divider(uint8_t *buffer) {
    for (int x = 0; x < DISPLAY_WIDTH; x++) {
        ssd1306_set_pixel(buffer, x, DISPLAY_HEIGHT - HISTOGRAM_HEIGHT - 1, true);
    }
}

int main() {
    stdio_init_all();

    srand(time_us_64());

    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    ssd1306_init();
    
    clear_buffer(display_buffer);

    for (int i = 0; i < MAX_BALLS; i++) {
        balls[i].active = false;
    }
    struct render_area area = {
        .start_column = 0,
        .end_column = DISPLAY_WIDTH - 1,
        .start_page = 0,
        .end_page = DISPLAY_HEIGHT / 8 - 1
    };
    calculate_render_area_buffer_length(&area);

    int tick_counter = 0;
    clear_buffer(display_buffer);
    ssd1306_draw_string(display_buffer, 10, 20, "GALTON BOARD");
    ssd1306_draw_string(display_buffer, 20, 35, "SIMULATION");
    render_on_display(display_buffer, &area);
    sleep_ms(2000);

    while (true) {
        clear_buffer(display_buffer);
        tick_counter++;
        if (tick_counter >= NEW_BALL_INTERVAL) {
            for (int i = 0; i < MAX_BALLS; i++) {
                if (!balls[i].active) {
                    init_ball(&balls[i]);
                    tick_counter = 0;
                    break;
                }
            }
        }
        
        draw_pins(display_buffer);
        draw_divider(display_buffer);
        
        for (int i = 0; i < MAX_BALLS; i++) {
            if (balls[i].active) {
                update_ball(&balls[i]);
                draw_ball(display_buffer, balls[i].x, balls[i].y, BALL_RADIUS);
            }
        }
        
        draw_histogram(display_buffer);
        draw_counter(display_buffer);
        render_on_display(display_buffer, &area);
        sleep_ms(50);
    }
}