/*
 * exti.h — 外部中断模块
 * =====================
 *   KEY1 → CHANGE 中断（自锁开关扳动检测）
 *   KEY2 → RISING 中断（轻触按键按下检测）
 *
 *   PPT Slide 14 注意事项：
 *     ① ISR 尽量短  ② 不用 delay，用 millis()
 *     ③ 共享变量加 volatile  ④ ISR 中不用 Serial.print
 */

#ifndef __EXTI_H
#define __EXTI_H

#include "Arduino.h"
#include "key.h"

// ---- 全局状态（ISR ↔ loop 共享，volatile） ----
extern volatile int key1_edge;    // KEY1 发生电平变化
extern volatile int key2_edge;    // KEY2 被按下
extern volatile int relay_state;  // 继电器/输出状态: 0=关, 1=开

// ---- 函数 ----
void exti_init(void);             // attachInterrupt 绑定两个按键
void exti_update(void);           // loop 中每帧调用：消抖 + 确认
int  key1_is_on(void);            // KEY1 消抖后状态: 1=导通, 0=断开

#endif
