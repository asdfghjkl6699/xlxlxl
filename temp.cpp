// d:\Arduino\Projects\PCT_100\temp.cpp
#include "temp.h"
#include <OneWire.h>
#include <DallasTemperature.h>

float current_temp = 25.0;
bool is_hot = false;

static OneWire oneWire(TEMP_PIN);
static DallasTemperature sensors(&oneWire);
static unsigned long last_temp_time = 0;
static unsigned long conversion_start_time = 0;
static bool is_converting = false;

void temp_init(void) {
  sensors.begin();
}

void temp_update_nonblocking(void) {
  unsigned long now = millis();
  
  // 检查是否需要开始新的采集周期
  if (!is_converting && (now - last_temp_time >= TEMP_INTERVAL)) {
    sensors.requestTemperatures();  // 开始温度转换（非阻塞）
    conversion_start_time = now;
    is_converting = true;
    return;
  }
  
  // 等待转换完成（约750ms）
  if (is_converting && (now - conversion_start_time >= CONVERSION_DELAY)) {
    current_temp = sensors.getTempCByIndex(0);  // 读取温度
    is_hot = (current_temp > TEMP_THRESHOLD);
    is_converting = false;
    last_temp_time = now;
  }
}

// 兼容旧接口
void temp_update(void) {
  temp_update_nonblocking();
}