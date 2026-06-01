// d:\Arduino\Projects\PCT_100\light.cpp
#include "light.h"

bool is_dark = false;
int light_adc_value = 0;

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
    return true;
  }
  return false;
}