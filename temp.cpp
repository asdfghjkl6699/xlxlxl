// d:\Arduino\Projects\PCT_100\temp.cpp
#include "temp.h"
#include <OneWire.h>
#include <DallasTemperature.h>

float current_temp = 25.0;
bool is_hot = false;

static OneWire oneWire(TEMP_PIN);
static DallasTemperature sensors(&oneWire);
static unsigned long last_temp_time = 0;

void temp_init(void) {
  sensors.begin();
}

bool temp_update(void) {
  unsigned long now = millis();
  if (now - last_temp_time >= TEMP_INTERVAL) {
    last_temp_time = now;
    sensors.requestTemperatures();
    current_temp = sensors.getTempCByIndex(0);
    is_hot = (current_temp > TEMP_THRESHOLD);
    return true;
  }
  return false;
}