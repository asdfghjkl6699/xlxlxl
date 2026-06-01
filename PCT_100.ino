#include "exti.h"
#include "relay.h"
#include <OneWire.h>
#include <DallasTemperature.h>

enum State { S00, S01, S10, S11 };
enum State current_state = S00;
int step = 0;

// ===================== 光照采集配置 =====================
const int lightAdcPin = 1;      // 光照ADC引脚（根据电路图，ADC连接到IO17，ADC2通道6）
const float referenceVoltage = 3.3;  // ADC 参考电压（ESP32-C3 默认使用 3.3V）
const int adcMaxValue = 4095;    // ADC 分辨率（12 位，范围 0 ~ 4095）
const int lightThreshold = 2000; // 光照阈值（低于此值认为是黑暗，需要开灯）
// =========================================================

// ===================== 温度采集配置 =====================
const int tempPin = 10;          // 温度传感器引脚（DS18B20连接到IO10）
const float tempThreshold = 32.0; // 温度阈值（高于此值风扇转动，单位：摄氏度）

OneWire oneWire(tempPin);        // 初始化OneWire总线
DallasTemperature sensors(&oneWire); // 初始化温度传感器
// =========================================================

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
const unsigned long adc_interval = 2000;  // 光照采集间隔（毫秒）
unsigned long last_temp_time = 0;  // 上次温度采集时间
const unsigned long temp_interval = 3000;  // 温度采集间隔（毫秒）
float current_temp = 25.0;         // 当前温度缓存

// 设置继电器
void setRelay(enum State s) {
  switch (s) {
    case S00: digitalWrite(6, LOW);  digitalWrite(7, LOW);  break;
    case S01: digitalWrite(6, LOW); digitalWrite(7, HIGH);  break;
    case S10: digitalWrite(6, HIGH);  digitalWrite(7, LOW); break;
    case S11: digitalWrite(6, HIGH); digitalWrite(7, HIGH); break;
  }
}

void setup() {
  Serial.begin(115200);
  exti_init();
  relay_init();
  relay_off();
  setRelay(S00);
  sensors.begin();               // 初始化温度传感器
  Serial.println("系统初始化完成，默认：自动模式");
  Serial.println("ADC 分辨率: 12 位 (0 ~ 4095)");
}

void loop() {
  // 优先处理按键中断（非阻塞）
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

  unsigned long now = millis();

  // ===================== 非阻塞温度采集（独立进行，减少阻塞影响） =====================
  if (now - last_temp_time >= temp_interval) {
    last_temp_time = now;
    // 采集温度（DS18B20数字传感器，会阻塞约750ms）
    sensors.requestTemperatures();
    current_temp = sensors.getTempCByIndex(0);
  }

  // ===================== 非阻塞光照采集（始终运行） =====================
  if (now - last_adc_time >= adc_interval) {
    last_adc_time = now;
    
    // 采集光照
    int lightAdcValue = analogRead(lightAdcPin);
    float lightVoltage = (lightAdcValue * referenceVoltage) / adcMaxValue;
    bool is_dark = (lightAdcValue > lightThreshold);  // 光照值越低越暗
    
    // 使用缓存的温度值，避免每次都阻塞
    bool is_hot = (current_temp > tempThreshold);
    
    Serial.print("光照ADC: ");
    Serial.print(lightAdcValue);
    Serial.print("\t温度: ");
    Serial.print(current_temp, 1);
    Serial.print(" C\t模式: ");
    Serial.print(is_auto_mode ? "自动" : "手动");
    Serial.print("\t灯: ");
    Serial.print((current_state == S10 || current_state == S11) ? "亮" : "灭");
    Serial.print("\t风扇: ");
    Serial.println((current_state == S01 || current_state == S11) ? "开" : "关");

    // ===================== 自动模式：光照控制灯，温度控制风扇 =====================
    if (is_auto_mode) {
      // 根据温度和光照组合设置状态
      if (is_dark && is_hot && current_state != S11) {
        // 黑暗且温度高 → 灯亮，风扇转（S11）
        current_state = S11;
        setRelay(current_state);
        Serial.println("自动模式 → 黑暗+高温，开灯+开风扇");
      } else if (is_dark && !is_hot && current_state != S10) {
        // 黑暗且温度正常 → 灯亮，风扇停（S10）
        current_state = S10;
        setRelay(current_state);
        Serial.println("自动模式 → 黑暗+常温，开灯");
      } else if (!is_dark && is_hot && current_state != S01) {
        // 明亮且温度高 → 灯灭，风扇转（S01）
        current_state = S01;
        setRelay(current_state);
        Serial.println("自动模式 → 明亮+高温，开风扇");
      } else if (!is_dark && !is_hot && current_state != S00) {
        // 明亮且温度正常 → 灯灭，风扇停（S00）
        current_state = S00;
        setRelay(current_state);
        Serial.println("自动模式 → 明亮+常温，全部关闭");
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
          case 1: current_state = S00; break;  // 灭 停
          case 2: current_state = S01; break;  // 灭 转
          case 3: current_state = S10; break;  // 亮 停
          case 4: current_state = S11; step = 0; break;  // 亮 转
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