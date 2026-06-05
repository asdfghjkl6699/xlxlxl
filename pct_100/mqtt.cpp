#include "mqtt.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include "light.h"
#include "temp.h"
#include "lcd.h"

// 状态枚举（与主程序一致）
enum State { S00, S01, S10, S11 };

// 运行时可变的阈值变量（初始化为默认值）
float g_temp_threshold = TEMP_THRESHOLD;
int g_light_threshold = LIGHT_THRESHOLD;  // ADC阈值（0-4095）

// 内部状态
static WiFiClient      s_wifi_client;
static PubSubClient    s_mqtt(s_wifi_client);
static char            s_status_topic[MQTT_TOPIC_LEN];
static char            s_command_topic[MQTT_TOPIC_LEN];
static char            s_lwt_topic[MQTT_TOPIC_LEN];
static unsigned long   s_last_publish = 0;
static unsigned long   s_last_reconnect_attempt = 0;
static bool            s_initialized  = false;
static bool            s_auto_connect_enabled = true;  // 是否允许自动连接（启动时为true，断开后为false）

// 配置变量
static char     s_ip[MQTT_IP_LEN]         = MQTT_IP_DEFAULT;
static uint16_t s_port                    = MQTT_PORT_DEFAULT;
static char     s_user[MQTT_USER_LEN]     = MQTT_USER_DEFAULT;
static char     s_pass[MQTT_PASS_LEN]     = MQTT_PASS_DEFAULT;
static char     s_device_id[MQTT_ID_LEN]  = DEVICE_ID_DEFAULT;

// NVS 键
static const char* NVS_KEY_IP    = "mqtt_ip";
static const char* NVS_KEY_PORT  = "mqtt_port";
static const char* NVS_KEY_USER  = "mqtt_user";
static const char* NVS_KEY_PASS  = "mqtt_pass";
static const char* NVS_KEY_ID    = "mqtt_id";
static const char* NVS_KEY_TEMP_THRESH = "temp_thresh";
static const char* NVS_KEY_LIGHT_THRESH = "light_thresh";

// NVS 加载
static void load_mqtt_config(void)
{
    Preferences p;
    p.begin("pct100", true);
    String ip   = p.getString(NVS_KEY_IP,   MQTT_IP_DEFAULT);
    int     port = p.getInt(NVS_KEY_PORT,   MQTT_PORT_DEFAULT);
    String user = p.getString(NVS_KEY_USER, MQTT_USER_DEFAULT);
    String pass = p.getString(NVS_KEY_PASS, MQTT_PASS_DEFAULT);
    String id   = p.getString(NVS_KEY_ID,   DEVICE_ID_DEFAULT);
    
    // 加载阈值（使用保存的值，如果没有则使用默认值）
    g_temp_threshold = p.getFloat(NVS_KEY_TEMP_THRESH, TEMP_THRESHOLD);
    g_light_threshold = p.getInt(NVS_KEY_LIGHT_THRESH, LIGHT_THRESHOLD);
    
    p.end();

    strncpy(s_ip, ip.c_str(), sizeof(s_ip) - 1);
    s_ip[sizeof(s_ip) - 1] = '\0';
    s_port = (uint16_t)port;
    strncpy(s_user, user.c_str(), sizeof(s_user) - 1);
    s_user[sizeof(s_user) - 1] = '\0';
    strncpy(s_pass, pass.c_str(), sizeof(s_pass) - 1);
    s_pass[sizeof(s_pass) - 1] = '\0';
    strncpy(s_device_id, id.c_str(), sizeof(s_device_id) - 1);
    s_device_id[sizeof(s_device_id) - 1] = '\0';

    Serial.printf("[MQTT] NVS 加载: %s:%d user=%s id=%s\n",
                  s_ip, s_port, s_user, s_device_id);
    Serial.printf("[MQTT] 加载阈值: temp=%.1f, light=%d\n", g_temp_threshold, g_light_threshold);
}

// 主题构造
static void build_topics(void)
{
    snprintf(s_status_topic,  sizeof(s_status_topic),
             "chemctrl/%s/status",  s_device_id);
    snprintf(s_command_topic, sizeof(s_command_topic),
             "chemctrl/%s/command", s_device_id);
    snprintf(s_lwt_topic,     sizeof(s_lwt_topic),
             "chemctrl/%s/lwt",     s_device_id);
}

// 外部变量声明（从主程序获取）
extern float current_temp;
extern int light_intensity;
extern bool is_auto_mode;
extern bool key1_is_on(void);
extern enum State current_state;

// 函数原型声明
bool mqtt_mgr_save_config(void);

// MQTT消息回调
static void mqtt_callback(char* topic, byte* payload, unsigned int length)
{
    Serial.print("[MQTT] 收到消息: ");
    Serial.println(topic);
    
    StaticJsonDocument<256> doc;
    deserializeJson(doc, payload, length);
    
    const char* cmd = doc["cmd"] | "";
    
    if (strcmp(cmd, "set_relay") == 0) {
        int relay = doc["relay"] | 0;
        bool val = doc["value"] | false;
        Serial.printf("[MQTT] set_relay relay=%d val=%d\n", relay, val);
    } else if (strcmp(cmd, "get_status") == 0) {
        Serial.println("[MQTT] get_status");
        mqtt_mgr_publish_status();
    } else if (strcmp(cmd, "set_mode") == 0) {
        const char* mode = doc["mode"] | "";
        Serial.printf("[MQTT] set_mode %s\n", mode);
    } else if (strcmp(cmd, "set_threshold") == 0) {
        // 处理阈值修改命令（前端发送的字段名是 temp 和 light）
        float new_temp_thresh = doc["temp"] | g_temp_threshold;
        int new_light_thresh = doc["light"] | (1000 - (g_light_threshold * 1000) / LIGHT_MAX_ADC);
        
        // 更新温度阈值
        g_temp_threshold = new_temp_thresh;
        
        // 更新光照阈值（前端值反转后转换为ADC范围：0-4095）
        // 前端值范围: 0(暗)-1000(亮), ADC范围: 4095(暗)-0(亮)
        // 使用四舍五入避免整数除法误差
        g_light_threshold = ((1000 - new_light_thresh) * LIGHT_MAX_ADC + 500) / 1000;
        
        Serial.printf("[MQTT] set_threshold temp=%.1f light=%d (display=%d)\n", 
                      g_temp_threshold, g_light_threshold, new_light_thresh);
        
        // 自动保存阈值到Flash，下次开机自动加载
        mqtt_mgr_save_config();
        
        // 立即更新LCD显示
        int light_thresh_display = 1000 - (g_light_threshold * 1000) / LIGHT_MAX_ADC;
        bool led_on = (current_state == S10 || current_state == S11);
        bool fan_on = (current_state == S01 || current_state == S11);
        bool wifi_connected = (WiFi.status() == WL_CONNECTED);
        bool mqtt_connected = s_mqtt.connected();
        lcd_update(is_auto_mode, key1_is_on(), light_intensity, light_thresh_display, current_temp, g_temp_threshold, led_on, fan_on, wifi_connected, mqtt_connected);
        
        // 发布更新后的状态
        mqtt_mgr_publish_status();
    } else {
        Serial.printf("[MQTT] 未知命令: %s\n", cmd);
    }
}

// 继电器引脚定义
#define RELAY_LED_PIN    6  // 灯继电器引脚
#define RELAY_FAN_PIN    7  // 风扇继电器引脚

// 发布状态
void mqtt_mgr_publish_status(void)
{
    if (!s_mqtt.connected()) return;

    // 直接读取引脚状态判断继电器
    bool led_on = digitalRead(RELAY_LED_PIN) == HIGH;
    bool fan_on = digitalRead(RELAY_FAN_PIN) == HIGH;

    StaticJsonDocument<256> doc;
    doc["temperature"]     = current_temp;
    doc["light"]           = light_intensity;  // 已在light.cpp中转换为0-1000范围
    doc["mode"]            = is_auto_mode ? "auto" : "manual";
    doc["key1_lock"]       = key1_is_on();
    doc["relay3"]          = led_on;
    doc["relay4"]          = fan_on;
    doc["temp_threshold"]  = g_temp_threshold;
    // 转换为前端显示范围（0-1000），使用四舍五入保持转换一致性
    int light_display = 1000 - (g_light_threshold * 1000 + LIGHT_MAX_ADC / 2) / LIGHT_MAX_ADC;
    doc["light_threshold"] = light_display;

    char buf[256];
    serializeJson(doc, buf, sizeof(buf));
    s_mqtt.publish(s_status_topic, buf);
    Serial.printf("[MQTT] 发布状态: %s\n", buf);
}

// MQTT重连（非阻塞方式）
static void mqtt_reconnect(void)
{
    if (s_mqtt.connected()) return;

    // 非阻塞重连：每5秒尝试一次
    unsigned long now = millis();
    if (now - s_last_reconnect_attempt < 5000) {
        return;
    }
    s_last_reconnect_attempt = now;

    Serial.printf("[MQTT] 尝试连接 %s:%d (client_id=%s)...\n", s_ip, s_port, s_device_id);
    
    // 使用 connect 方法的扩展版本设置 LWT
    if (s_mqtt.connect(s_device_id, s_user, s_pass, s_lwt_topic, MQTT_QOS, true, MQTT_LWT_OFFLINE)) {
        Serial.println("[MQTT] 连接成功！");
        
        s_mqtt.publish(s_lwt_topic, MQTT_LWT_ONLINE);
        
        s_mqtt.subscribe(s_command_topic);
        Serial.printf("[MQTT] 订阅主题: %s\n", s_command_topic);
        
    } else {
        int rc = s_mqtt.state();
        Serial.printf("[MQTT] 连接失败, rc=%d ", rc);
        switch(rc) {
            case -1: Serial.println("(连接超时)"); break;
            case -2: Serial.println("(客户端ID无效)"); break;
            case -3: Serial.println("(服务器拒绝连接/认证失败)"); break;
            case -4: Serial.println("(服务器不可达)"); break;
            default: Serial.println("(未知错误)");
        }
    }
}

// 设置设备ID
void mqtt_mgr_set_device_id(const char* id)
{
    // 先保存旧的设备ID用于日志
    char old_id[MQTT_ID_LEN];
    strcpy(old_id, s_device_id);
    
    // 更新设备ID
    strncpy(s_device_id, id, sizeof(s_device_id) - 1);
    s_device_id[sizeof(s_device_id) - 1] = '\0';
    
    // 重新构造主题
    build_topics();
    
    // 强制断开MQTT连接
    if (s_mqtt.connected()) {
        s_mqtt.disconnect();
        Serial.printf("[MQTT] 已断开旧连接 (ID: %s)\n", old_id);
    }
    
    // 重要：重新初始化WiFiClient，彻底清除连接状态
    s_wifi_client.stop();
    delay(100);
    
    // 等待2秒让服务器释放旧连接
    // MQTT服务器需要时间检测连接断开（通常30秒心跳超时），避免ID冲突
    Serial.println("[MQTT] 等待服务器释放连接...");
    delay(2000);
    
    // 重置重连计时器，立即尝试重新连接
    s_last_reconnect_attempt = 0;
    
    Serial.printf("[MQTT] 设备ID已修改为: %s，准备重新连接\n", s_device_id);
}

// 获取设备ID
const char* mqtt_mgr_get_device_id(void)
{
    return s_device_id;
}

// 保存配置到Flash
bool mqtt_mgr_save_config(void)
{
    Preferences p;
    if (!p.begin("pct100", false)) {
        Serial.println("[MQTT] 打开NVS失败");
        return false;
    }
    
    p.putString(NVS_KEY_IP, s_ip);
    p.putInt(NVS_KEY_PORT, s_port);
    p.putString(NVS_KEY_USER, s_user);
    p.putString(NVS_KEY_PASS, s_pass);
    p.putString(NVS_KEY_ID, s_device_id);
    
    // 保存阈值
    p.putFloat(NVS_KEY_TEMP_THRESH, g_temp_threshold);
    p.putInt(NVS_KEY_LIGHT_THRESH, g_light_threshold);
    
    p.end();
    
    Serial.println("[MQTT] 配置已保存到Flash");
    return true;
}

// 重置为默认配置
void mqtt_mgr_reset_config(void)
{
    strcpy(s_ip, MQTT_IP_DEFAULT);
    s_port = MQTT_PORT_DEFAULT;
    strcpy(s_user, MQTT_USER_DEFAULT);
    strcpy(s_pass, MQTT_PASS_DEFAULT);
    strcpy(s_device_id, DEVICE_ID_DEFAULT);
    
    // 重置阈值为默认值
    g_temp_threshold = TEMP_THRESHOLD;
    g_light_threshold = LIGHT_THRESHOLD;
    
    build_topics();
    
    // 断开连接以重新连接
    if (s_mqtt.connected()) {
        s_mqtt.disconnect();
        s_wifi_client.stop();
        delay(100);
    }
    s_last_reconnect_attempt = 0;
    
    Serial.println("[MQTT] 配置已重置为默认值");
}

// 初始化MQTT
void mqtt_mgr_init(void)
{
    load_mqtt_config();
    build_topics();
    
    // 设置服务器地址（必须在connect之前调用）
    s_mqtt.setServer(s_ip, s_port);
    s_mqtt.setCallback(mqtt_callback);
    
    // 初始化重连计时器为负值，确保开机后立即尝试连接
    s_last_reconnect_attempt = -5000;
    
    s_initialized = true;
    Serial.println("[MQTT] 模块初始化完成");
}

// MQTT更新
void mqtt_mgr_update(void)
{
    if (!s_initialized) return;
    
    if (WiFi.status() == WL_CONNECTED) {
        // 只有允许自动连接时才尝试重连
        if (s_auto_connect_enabled) {
            mqtt_reconnect();
        }
        s_mqtt.loop();
        // 移除定时心跳发布，改为事件驱动发布（按键、模式切换、状态变化时立即发布）
    }
}

// 检查连接状态
bool mqtt_mgr_is_connected(void)
{
    // 如果WiFi未连接，MQTT肯定也未连接
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    return s_mqtt.connected();
}

// 断开MQTT连接
void mqtt_mgr_disconnect(void)
{
    if (s_mqtt.connected()) {
        s_mqtt.disconnect();
        s_wifi_client.stop();
        s_last_reconnect_attempt = 0;  // 重置重连计时器
        s_auto_connect_enabled = false;  // 禁用自动连接，需手动重连
        Serial.println("[MQTT] 已断开连接（需手动输入mqtt_reconnect命令重新连接）");
    } else {
        Serial.println("[MQTT] 未连接，无需断开");
    }
}

// 立即尝试重新连接MQTT
void mqtt_mgr_reconnect_now(void)
{
    if (!s_initialized) {
        Serial.println("[MQTT] 模块未初始化");
        return;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[MQTT] WiFi未连接，请先连接WiFi");
        return;
    }
    
    s_auto_connect_enabled = true;  // 启用自动连接
    s_last_reconnect_attempt = 0;  // 重置重连计时器，立即触发重连
    Serial.println("[MQTT] 正在尝试重新连接...");
}