/*
 * ws2812.cpp — WS2812单总线RGB LED驱动实现
 */

#include "ws2812.h"

uint32_t current_led_color = COLOR_BLACK;

// HSV转RGB
static uint32_t hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v) {
    uint8_t r, g, b;
    uint8_t region, remainder, p, q, t;
    
    if (s == 0) {
        r = g = b = v;
        return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
    
    region = h / 43;
    remainder = (h - (region * 43)) * 6;
    
    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    
    switch (region) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

// 初始化WS2812
void ws2812_init(void) {
    pinMode(WS2812_PIN, OUTPUT);
    digitalWrite(WS2812_PIN, LOW);
    current_led_color = COLOR_BLACK;
}

// 发送WS2812数据位 (使用精确延时实现)
static void ws2812_send_bit(bool bit) {
    // WS2812时序:
    // 逻辑1: T0H=0.35us, T0L=0.8us (低电平时间)
    // 逻辑0: T1H=0.7us, T1L=0.6us (高电平时间)
    if (bit) {
        // 逻辑1: 高电平约0.7us
        digitalWrite(WS2812_PIN, HIGH);
        __asm__ __volatile__ ("nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;");
        __asm__ __volatile__ ("nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;");
        digitalWrite(WS2812_PIN, LOW);
        // 低电平约0.6us
        __asm__ __volatile__ ("nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;");
    } else {
        // 逻辑0: 高电平约0.35us
        digitalWrite(WS2812_PIN, HIGH);
        __asm__ __volatile__ ("nop;nop;nop;nop;nop;nop;nop;nop;");
        digitalWrite(WS2812_PIN, LOW);
        // 低电平约0.8us
        __asm__ __volatile__ ("nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;");
        __asm__ __volatile__ ("nop;nop;nop;nop;nop;nop;");
    }
}

// 发送一个字节
static void ws2812_send_byte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        ws2812_send_bit((byte >> i) & 1);
    }
}

// 设置LED颜色 (RGB格式: 0xRRGGBB)
void ws2812_set_color(uint32_t rgb) {
    current_led_color = rgb;
    
    // WS2812B采用GRB顺序
    uint8_t green = (rgb >> 8) & 0xFF;
    uint8_t red = (rgb >> 16) & 0xFF;
    uint8_t blue = rgb & 0xFF;
    
    // 发送颜色数据
    noInterrupts();  // 禁用中断确保时序准确
    ws2812_send_byte(green);
    ws2812_send_byte(red);
    ws2812_send_byte(blue);
    interrupts();    // 恢复中断
    
    // 发送复位信号 (至少280us低电平)
    delayMicroseconds(300);
}

// 关闭LED
void ws2812_off(void) {
    ws2812_set_color(COLOR_BLACK);
}

// 万色灯循环
void ws2812_rainbow_cycle(int cycles) {
    for (int cycle = 0; cycle < cycles; cycle++) {
        for (uint16_t hue = 0; hue < 256; hue++) {
            ws2812_set_color(hsv_to_rgb(hue, 255, 128));
            delay(10);
        }
    }
}

// WiFi状态指示
void ws2812_wifi_indicator(bool connected) {
    static unsigned long last_blink_time = 0;
    static bool led_state = false;
    
    if (connected) {
        // WiFi已连接：绿色常亮
        ws2812_set_color(COLOR_GREEN);
    } else {
        // WiFi未连接：红色闪烁
        unsigned long now = millis();
        if (now - last_blink_time >= 500) {
            last_blink_time = now;
            led_state = !led_state;
            ws2812_set_color(led_state ? COLOR_RED : COLOR_BLACK);
        }
    }
}