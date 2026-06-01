#include "exti.h"
#include "relay.h"

enum State { S00, S10, S01, S11 };
enum State current_state = S00;
int step = 0;

// ===================== 光照采集配置 =====================
const int adcPin = 1;           // ADC 引脚（根据电路图，ADC 连接到 IO17）
const float referenceVoltage = 3.3;  // ADC 参考电压（ESP32-C3 默认使用 3.3V）
const int adcMaxValue = 4095;    // ADC 分辨率（12 位，范围 0 ~ 4095）
const int lightThreshold = 2000; // 光照阈值（低于此值认为是黑暗，需要开灯）
// ========================================================

// 长按配置
unsigned long key2_down_time = 0;
bool key2_holding = false;
const unsigned int LONG_PRESS_MIN = 1000;  // 1秒
const unsigned int LONG_PRESS_MAX = 2000;  // 2秒

// ===================== 核心：模式切换标志 =====================
bool is_auto_mode = true;  // 默认：自动模式
// ============================================================

// ===================== 非阻塞定时器配置 =====================
unsigned long last_adc_time = 0;  // 上次ADC采集时间
const unsigned long adc_interval = 500;  // 采集间隔（毫秒）

// 设置继电器
void setRelay(enum State s) {
  switch (s) {
    case S00: digitalWrite(6, LOW);  digitalWrite(7, LOW);  break;
    case S10: digitalWrite(6, HIGH); digitalWrite(7, LOW);  break;
    case S01: digitalWrite(6, LOW);  digitalWrite(7, HIGH); break;
    case S11: digitalWrite(6, HIGH); digitalWrite(7, HIGH); break;
  }
}

void setup() {
  Serial.begin(115200);
  exti_init();
  relay_init();
  relay_off();
  setRelay(S00);
  Serial.println("系统初始化完成，默认：自动模式");
  Serial.println("ADC 分辨率: 12 位 (0 ~ 4095)");
}

void loop() {
  exti_update();

  // ===================== KEY1 总开关 =====================
  if (key1_edge) {
    key1_edge = 0;
    if (!key1_is_on()) {
      // KEY1 断开 → 全部关闭
      is_auto_mode = true;
      current_state = S00;
      setRelay(S00);
      step = 0;
      key2_holding = false;
      Serial.println("KEY1 断开 → 全部关闭，复位到自动模式");
    } else {
      // KEY1 闭合 → 上电
      Serial.println("KEY1 闭合 → 系统已上电");
    }
  }

  // KEY1 没开，直接跳过所有操作
  if (!key1_is_on()) {
    key2_holding = false;
    return;
  }

  // ===================== 非阻塞光照采集（始终运行） =====================
  unsigned long now = millis();
  if (now - last_adc_time >= adc_interval) {
    last_adc_time = now;
    
    int adcValue = analogRead(adcPin);
    float voltage = (adcValue * referenceVoltage) / adcMaxValue;
    bool is_dark = (adcValue > lightThreshold);  // 光照值越低越暗
    
    Serial.print("ADC值: ");
    Serial.print(adcValue);
    Serial.print("\t电压: ");
    Serial.print(voltage, 3);
    Serial.print(" V\t模式: ");
    Serial.print(is_auto_mode ? "自动" : "手动");
    Serial.print("\t状态: ");
    Serial.println(is_dark ? "暗" : "亮");

    // ===================== 自动模式：光照控制灯 =====================
    if (is_auto_mode) {
      if (is_dark && current_state != S10) {
        // 黑暗且灯未亮 → 开灯（S10: 灯亮，风扇停）
        current_state = S10;
        setRelay(current_state);
        Serial.println("自动模式 → 检测到黑暗，自动开灯");
      } else if (!is_dark && current_state != S00) {
        // 明亮且灯未灭 → 关灯（S00: 灯灭，风扇停）
        current_state = S00;
        setRelay(current_state);
        Serial.println("自动模式 → 检测到明亮，自动关灯");
      }
    }
  }

  // ===================== KEY2 按下：开始计时 =====================
  if (key2_edge) {
    key2_edge = 0;
    key2_down_time = millis();
    key2_holding = true;
  }

  // ===================== 长按 KEY2：切换自动/手动模式 =====================
  if (key2_holding) {
    unsigned long hold_time = millis() - key2_down_time;
    
    // 长按 1~2 秒：切换模式（自动 ↔ 手动）
    if (hold_time >= LONG_PRESS_MIN && hold_time <= LONG_PRESS_MAX) {
      is_auto_mode = !is_auto_mode;  // 取反：切换模式
      key2_holding = false;          // 防止重复触发

      if (is_auto_mode) {
        current_state = S00;
        setRelay(S00);
        step = 0;
        Serial.println("===== 切换为：自动模式 =====");
      } else {
        
        Serial.println("===== 切换为：手动模式 =====");
      }
    }

    
  }

  // ===================== KEY2 松手：短按（仅手动模式有效） =====================
  static bool last_key2 = false;
  bool now_key2 = (digitalRead(KEY2_PIN) == HIGH);
  
  // 检测松手
  if (last_key2 && !now_key2 && key2_holding) {
    unsigned long hold = millis() - key2_down_time;

    // 短按（小于1秒）
    if (hold < LONG_PRESS_MIN) {
      if (!is_auto_mode) {  // 只有手动模式才响应
        step++;
        switch (step) {
          case 1: current_state = S10; break;  // 亮 停
          case 2: current_state = S00; break;  // 灭 停
          case 3: current_state = S11; break;  // 亮 转
          case 4: current_state = S01; step = 0; break;  // 灭 转
        }
        setRelay(current_state);
        Serial.print("手动模式 → 短按 step: "); Serial.println(step);
      } else {
        Serial.println("自动模式 → 短按无效");
      }
    }
    key2_holding = false;
  }
  last_key2 = now_key2;
}