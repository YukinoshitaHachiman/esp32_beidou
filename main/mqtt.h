#ifndef MQTT_H
#define MQTT_H
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"

//MQTT连接标志
extern bool s_is_mqtt_connected;
extern esp_mqtt_client_handle_t s_mqtt_client;
#define MQTT_ADDRESS    "mqtt://broker-cn.emqx.io"     //MQTT连接地址
#define MQTT_PORT       1883                        //MQTT连接端口号
#define MQTT_CLIENT     "mqttx_d0416"              //Client ID（设备唯一，大家最好自行改一下）
#define MQTT_USERNAME   "hello1"                     //MQTT用户名
#define MQTT_PASSWORD   "12345678"                  //MQTT密码

#define MQTT_PUBLIC_TOPIC      "/test/topic1"       //测试用的,推送消息主题
#define MQTT_SUBSCRIBE_TOPIC    "/test/topic2"      //测试用的,需要订阅的主题
//定义一个事件组，用于通知main函数WIFI连接成功
//#define WIFI_CONNECT_BIT     BIT0
//static EventGroupHandle_t   s_wifi_ev = NULL;

//MQTT客户端操作句柄


extern void mqtt_start(void);

#endif