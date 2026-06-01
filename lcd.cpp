// d:\Arduino\Projects\PCT_100\lcd.cpp
#include "lcd.h"
#include <U8g2lib.h>

// 自定义I2C引脚（根据电路图：SDA→IO4，SCL→IO5）
#define I2C_SDA 4
#define I2C_SCL 5

// 使用软件I2C，指定地址为0x3C（SH1106常见地址）
U8G2_SH1106_128X64_VCOMH0_F_SW_I2C u8g2(U8G2_R0, I2C_SCL, I2C_SDA, U8X8_PIN_NONE);

void lcd_init(void) {
  u8g2.begin();
  u8g2.setContrast(0xFF);  // 最大对比度
  u8g2.setPowerSave(0);    // 开启显示
  u8g2.setFont(u8g2_font_ncenB08_tr);  // 设置字体
}

void lcd_update(bool is_auto, bool main_on, int light_val, float temp_val, bool led_on, bool fan_on) {
  u8g2.clearBuffer();
  
  // 第一行：MODE和MAIN状态
  u8g2.drawStr(0, 10, "MODE: ");
  u8g2.drawStr(40, 10, is_auto ? "auto" : "manual");
  u8g2.drawStr(68, 10, "MAIN: ");
  u8g2.drawStr(105, 10, main_on ? "ON" : "OFF");
  
  // 第二行：光照值
  u8g2.drawStr(0, 25, "LIGHT: ");
  char light_str[10];
  sprintf(light_str, "%d", light_val);
  u8g2.drawStr(56, 25, light_str);
  u8g2.drawStr(80, 25, "/ 4095");
  
  // 第三行：温度值
  u8g2.drawStr(0, 40, "TEMP: ");
  char temp_str[10];
  sprintf(temp_str, "%.1f", temp_val);
  u8g2.drawStr(48, 40, temp_str);
  u8g2.drawStr(72, 40, "/ 32.0");
  
  // 第四行：LED和FAN状态
  u8g2.drawStr(0, 55, "LED: ");
  u8g2.drawStr(36, 55, led_on ? "ON" : "OFF");
  u8g2.drawStr(60, 55, "FAN: ");
  u8g2.drawStr(96, 55, fan_on ? "ON" : "OFF");
  
  u8g2.sendBuffer();  // 刷新屏幕
}