/*
 * relay.h — 输出控制模块
 * =======================
 *   灯  (GPIO6) → USB2 口供电
 *   风扇(GPIO7) → USB3 口供电
 */

#ifndef __RELAY_H
#define __RELAY_H

#include "Arduino.h"

#define LIGHT_PIN     6         // USB2 → 灯
#define FAN_PIN       7         // USB3 → 风扇

void relay_init(void);           // 初始化输出引脚
void relay_on(void);             // 灯亮 + 风扇转
void relay_off(void);            // 灯灭 + 风扇停

#endif
