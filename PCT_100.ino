#include "exti.h"
#include "relay.h"
#include "light.h"
#include "temp.h"
#include "lcd.h"
#include "ws2812.h"
#include "WiFi.h"
#include <nvs_flash.h>
#include <nvs.h>

// WiFi配置
char WIFI_SSID[32] = "";    // WiFi账号
char WIFI_PASSWORD[64] = "";  // WiFi密码

// NVS存储相关
#define NVS_NAMESPACE "wifi_config"
nvs_handle_t my_nvs_handle;

// 状态枚举
enum State { S00, S01, S10, S11 };

// 从NVS读取WiFi配置
bool loadWiFiConfig() {
  esp_err_t err;
  
  // 打开NVS命名空间
  err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_nvs_handle);
  if (err != ESP_OK) {
    Serial.println("NVS打开失败，首次配置");
    return false;
  }  
  
  // 读取SSID
  size_t len = sizeof(WIFI_SSID);
  err = nvs_get_str(my_nvs_handle, "ssid", WIFI_SSID, &len);
  if (err != ESP_OK) {
    Serial.println("未找到保存的WiFi配置");
    nvs_close(my_nvs_handle);
    return false;
  }
  
  // 读取密码
  len = sizeof(WIFI_PASSWORD);
  err = nvs_get_str(my_nvs_handle, "password", WIFI_PASSWORD, &len);
  if (err != ESP_OK) {
    Serial.println("密码读取失败");
    nvs_close(my_nvs_handle);
    return false;
  }
  
  .
  nvs_close(my_nvs_handle);
  Serial.printf("已加载保存的WiFi配置: %s\n", WIFI_SSID);
  return true;
} 

// 保存WiFi配置到NVS
bool saveWiFiConfig() {
  esp_err_t err;
  
  // 打开NVS命名空间（读写模式）
  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_nvs_handle);
  if (err != ESP_OK) {
    Serial.println("NVS打开失败");
    return false;
  }
  
  // 写入SSID
  err = nvs_set_str(my_nvs_handle, "ssid", WIFI_SSID);
  if (err != ESP_OK) {
    Serial.println("SSID保存失败");
    nvs_close(my_nvs_handle);
    return false;
  }
  
  // 写入密码
  err = nvs_set_str(my_nvs_handle, "password", WIFI_PASSWORD);
  if (err != ESP_OK) {
    Serial.println("密码保存失败");
    nvs_close(my_nvs_handle);
    return false;
  }
  
  // 提交写入
  err = nvs_commit(my_nvs_handle);
  if (err != ESP_OK) {
    Serial.println("NVS提交失败");
    nvs_close(my_nvs_handle);
    return false;
  }
  
  nvs_close(my_nvs_handle);
  Serial.println("WiFi配置已保存到Flash");
  return true;
}

// 设置继电器
void setRelay(enum State s) {
  switch (s) {
    case S00: digitalWrite(6, LOW);  digitalWrite(7, LOW);  break;
    case S01: digitalWrite(6, LOW);  digitalWrite(7, HIGH); break;
    case S10: digitalWrite(6, HIGH); digitalWrite(7, LOW);  break;
    case S11: digitalWrite(6, HIGH); digitalWrite(7, HIGH); break;
  }
}

enum State current_state = S00;
int step = 0;

// 长按配置
unsigned long key2_down_time = 0;
bool key2_holding = false;
const unsigned int LONG_PRESS_MIN = 1000;  // 1秒
const unsigned int LONG_PRESS_MAX = 2000;  // 2秒

// 模式切换标志
bool is_auto_mode = true;  

// 定时时间戳（分离各个任务周期，互不干扰）
static unsigned long last_sensor_time = 0;
static unsigned long last_display_time = 0;
const unsigned int SENSOR_INTERVAL = 50;    // 传感器50ms更新一次（加快ADC读取频率）
const unsigned int DISPLAY_INTERVAL = 2000;  // 显示+串口2s更新一次

// 断开WiFi连接（保留配置，方便重连）
void disconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    // 参数为false表示断开连接但保留AP配置
    WiFi.disconnect(false);
    // 关闭WiFi模块电源以确保完全断开
    WiFi.mode(WIFI_OFF);
    Serial.println("\nWiFi已断开连接（配置已保留）");
  } else {
    Serial.println("\nWiFi未连接");
  }
}

// 清除保存的WiFi配置
void clearWiFiConfig() {
  esp_err_t err;
  
  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_nvs_handle);
  if (err != ESP_OK) {
    Serial.println("NVS打开失败");
    return;
  }
  
  // 删除SSID和密码
  err = nvs_erase_key(my_nvs_handle, "ssid");
  if (err != ESP_OK) {
    Serial.println("SSID删除失败");
    nvs_close(my_nvs_handle);
    return;
  }
  
  err = nvs_erase_key(my_nvs_handle, "password");
  if (err != ESP_OK) {
    Serial.println("密码删除失败");
    nvs_close(my_nvs_handle);
    return;
  }
  
  err = nvs_commit(my_nvs_handle);
  if (err != ESP_OK) {
    Serial.println("NVS提交失败");
    nvs_close(my_nvs_handle);
    return;
  }
  
  nvs_close(my_nvs_handle);
  WIFI_SSID[0] = '\0';
  WIFI_PASSWORD[0] = '\0';
  Serial.println("WiFi配置已清除，下次上电将进入配置向导");
}

// WiFi扫描和连接函数
void wifiConnect() {
  Serial.println("\n====================================");
  Serial.println("        WiFi配置向导");
  Serial.println("====================================");
  
  // 开始扫描WiFi网络
  Serial.println("\n正在扫描附近WiFi网络...");
  int n = WiFi.scanNetworks();
  
  if (n == 0) {
    Serial.println("未发现WiFi网络！");
    return;
  }
  
  Serial.printf("\n发现 %d 个WiFi网络:\n", n);
  for (int i = 0; i < n; i++) {
    Serial.printf("%2d. %s	信号强度: %ddBm	加密: %s\n",
                  i + 1,
                  WiFi.SSID(i).c_str(),
                  WiFi.RSSI(i),
                  WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "无" : "有");
    delay(10);
  }
  
  // 等待用户选择WiFi
  Serial.println("\n请输入要连接的WiFi序号（1-" + String(n) + "):");
  while (!Serial.available()) {
    delay(100);
  }
  int selected = Serial.parseInt() - 1;
  while (Serial.available()) Serial.read(); // 清空缓冲区
  
  if (selected < 0 || selected >= n) {
    Serial.println("输入无效！");
    return;
  }
  
  char selected_ssid[32];
  strcpy(selected_ssid, WiFi.SSID(selected).c_str());
  Serial.printf("\n已选择: %s\n", selected_ssid);
  
  // 检查是否是已保存过密码的网络
  bool useSavedPassword = false;
  if (strcmp(selected_ssid, WIFI_SSID) == 0 && strlen(WIFI_PASSWORD) > 0) {
    useSavedPassword = true;
    Serial.println("检测到已保存的密码，自动使用...");
  }
  
  if (!useSavedPassword) {
    // 输入密码
    Serial.println("请输入WiFi密码（无密码直接回车）:");
    while (!Serial.available()) {
      delay(100);
    }
    
    int len = 0;
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') break;
      if (len < 63) {
        WIFI_PASSWORD[len++] = c;
      }
    }
    WIFI_PASSWORD[len] = '\0';
  }
  
  // 更新SSID为当前选择的网络
  strcpy(WIFI_SSID, selected_ssid);
  
  // 连接WiFi
  Serial.printf("\n正在连接 %s...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int connect_count = 0;
  while (WiFi.status() != WL_CONNECTED && connect_count < 30) {
    delay(500);
    Serial.print(".");
    connect_count++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n\n====================================");
    Serial.println("        WiFi连接成功！");
    Serial.println("====================================");
    Serial.print("IP地址: ");
    Serial.println(WiFi.localIP());
    Serial.print("子网掩码: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("网关: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("DNS服务器: ");
    Serial.println(WiFi.dnsIP());
    Serial.println("====================================\n");
    
    // 连接成功后保存配置到Flash
    saveWiFiConfig();
  } else {
    Serial.println("\nWiFi连接失败！请检查密码是否正确。");
  }
}

void setup() {
  Serial.begin(115200);
  
  // 初始化WS2812 LED
  ws2812_init();
  
  // 开机万色灯闪烁提示
  Serial.println("\n====================================");
  Serial.println("        开机万色灯检测");
  Serial.println("====================================");
  ws2812_rainbow_cycle(2);  // 循环2次
  ws2812_off();
  Serial.println("万色灯检测完成！\n");
  
  // 初始化NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS分区需要擦除，然后重新初始化
    nvs_flash_erase();
    nvs_flash_init();
  }
  
  // 尝试从NVS加载WiFi配置
  bool hasSavedConfig = loadWiFiConfig();
  
  if (hasSavedConfig) {
    // 有保存的配置，直接连接
    Serial.printf("尝试连接已保存的WiFi: %s...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int connect_count = 0;
    while (WiFi.status() != WL_CONNECTED && connect_count < 30) {
      delay(500);
      Serial.print(".");
      ws2812_wifi_indicator(false);  // 连接中指示
      connect_count++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi连接成功！");
      Serial.print("IP地址: ");
      Serial.println(WiFi.localIP());
      ws2812_wifi_indicator(true);  // 已连接指示
    } else {
      Serial.println("\n保存的WiFi连接失败，进入配置向导...");
      wifiConnect();
    }
  } else {
    // 没有保存的配置，进入配置向导
    wifiConnect();
  }
  
  exti_init();
  relay_init();
  relay_off();
  setRelay(S00);
  light_init();
  temp_init();
  lcd_init();
  Serial.println("系统初始化完成，默认：自动模式");
  Serial.println("ADC 分辨率: 12 位 (0 ~ 4095)");
}

void loop() {
  // ===================== WiFi状态指示更新 =====================
  ws2812_wifi_indicator(WiFi.status() == WL_CONNECTED);
  
  // ===================== 【串口命令处理】 =====================
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    
    if (cmd == "disconnect") {
      disconnectWiFi();
    } else if (cmd == "clear") {
      clearWiFiConfig();
    } else if (cmd == "reconnect") {
      disconnectWiFi();
      delay(500);
      if (strlen(WIFI_SSID) > 0) {
        // 重新启用WiFi模块
        WiFi.mode(WIFI_STA);
        Serial.printf("重新连接 %s...\n", WIFI_SSID);
        // 使用保存的SSID和密码直接连接
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        
        int connect_count = 0;
        while (WiFi.status() != WL_CONNECTED && connect_count < 30) {
          delay(500);
          Serial.print(".");
          ws2812_wifi_indicator(false);  // 连接中指示
          connect_count++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("\n重新连接成功！");
          Serial.print("IP地址: ");
          Serial.println(WiFi.localIP());
          ws2812_wifi_indicator(true);  // 已连接指示
        } else {
          Serial.println("\n重新连接失败！");
        }
      } else {
        Serial.println("没有保存的WiFi配置，请进入配置向导");
        wifiConnect();
      }
    } else if (cmd == "wifi") {
      wifiConnect();
    } else if (cmd == "status") {
      Serial.print("WiFi状态: ");
      switch (WiFi.status()) {
        case WL_CONNECTED:
          Serial.println("已连接");
          Serial.print("IP: ");
          Serial.println(WiFi.localIP());
          break;
        case WL_DISCONNECTED:
          Serial.println("已断开");
          break;
        case WL_CONNECT_FAILED:
          Serial.println("连接失败");
          break;
        default:
          Serial.println("未知");
      }
    } else if (cmd == "test_led") {
      // 测试万色灯功能
      Serial.println("\n====================================");
      Serial.println("        万色灯测试");
      Serial.println("====================================");
      
      // 测试各种颜色
      Serial.println("红色...");
      ws2812_set_color(COLOR_RED);
      delay(500);
      
      Serial.println("绿色...");
      ws2812_set_color(COLOR_GREEN);
      delay(500);
      
      Serial.println("蓝色...");
      ws2812_set_color(COLOR_BLUE);
      delay(500);
      
      Serial.println("黄色...");
      ws2812_set_color(COLOR_YELLOW);
      delay(500);
      
      Serial.println("青色...");
      ws2812_set_color(COLOR_CYAN);
      delay(500);
      
      Serial.println("品红...");
      ws2812_set_color(COLOR_MAGENTA);
      delay(500);
      
      Serial.println("白色...");
      ws2812_set_color(COLOR_WHITE);
      delay(500);
      
      Serial.println("万色循环...");
      ws2812_rainbow_cycle(3);
      
      ws2812_wifi_indicator(WiFi.status() == WL_CONNECTED);
      Serial.println("\n万色灯测试完成！");
    }
  }

  // ===================== 【最高优先级】按键逻辑 优先执行 =====================
  exti_update();

  // -------- KEY1 总开关 --------
  if (key1_edge) {
    key1_edge = 0;
    if (!key1_is_on()) {
      is_auto_mode = true;
      current_state = S00;
      setRelay(S00);
      step = 0;
      key2_holding = false;
      Serial.println("KEY1 断开 → 全部关闭，复位到自动模式");
    } else {
      Serial.println("KEY1 闭合 → 系统已上电");
    }
  }

  // 主开关关闭：仅刷新屏幕，直接返回，不再执行后续逻辑
  if (!key1_is_on()) {
    key2_holding = false;
    int light_thresh_display = 1000 - (LIGHT_THRESHOLD * 1000 / LIGHT_MAX_ADC);
    lcd_update(is_auto_mode, false, light_intensity, light_thresh_display, current_temp, TEMP_THRESHOLD, false, false);
    return;
  }

  // -------- KEY2 按键（短按/长按）全程优先处理 --------
  static bool key2_processed = false;
  static bool last_key2_state = false;
  bool key2_state = (digitalRead(KEY2_PIN) == HIGH);

  // KEY2 按下触发
  if (key2_edge) {
    key2_edge = 0;
    key2_down_time = millis();
    key2_holding = true;
    key2_processed = false;
  }

  // KEY2 长按检测
  if (key2_holding && !key2_processed) {
    unsigned long hold_time = millis() - key2_down_time;
    if (hold_time >= LONG_PRESS_MIN && hold_time <= LONG_PRESS_MAX) {
      is_auto_mode = !is_auto_mode;
      key2_processed = true;

      if (is_auto_mode) {
        step = 0;
        if (is_dark && is_hot)      current_state = S11;
        else if (is_dark && !is_hot) current_state = S10;
        else if (!is_dark && is_hot) current_state = S01;
        else                         current_state = S00;
        setRelay(current_state);
        Serial.println("===== 切换为：自动模式 =====");
      } else {
        Serial.println("===== 切换为：手动模式 =====");
      }
    }
  }

  // KEY2 松手检测 + 短按逻辑
  if (last_key2_state && !key2_state && key2_holding) {
    unsigned long hold_time = millis() - key2_down_time;
    if (hold_time < LONG_PRESS_MIN && !key2_processed) {
      if (!is_auto_mode) {
        step++;
        switch (step) {
          case 1: current_state = S10; break;
          case 2: current_state = S00; break;
          case 3: current_state = S11; break;
          case 4: current_state = S01; step = 0; break;
        }
        setRelay(current_state);
        Serial.print("手动模式 → 短按 step: ");
        Serial.println(step);
        // 手动模式每按一下按键输出一次灯和风扇状态
        bool led_on  = (current_state == S10 || current_state == S11);
        bool fan_on  = (current_state == S01 || current_state == S11);
        Serial.print("模式: 手动\t灯: ");
        Serial.print(led_on ? "亮" : "灭");
        Serial.print("\t风扇: ");
        Serial.println(fan_on ? "开" : "关");
      } else {
        Serial.println("自动模式 → 短按无效");
      }
    }
    key2_holding = false;
    key2_processed = false;
  }
  last_key2_state = key2_state;
  // ===================== 按键逻辑结束 =====================


  // ===================== 【次优先级】传感器采集（定时非阻塞） =====================
  // 手动模式下不采集ADC，避免影响按键功能
  if (is_auto_mode && millis() - last_sensor_time >= SENSOR_INTERVAL) {
    last_sensor_time = millis();
    temp_update();
    light_update();

    // 自动模式逻辑（跟随传感器更新，响应及时）
    if (is_dark && is_hot && current_state != S11) {
      current_state = S11;
      setRelay(current_state);
    } else if (is_dark && !is_hot && current_state != S10) {
      current_state = S10;
      setRelay(current_state);
    } else if (!is_dark && is_hot && current_state != S01) {
      current_state = S01;
      setRelay(current_state);
    } else if (!is_dark && !is_hot && current_state != S00) {
      current_state = S00;
      setRelay(current_state);
    }
  }


  // ===================== 【最低优先级】LCD + 串口输出（低频定时，不抢占按键） =====================
  if (millis() - last_display_time >= DISPLAY_INTERVAL) {
    last_display_time = millis();

    bool led_on  = (current_state == S10 || current_state == S11);
    bool fan_on  = (current_state == S01 || current_state == S11);

    int light_thresh_display = 1000 - (LIGHT_THRESHOLD * 1000 / LIGHT_MAX_ADC);
    lcd_update(is_auto_mode, key1_is_on(), light_intensity, light_thresh_display, current_temp, TEMP_THRESHOLD, led_on, fan_on);

    if (is_auto_mode) {
      // 自动模式定时输出完整信息
      Serial.print("光照: ");
      Serial.print(light_intensity);
      Serial.print("/1000\t温度: ");
      Serial.print(current_temp, 1);
      Serial.print(" C\t模式: ");
      Serial.print("自动");
      Serial.print("\t灯: ");
      Serial.print(led_on ? "亮" : "灭");
      Serial.print("\t风扇: ");
      Serial.println(fan_on ? "开" : "关");
    }
    // 手动模式的串口输出在按键处理中完成
  }
}