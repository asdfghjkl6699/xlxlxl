#ifndef __MQTT_H
#define __MQTT_H

#include <WiFi.h>
#include <PubSubClient.h>

// MQTT 默认配置
#define MQTT_IP_DEFAULT     "47.98.170.180"
#define MQTT_PORT_DEFAULT   8081
#define MQTT_USER_DEFAULT   "dzdx_emqx"
#define MQTT_PASS_DEFAULT   "Jp4!sQ7$"
#define DEVICE_ID_DEFAULT   "PCT_100_004"

// 字符串缓冲长度
#define MQTT_IP_LEN         32
#define MQTT_USER_LEN       32
#define MQTT_PASS_LEN       32
#define MQTT_ID_LEN         32
#define MQTT_TOPIC_LEN      64

// 协议参数
#define MQTT_KEEPALIVE_S    60
#define MQTT_QOS            1

// 心跳周期
#define MQTT_HEARTBEAT_MS   60000UL

// LWT payload
#define MQTT_LWT_OFFLINE    "{\"online\":false}"
#define MQTT_LWT_ONLINE     "{\"online\":true}"

// 公开 API
void mqtt_mgr_init(void);
void mqtt_mgr_update(void);
bool mqtt_mgr_is_connected(void);
void mqtt_mgr_publish_status(void);
void mqtt_mgr_set_device_id(const char* id);  // 设置设备ID
const char* mqtt_mgr_get_device_id(void);     // 获取设备ID
void mqtt_mgr_disconnect(void);               // 断开MQTT连接
void mqtt_mgr_reconnect_now(void);            // 立即尝试重新连接MQTT

#endif