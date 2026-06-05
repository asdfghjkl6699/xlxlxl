/*
 * key.cpp — 按键引脚初始化
 */

#include "key.h"

void key_init(void)
{
    pinMode(KEY1_PIN, INPUT_PULLDOWN);   // 下拉: 按下导通→HIGH
    pinMode(KEY2_PIN, INPUT_PULLDOWN);   // 下拉: 按下→HIGH (PPT Slide 15)
}
