#include "rgb_led.h"

// 创建NeoPixel对象
Adafruit_NeoPixel rgb_led(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// 当前状态
static RGBStatus s_current_status = RGB_STATUS_NORMAL;
static unsigned long s_last_time = 0;
static int s_step = 0;

// 亮度控制（0-255）
#define RGB_BRIGHTNESS   100  // 降低亮度到100（默认255）

void rgb_led_init(void) {
    rgb_led.begin();
    rgb_led.show(); // 初始关闭（全黑）
    s_last_time = millis();
}

void rgb_led_set_color(RGBColor color) {
    // 应用亮度控制
    uint8_t r = (color.r * RGB_BRIGHTNESS) / 255;
    uint8_t g = (color.g * RGB_BRIGHTNESS) / 255;
    uint8_t b = (color.b * RGB_BRIGHTNESS) / 255;
    rgb_led.setPixelColor(0, rgb_led.Color(r, g, b));
    rgb_led.show();
}

void rgb_led_set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    // 应用亮度控制
    r = (r * RGB_BRIGHTNESS) / 255;
    g = (g * RGB_BRIGHTNESS) / 255;
    b = (b * RGB_BRIGHTNESS) / 255;
    rgb_led.setPixelColor(0, rgb_led.Color(r, g, b));
    rgb_led.show();
}

void rgb_led_rainbow(uint16_t duration) {
    unsigned long start_time = millis();
    uint16_t hue = 0;
    
    while (millis() - start_time < duration) {
        // 使用HSV转RGB，产生彩虹效果
        uint32_t color = rgb_led.ColorHSV(hue * 65536 / 256);
        rgb_led.setPixelColor(0, color);
        rgb_led.show();
        
        hue = (hue + 2) % 256;
        delay(10);
    }
    
    rgb_led_set_rgb(0, 0, 0); // 结束后关闭
}

void rgb_led_set_status(RGBStatus status) {
    if (s_current_status != status) {
        s_current_status = status;
        s_step = 0;
        s_last_time = millis();
    }
}

void rgb_led_update(void) {
    unsigned long now = millis();
    
    switch (s_current_status) {
        case RGB_STATUS_SEVERE: {
            // 严重警报：红灯100→灭50→红灯100→灭50→绿灯100→灭50→绿灯100→灭50→蓝灯100→灭50→蓝灯100→灭350
            static const int steps[] = {100, 50, 100, 50, 100, 50, 100, 50, 100, 50, 100, 350};
            static const int colors[][3] = {
                {255,0,0}, {0,0,0}, {255,0,0}, {0,0,0},
                {0,255,0}, {0,0,0}, {0,255,0}, {0,0,0},
                {0,0,255}, {0,0,0}, {0,0,255}, {0,0,0}
            };
            if (now - s_last_time >= steps[s_step]) {
                rgb_led_set_rgb(colors[s_step][0], colors[s_step][1], colors[s_step][2]);
                s_step = (s_step + 1) % 12;
                s_last_time = now;
            }
            break;
        }
        case RGB_STATUS_ALARM_LIGHT: {
            // 光照越界：红灯300→灭200→绿灯300→灭200→蓝灯300→灭200
            static const int steps[] = {300, 200, 300, 200, 300, 200};
            static const int colors[][3] = {
                {255,0,0}, {0,0,0}, {0,255,0}, {0,0,0}, {0,0,255}, {0,0,0}
            };
            if (now - s_last_time >= steps[s_step]) {
                rgb_led_set_rgb(colors[s_step][0], colors[s_step][1], colors[s_step][2]);
                s_step = (s_step + 1) % 6;
                s_last_time = now;
            }
            break;
        }
        case RGB_STATUS_ALARM_TEMP: {
            // 温度越界：红灯500→绿灯500→蓝灯500→灭700
            static const int steps[] = {500, 500, 500, 700};
            static const int colors[][3] = {
                {255,0,0}, {0,255,0}, {0,0,255}, {0,0,0}
            };
            if (now - s_last_time >= steps[s_step]) {
                rgb_led_set_rgb(colors[s_step][0], colors[s_step][1], colors[s_step][2]);
                s_step = (s_step + 1) % 4;
                s_last_time = now;
            }
            break;
        }
        case RGB_STATUS_MQTT_DISCONNECTED: {
            // MQTT未连接：红→绿渐变(500ms)→绿→灭渐变(500ms)→灭→蓝渐变(500ms)→蓝→灭渐变(500ms)
            unsigned long elapsed = now - s_last_time;
            if (s_step == 0) { // 红→绿渐变
                int r = 255 - (elapsed * 255 / 500);
                int g = elapsed * 255 / 500;
                rgb_led_set_rgb(r, g, 0);
                if (elapsed >= 500) { s_step = 1; s_last_time = now; }
            } else if (s_step == 1) { // 绿→灭渐变
                int g = 255 - (elapsed * 255 / 500);
                rgb_led_set_rgb(0, g, 0);
                if (elapsed >= 500) { s_step = 2; s_last_time = now; }
            } else if (s_step == 2) { // 灭→蓝渐变
                int b = elapsed * 255 / 500;
                rgb_led_set_rgb(0, 0, b);
                if (elapsed >= 500) { s_step = 3; s_last_time = now; }
            } else { // 蓝→灭渐变
                int b = 255 - (elapsed * 255 / 500);
                rgb_led_set_rgb(0, 0, b);
                if (elapsed >= 500) { s_step = 0; s_last_time = now; }
            }
            break;
        }
        case RGB_STATUS_WIFI_DISCONNECTED: {
            // WiFi未连接：红灯200→绿灯200→灭500
            static const int steps[] = {200, 200, 500};
            static const int colors[][3] = {
                {255,0,0}, {0,255,0}, {0,0,0}
            };
            if (now - s_last_time >= steps[s_step]) {
                rgb_led_set_rgb(colors[s_step][0], colors[s_step][1], colors[s_step][2]);
                s_step = (s_step + 1) % 3;
                s_last_time = now;
            }
            break;
        }
        case RGB_STATUS_NORMAL: {
            // 正常状态：灭→红渐变(1000ms)→红→绿渐变(1000ms)→绿→蓝渐变(1000ms)→蓝→灭渐变(1000ms)→灭200ms
            unsigned long elapsed = now - s_last_time;
            if (s_step == 0) { // 灭→红渐变
                int r = elapsed * 255 / 1000;
                rgb_led_set_rgb(r, 0, 0);
                if (elapsed >= 1000) { s_step = 1; s_last_time = now; }
            } else if (s_step == 1) { // 红→绿渐变
                int r = 255 - (elapsed * 255 / 1000);
                int g = elapsed * 255 / 1000;
                rgb_led_set_rgb(r, g, 0);
                if (elapsed >= 1000) { s_step = 2; s_last_time = now; }
            } else if (s_step == 2) { // 绿→蓝渐变
                int g = 255 - (elapsed * 255 / 1000);
                int b = elapsed * 255 / 1000;
                rgb_led_set_rgb(0, g, b);
                if (elapsed >= 1000) { s_step = 3; s_last_time = now; }
            } else if (s_step == 3) { // 蓝→灭渐变
                int b = 255 - (elapsed * 255 / 1000);
                rgb_led_set_rgb(0, 0, b);
                if (elapsed >= 1000) { s_step = 4; s_last_time = now; }
            } else { // 灭状态保持200ms
                rgb_led_set_rgb(0, 0, 0);
                if (elapsed >= 200) { s_step = 0; s_last_time = now; }
            }
            break;
        }
    }
}