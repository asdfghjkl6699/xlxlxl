#include "lcd.h"
#include <U8g2lib.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0,
  /* reset=*/ U8X8_PIN_NONE,
  /* clock=*/ OLED_SCL_PIN,
  /* data=*/ OLED_SDA_PIN
);

void lcd_init(void) {
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.drawUTF8(28, 35, "系统启动中...");
  u8g2.sendBuffer();
  delay(1000);
}

void lcd_update(bool is_auto_mode, bool main_on,
                 int light_val, int light_threshold,
                 float temp_val, float temp_threshold,
                 bool led_on, bool fan_on,
                 bool wifi_connected, bool mqtt_connected) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);

  // 第一行：模式 + 总开关
  u8g2.setCursor(2, 12);
  u8g2.print("模式:");
  u8g2.print(is_auto_mode ? "自动" : "手动");
  u8g2.setCursor(70, 12);
  u8g2.print("总:");
  u8g2.print(main_on ? "开" : "关");

  // 第二行：光照（显示当前值/阈值）
  u8g2.setCursor(2, 24);
  u8g2.print("光照:");
  u8g2.print(light_val);
  u8g2.print("/");
  u8g2.print(light_threshold);
  u8g2.print("lux");

  // 第三行：温度（显示当前值/阈值）
  u8g2.setCursor(2, 36);
  u8g2.print("温度:");
  u8g2.print(temp_val, 1);
  u8g2.print("/");
  u8g2.print(temp_threshold, 1);
  u8g2.print("℃");

  // 第四行：灯 + 风扇
  u8g2.setCursor(2, 48);
  u8g2.print("灯:");
  u8g2.print(led_on ? "开" : "关");
  u8g2.setCursor(58, 48);
  u8g2.print("风扇:");
  u8g2.print(fan_on ? "开" : "关");

  // 第五行：WiFi + MQTT状态
  u8g2.setCursor(2, 60);
  u8g2.print("WiFi:");
  u8g2.print(wifi_connected ? "是" : "否");
  u8g2.setCursor(65, 60);
  u8g2.print("MQTT:");
  u8g2.print(mqtt_connected ? "是" : "否");

  u8g2.sendBuffer();
}