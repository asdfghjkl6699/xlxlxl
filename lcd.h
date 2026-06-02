// d:\Arduino\Projects\PCT_100\lcd.h
#ifndef __LCD_H
#define __LCD_H

#include "Arduino.h"

// OLED 引脚定义
#define OLED_SCL_PIN    5         // I2C SCL 引脚
#define OLED_SDA_PIN    4         // I2C SDA 引脚

void lcd_init(void);                    // 初始化LCD
void lcd_update(bool is_auto, bool main_on, int light_val, int light_threshold, float temp_val, float temp_threshold, bool led_on, bool fan_on);

#endif