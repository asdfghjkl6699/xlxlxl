/*
 * key.h — 按键引脚定义与初始化
 * ==============================
 *   第4课时 Slide 15-16 基础
 *   KEY1: GPIO20, 自锁开关, 下拉输入
 *   KEY2: GPIO21, 轻触按键, 下拉输入
 */

#ifndef __KEY_H
#define __KEY_H

#include "Arduino.h"

#define KEY1_PIN      20        // 自锁开关 → GPIO20
#define KEY2_PIN      21        // 轻触按键 → GPIO21
#define DEBOUNCE_MS   50

void key_init(void);            // 初始化两个按键引脚

#endif
