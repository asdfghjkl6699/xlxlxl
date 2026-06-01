// d:\Arduino\Projects\PCT_100\light.h
#ifndef __LIGHT_H
#define __LIGHT_H

#include "Arduino.h"

// 光照采集配置
#define LIGHT_ADC_PIN     1           // 光照ADC引脚（连接到IO17）
#define LIGHT_THRESHOLD   2000        // 光照阈值（低于此值认为是黑暗）
#define LIGHT_INTERVAL    2000        // 采集间隔（毫秒）

extern bool is_dark;                  // 当前是否黑暗
extern int light_adc_value;           // 当前光照ADC值

void light_init(void);                // 初始化光照采集
bool light_update(void);              // 更新光照状态

#endif