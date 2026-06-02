// d:\Arduino\Projects\PCT_100\light.cpp
#include "light.h"

bool is_dark = false;
int light_adc_value = 0;
int light_intensity = 0;              // 光照强度（0-1000，0=最暗，1000=最亮）

static unsigned long last_light_time = 0;

void light_init(void) {
  // ADC引脚默认是输入模式，无需额外配置
}

bool light_update(void) {
  unsigned long now = millis();
  if (now - last_light_time >= LIGHT_INTERVAL) {
    last_light_time = now;
    light_adc_value = analogRead(LIGHT_ADC_PIN);
    is_dark = (light_adc_value > LIGHT_THRESHOLD);
    
    // 将ADC值（0-4095）转换为光照强度（0-1000）
    // 光照传感器特性：光线越暗，ADC值越大
    // 所以需要反转：ADC值越大，光照强度越小
    light_intensity = 1000 - (light_adc_value * 1000 / LIGHT_MAX_ADC);
    return true;
  }
  return false;
}