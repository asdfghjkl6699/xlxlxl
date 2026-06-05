/*
 * exti.cpp — 外部中断实现（第4课时核心）
 * =====================================
 *   exti_init()    绑定中断（调用 key_init 初始化引脚）
 *   exti_update()  状态机消抖（loop 中每帧调用）
 *
 *   KEY1 (CHANGE): 扳动 → 消抖 → 更新 key1_stable → 通知主程序
 *   KEY2 (RISING): 按下 → 消抖 → 确认 → 通知主程序 → 等松手
 */

#include "exti.h"

// ---- 状态机 ----
#define STATE_IDLE       0
#define STATE_DEBOUNCE   1
#define STATE_CONFIRMED  2

// KEY1
static uint8_t      k1_state = STATE_IDLE;
static unsigned long k1_time = 0;
static int          k1_stable = 0;        // 消抖后值 (下拉: LOW=弹起)
volatile int        key1_edge = 0;

// KEY2
static uint8_t      k2_state = STATE_IDLE;
static unsigned long k2_time = 0;
volatile int        key2_edge = 0;

volatile int relay_state = 0;

// ---- ISR 前向声明 ----
static void IRAM_ATTR k1_isr(void);
static void IRAM_ATTR k2_isr(void);

// ==================== 初始化 ====================
void exti_init(void)
{
    key_init();   // 初始化 KEY1/KEY2 引脚（key.cpp）

    attachInterrupt(digitalPinToInterrupt(KEY1_PIN), k1_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(KEY2_PIN), k2_isr, RISING);

    k1_stable = digitalRead(KEY1_PIN);
}

// ==================== ISR（PPT: 快进快出，只记时间） ====================

static void IRAM_ATTR k1_isr(void)
{
    if (k1_state != STATE_IDLE) return;
    k1_state = STATE_DEBOUNCE;
    k1_time  = millis();
}

static void IRAM_ATTR k2_isr(void)
{
    if (k2_state != STATE_IDLE) return;
    k2_state = STATE_DEBOUNCE;
    k2_time  = millis();
}

// ==================== 消抖（loop 每帧调用） ====================

void exti_update(void)
{
    unsigned long now = millis();

    // KEY1 (CHANGE): 消抖后确认电平变化
    if (k1_state == STATE_DEBOUNCE && (now - k1_time >= DEBOUNCE_MS)) {
        int raw = digitalRead(KEY1_PIN);
        if (raw != k1_stable) {
            k1_stable = raw;
            key1_edge = 1;                 // 通知主程序
        }
        k1_state = STATE_IDLE;
    }

    // KEY2 (RISING): 消抖后确认按下
    if (k2_state == STATE_DEBOUNCE && (now - k2_time >= DEBOUNCE_MS)) {
        if (digitalRead(KEY2_PIN) == HIGH) {
            key2_edge = 1;                 // 确认按下
            k2_state  = STATE_CONFIRMED;   // 等松手
        } else {
            k2_state = STATE_IDLE;         // 毛刺
        }
    }

    // KEY2 等松手后回到空闲
    if (k2_state == STATE_CONFIRMED && digitalRead(KEY2_PIN) == LOW) {
        k2_state = STATE_IDLE;
    }
}

// ==================== 对外接口 ====================

int key1_is_on(void)
{
    return (k1_stable == HIGH);            // 下拉: HIGH=导通
}
