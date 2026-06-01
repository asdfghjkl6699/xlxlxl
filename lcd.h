// d:\Arduino\Projects\PCT_100\lcd.h
#ifndef __LCD_H
#define __LCD_H

#include "Arduino.h"

void lcd_init(void);                    // 初始化LCD
void lcd_update(bool is_auto, bool main_on, int light_val, float temp_val, bool led_on, bool fan_on);

#endif