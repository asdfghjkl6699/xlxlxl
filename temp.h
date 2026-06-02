// d:\Arduino\Projects\PCT_100\temp.h
#ifndef __TEMP_H
#define __TEMP_H

#include "Arduino.h"

#define TEMP_PIN         10          // 温度传感器引脚（DS18B20连接到IO10）
#define TEMP_THRESHOLD   32.0        // 温度阈值（高于此值风扇转动）
#define TEMP_INTERVAL    3000        // 采集间隔（毫秒）
#define CONVERSION_DELAY 750         // DS18B20转换时间（毫秒）

extern float current_temp;            // 当前温度值
extern bool is_hot;                   // 当前是否高温

void temp_init(void);                 // 初始化温度传感器
void temp_update(void);               // 更新温度（兼容旧接口）
void temp_update_nonblocking(void);   // 非阻塞更新温度

#endif