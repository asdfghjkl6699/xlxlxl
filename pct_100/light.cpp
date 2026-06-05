// d:\Arduino\Projects\PCT_100\light.cpp
#include "light.h"

// 外部引用运行时阈值变量
extern int g_light_threshold;

bool is_dark = false;
int light_adc_value = 0;
int light_intensity = 0;              // 光照强度（0-1000，0=最暗，1000=最亮）

static unsigned long last_light_time = 0;

// 消抖相关
#define DEBOUNCE_SAMPLES 3  // 需要连续3次检测确认状态变化
static int dark_count = 0;   // 检测到暗的次数
static int light_count = 0;  // 检测到亮的次数

void light_init(void) {
  // ADC引脚默认是输入模式，无需额外配置
}

bool light_update(void) {
  unsigned long now = millis();
  if (now - last_light_time >= LIGHT_INTERVAL) {
    last_light_time = now;
    light_adc_value = analogRead(LIGHT_ADC_PIN);
    
    // 将ADC值（0-4095）转换为光照强度（0-1000）
    // 光照传感器特性：光线越暗，ADC值越大
    // 所以需要反转：ADC值越大，光照强度越小
    light_intensity = 1000 - (light_adc_value * 1000 / LIGHT_MAX_ADC);
    
    // 消抖处理：需要连续DEBOUNCE_SAMPLES次检测到相同状态才更新
    bool current_dark = (light_adc_value > g_light_threshold);
    
    if (current_dark) {
      dark_count++;
      light_count = 0;
      if (dark_count >= DEBOUNCE_SAMPLES) {
        dark_count = DEBOUNCE_SAMPLES;  // 防止溢出
        if (!is_dark) {
          is_dark = true;
          return true;  // 状态变化，返回true
        }
      }
    } else {
      light_count++;
      dark_count = 0;
      if (light_count >= DEBOUNCE_SAMPLES) {
        light_count = DEBOUNCE_SAMPLES;  // 防止溢出
        if (is_dark) {
          is_dark = false;
          return true;  // 状态变化，返回true
        }
      }
    }
    
    return false;  // 状态未变化
  }
  return false;
}