/*
 * ws2812.h — 单总线RGB LED驱动模块
 * ===================================
 *   控制WS2812B等单总线RGB LED
 *   支持万色灯闪烁和状态指示
 */

#ifndef __WS2812_H
#define __WS2812_H

#include "Arduino.h"

// WS2812B LED数据引脚
#define WS2812_PIN     8         // GPIO8 → WS2812 DATA

// RGB颜色定义
#define COLOR_RED     0xFF0000
#define COLOR_GREEN   0x00FF00
#define COLOR_BLUE    0x0000FF
#define COLOR_YELLOW  0xFFFF00
#define COLOR_CYAN    0x00FFFF
#define COLOR_MAGENTA 0xFF00FF
#define COLOR_WHITE   0xFFFFFF
#define COLOR_BLACK   0x000000

extern uint32_t current_led_color;  // 当前LED颜色

void ws2812_init(void);             // 初始化WS2812
void ws2812_set_color(uint32_t rgb); // 设置LED颜色 (0xRRGGBB)
void ws2812_off(void);              // 关闭LED
void ws2812_rainbow_cycle(int cycles); // 万色灯循环
void ws2812_wifi_indicator(bool connected); // WiFi状态指示

#endif