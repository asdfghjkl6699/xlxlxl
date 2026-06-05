#ifndef __RGB_LED_H
#define __RGB_LED_H

#include "Arduino.h"
#include <Adafruit_NeoPixel.h>

// RGB LED配置（WS2812）
#define RGB_LED_PIN     0          // RGB LED数据引脚（GPIO0）
#define RGB_LED_COUNT   1          // LED数量

// RGB颜色定义
typedef struct RGBColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGBColor;

// 预设颜色
#define RGB_RED     {255, 0, 0}
#define RGB_GREEN   {0, 255, 0}
#define RGB_BLUE    {0, 0, 255}
#define RGB_YELLOW  {255, 255, 0}
#define RGB_MAGENTA {255, 0, 255}
#define RGB_CYAN    {0, 255, 255}
#define RGB_WHITE   {255, 255, 255}
#define RGB_BLACK   {0, 0, 0}

extern Adafruit_NeoPixel rgb_led;

// 指示灯状态枚举
typedef enum {
    RGB_STATUS_NORMAL,           // 所有状态正常
    RGB_STATUS_WIFI_DISCONNECTED, // WiFi未连接
    RGB_STATUS_MQTT_DISCONNECTED, // MQTT未连接
    RGB_STATUS_ALARM_TEMP,       // 温度越界（风扇启动）
    RGB_STATUS_ALARM_LIGHT,      // 光照越界（LED灯亮）
    RGB_STATUS_SEVERE            // 严重警报（LED+风扇都启动）
} RGBStatus;

void rgb_led_init(void);              // 初始化RGB LED
void rgb_led_set_color(RGBColor color); // 设置LED颜色
void rgb_led_set_rgb(uint8_t r, uint8_t g, uint8_t b); // 设置RGB值
void rgb_led_rainbow(uint16_t duration); // 万色灯闪烁效果
void rgb_led_set_status(RGBStatus status); // 设置指示灯状态
void rgb_led_update(void);             // 更新指示灯显示（需在loop中调用）

#endif