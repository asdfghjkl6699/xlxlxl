#include "exti.h"
#include "relay.h"
#include "light.h"
#include "temp.h"
#include "lcd.h"

enum State { S00, S01, S10, S11 };
enum State current_state = S00;
int step = 0;

// 长按配置
unsigned long key2_down_time = 0;
bool key2_holding = false;
const unsigned int LONG_PRESS_MIN = 1000;  // 1秒
const unsigned int LONG_PRESS_MAX = 2000;  // 2秒

// 模式切换标志
bool is_auto_mode = true;  

// 定时时间戳（分离各个任务周期，互不干扰）
static unsigned long last_sensor_time = 0;
static unsigned long last_display_time = 0;
const unsigned int SENSOR_INTERVAL = 50;    // 传感器50ms更新一次（加快ADC读取频率）
const unsigned int DISPLAY_INTERVAL = 2000;  // 显示+串口2s更新一次

// 设置继电器
void setRelay(enum State s) {
  switch (s) {
    case S00: digitalWrite(6, LOW);  digitalWrite(7, LOW);  break;
    case S01: digitalWrite(6, LOW);  digitalWrite(7, HIGH); break;
    case S10: digitalWrite(6, HIGH); digitalWrite(7, LOW);  break;
    case S11: digitalWrite(6, HIGH); digitalWrite(7, HIGH); break;
  }
}

void setup() {
  Serial.begin(115200);
  exti_init();
  relay_init();
  relay_off();
  setRelay(S00);
  light_init();
  temp_init();
  lcd_init();
  Serial.println("系统初始化完成，默认：自动模式");
  Serial.println("ADC 分辨率: 12 位 (0 ~ 4095)");
}

void loop() {
  // ===================== 【最高优先级】按键逻辑 优先执行 =====================
  exti_update();

  // -------- KEY1 总开关 --------
  if (key1_edge) {
    key1_edge = 0;
    if (!key1_is_on()) {
      is_auto_mode = true;
      current_state = S00;
      setRelay(S00);
      step = 0;
      key2_holding = false;
      Serial.println("KEY1 断开 → 全部关闭，复位到自动模式");
    } else {
      Serial.println("KEY1 闭合 → 系统已上电");
    }
  }

  // 主开关关闭：仅刷新屏幕，直接返回，不再执行后续逻辑
  if (!key1_is_on()) {
    key2_holding = false;
    int light_thresh_display = 1000 - (LIGHT_THRESHOLD * 1000 / LIGHT_MAX_ADC);
    lcd_update(is_auto_mode, false, light_intensity, light_thresh_display, current_temp, TEMP_THRESHOLD, false, false);
    return;
  }

  // -------- KEY2 按键（短按/长按）全程优先处理 --------
  static bool key2_processed = false;
  static bool last_key2_state = false;
  bool key2_state = (digitalRead(KEY2_PIN) == HIGH);

  // KEY2 按下触发
  if (key2_edge) {
    key2_edge = 0;
    key2_down_time = millis();
    key2_holding = true;
    key2_processed = false;
  }

  // KEY2 长按检测
  if (key2_holding && !key2_processed) {
    unsigned long hold_time = millis() - key2_down_time;
    if (hold_time >= LONG_PRESS_MIN && hold_time <= LONG_PRESS_MAX) {
      is_auto_mode = !is_auto_mode;
      key2_processed = true;

      if (is_auto_mode) {
        step = 0;
        if (is_dark && is_hot)      current_state = S11;
        else if (is_dark && !is_hot) current_state = S10;
        else if (!is_dark && is_hot) current_state = S01;
        else                         current_state = S00;
        setRelay(current_state);
        Serial.println("===== 切换为：自动模式 =====");
      } else {
        Serial.println("===== 切换为：手动模式 =====");
      }
    }
  }

  // KEY2 松手检测 + 短按逻辑
  if (last_key2_state && !key2_state && key2_holding) {
    unsigned long hold_time = millis() - key2_down_time;
    if (hold_time < LONG_PRESS_MIN && !key2_processed) {
      if (!is_auto_mode) {
        step++;
        switch (step) {
          case 1: current_state = S10; break;
          case 2: current_state = S00; break;
          case 3: current_state = S11; break;
          case 4: current_state = S01; step = 0; break;
        }
        setRelay(current_state);
        Serial.print("手动模式 → 短按 step: ");
        Serial.println(step);
        // 手动模式每按一下按键输出一次灯和风扇状态
        bool led_on  = (current_state == S10 || current_state == S11);
        bool fan_on  = (current_state == S01 || current_state == S11);
        Serial.print("模式: 手动\t灯: ");
        Serial.print(led_on ? "亮" : "灭");
        Serial.print("\t风扇: ");
        Serial.println(fan_on ? "开" : "关");
      } else {
        Serial.println("自动模式 → 短按无效");
      }
    }
    key2_holding = false;
    key2_processed = false;
  }
  last_key2_state = key2_state;
  // ===================== 按键逻辑结束 =====================


  // ===================== 【次优先级】传感器采集（定时非阻塞） =====================
  // 手动模式下不采集ADC，避免影响按键功能
  if (is_auto_mode && millis() - last_sensor_time >= SENSOR_INTERVAL) {
    last_sensor_time = millis();
    temp_update();
    light_update();

    // 自动模式逻辑（跟随传感器更新，响应及时）
    if (is_dark && is_hot && current_state != S11) {
      current_state = S11;
      setRelay(current_state);
    } else if (is_dark && !is_hot && current_state != S10) {
      current_state = S10;
      setRelay(current_state);
    } else if (!is_dark && is_hot && current_state != S01) {
      current_state = S01;
      setRelay(current_state);
    } else if (!is_dark && !is_hot && current_state != S00) {
      current_state = S00;
      setRelay(current_state);
    }
  }


  // ===================== 【最低优先级】LCD + 串口输出（低频定时，不抢占按键） =====================
  if (millis() - last_display_time >= DISPLAY_INTERVAL) {
    last_display_time = millis();

    bool led_on  = (current_state == S10 || current_state == S11);
    bool fan_on  = (current_state == S01 || current_state == S11);

    int light_thresh_display = 1000 - (LIGHT_THRESHOLD * 1000 / LIGHT_MAX_ADC);
    lcd_update(is_auto_mode, key1_is_on(), light_intensity, light_thresh_display, current_temp, TEMP_THRESHOLD, led_on, fan_on);

    if (is_auto_mode) {
      // 自动模式定时输出完整信息
      Serial.print("光照: ");
      Serial.print(light_intensity);
      Serial.print("/1000\t温度: ");
      Serial.print(current_temp, 1);
      Serial.print(" C\t模式: ");
      Serial.print("自动");
      Serial.print("\t灯: ");
      Serial.print(led_on ? "亮" : "灭");
      Serial.print("\t风扇: ");
      Serial.println(fan_on ? "开" : "关");
    }
    // 手动模式的串口输出在按键处理中完成
  }
}