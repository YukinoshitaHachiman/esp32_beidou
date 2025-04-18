#include <stdio.h>
#include <string.h>
#include "myi2c.h"
#include "qmi8658c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"
#include "wifi_station.h"
#include "lcd_init.h"
#include <math.h>
#include "mqtt.h"
#include "time.h"

#define UART_NUM UART_NUM_1
#define RX_PIN 18
#define TX_PIN 19
#define BUF_SIZE 1024

t_sQMI8658C QMI8658C;
static QueueHandle_t uart1_queue;
static const char *TAG = "BEIDOU_MAIN"; 

static double g_last_lat = 0.0;
static double g_last_lon = 0.0;
static char g_last_lat_dir = 'N';
static char g_last_lon_dir = 'E';
static bool g_gps_data_ready = false;

static TaskHandle_t gps_publish_task_handle = NULL;
//static TaskHandle_t gyro_publish_task_handle = NULL;



void update_gps_info(const char *lat, char lat_dir, const char *lon, char lon_dir, const char *time);

double convert_to_decimal_degrees(const char* dm_str) {
    if (!dm_str || strlen(dm_str) == 0) {
        return 0.0;
    }
    
    double value = atof(dm_str);
    int degrees = (int)(value / 100);
    double minutes = value - (degrees * 100);
    return degrees + (minutes / 60.0);
}

void parse_gga(const char* sentence) {
    char time[20] = {0};  
    char lat[20] = {0};   
    char lon[20] = {0};   
    char lat_dir = 0, lon_dir = 0;
    int fix_quality = 0, num_satellites = 0;
    float altitude = 0.0;
    
    int result = sscanf(sentence, "$GNGGA,%[^,],%[^,],%c,%[^,],%c,%d,%d,%*[^,],%f",
        time, lat, &lat_dir, lon, &lon_dir, &fix_quality, &num_satellites, &altitude);
    
    if(result >= 5) {
        double decimal_lat = convert_to_decimal_degrees(lat);
        double decimal_lon = convert_to_decimal_degrees(lon);
        
        g_last_lat = decimal_lat;
        g_last_lon = decimal_lon;
        g_last_lat_dir = lat_dir ? lat_dir : 'N';
        g_last_lon_dir = lon_dir ? lon_dir : 'E';
        g_gps_data_ready = true;


        char lat_str[20], lon_str[20];
        snprintf(lat_str, sizeof(lat_str), "%.7f", decimal_lat);
        snprintf(lon_str, sizeof(lon_str), "%.7f", decimal_lon);
        update_gps_info(lat_str, lat_dir ? lat_dir : 'N',
            lon_str, lon_dir ? lon_dir : 'E',
            time);
    }
}

void uart_event_task(void *pvParameters) {    
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    uart_event_t event;
    uint8_t* dtmp = (uint8_t*) malloc(BUF_SIZE);
    static char buffer[BUF_SIZE * 2] = {0};
    static int buffer_len = 0;

    for(;;) {
        esp_task_wdt_reset();
        if(xQueueReceive(uart1_queue, (void *)&event, 5 / portTICK_PERIOD_MS)) {
            if(event.type == UART_DATA) {
                bzero(dtmp, BUF_SIZE);
                int len = uart_read_bytes(UART_NUM, dtmp, event.size, 5 / portTICK_PERIOD_MS);
                
                if(len > 0) {
                    if(buffer_len + len < sizeof(buffer)) {
                        memcpy(buffer + buffer_len, dtmp, len);
                        buffer_len += len;
                        buffer[buffer_len] = '\0';
                    } else {
                        buffer_len = 0;
                        continue;
                    }
                    
                    char *start = buffer;
                    char *end;
                    while((end = strstr(start, "\r\n")) != NULL) {
                        *end = '\0';
                        if(strncmp(start, "$GNGGA", 6) == 0) {
                            parse_gga(start);
                        }
                        start = end + 2;
                        vTaskDelay(1);
                    }
                    
                    if(start < buffer + buffer_len) {
                        buffer_len -= (start - buffer);
                        memmove(buffer, start, buffer_len);
                    } else {
                        buffer_len = 0;
                    }
                }
            }
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    free(dtmp);
    vTaskDelete(NULL);
}

// GPS发布任务
void gps_publish_task(void *pvParameters) {
    while(1) {
        if(s_is_mqtt_connected && g_gps_data_ready) {
            static char mqtt_pub_buff[128];
            snprintf(mqtt_pub_buff, sizeof(mqtt_pub_buff), 
                    "{\"latitude\":%.7f,\"lat_dir\":\"%c\",\"longitude\":%.7f,\"lon_dir\":\"%c\"}",
                    g_last_lat, g_last_lat_dir, g_last_lon, g_last_lon_dir);
            
            esp_mqtt_client_publish(s_mqtt_client, "/gps/location", mqtt_pub_buff, 0, 1, 0);
            ESP_LOGI(TAG, "已发布GPS数据: %s", mqtt_pub_buff);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // 1秒发送一次
    }
}

// 陀螺仪发布任务
// void gyro_publish_task(void *pvParameters) {
//     while(1) {
//         if(s_is_mqtt_connected) {
//             time_t now;
//             time(&now);
//             struct tm timeinfo;
//             localtime_r(&now, &timeinfo);
            
//             qmi8658c_fetch_angleFromAcc(&QMI8658C);
            
//             static char gyro_buff[256];
//             snprintf(gyro_buff, sizeof(gyro_buff), 
//                     "{\"time\":\"%04d-%02d-%02d %02d:%02d:%02d\",\"gyr_x\":%d,\"gyr_y\":%d,\"gyr_z\":%d}",
//                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
//                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
//                     QMI8658C.gyr_x, QMI8658C.gyr_y, QMI8658C.gyr_z);
            
//             esp_mqtt_client_publish(s_mqtt_client, "/qmi8658c", gyro_buff, 0, 1, 0);
//             ESP_LOGI(TAG, "已发布陀螺仪数据: %s", gyro_buff);
//         }
//         vTaskDelay(pdMS_TO_TICKS(100)); // 100ms发送一次，即每秒10次
//     }
// }

void app_main(void) {   

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    lcd_init();
    lvgl_init();

    ESP_LOGI("main", "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    
    printf("\n正在初始化北斗模块...\n");

    esp_task_wdt_deinit();
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 30000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config));

    const uart_config_t uart_config = {
        .baud_rate = 38400,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart1_queue, 0));

    if (xTaskCreate(uart_event_task, "uart_event_task", 4096, NULL, 12, NULL) == pdPASS) {
        printf("UART事件处理任务创建成功\n");
        printf("等待北斗数据...\n\n");
    } else {
        printf("错误：UART事件处理任务创建失败\n");
    }

    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");

    qmi8658c_init();
    mqtt_start();

    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    

    // 创建GPS发布任务
    xTaskCreate(gps_publish_task, 
            "gps_publish_task", 
            4096,           // 堆栈大小
            NULL,           // 参数
            5,             // 优先级
            &gps_publish_task_handle);

   // 创建陀螺仪发布任务
    // xTaskCreate(gyro_publish_task, 
    //         "gyro_publish_task", 
    //         4096,           // 堆栈大小
    //         NULL,           // 参数
    //         6,             // 优先级
    //         &gyro_publish_task_handle);

    while(1) {
    
        esp_task_wdt_reset();
        //qmi8658c_fetch_angleFromAcc(&QMI8658C);
        qmi8658c_Read_AccAndGry(&QMI8658C);
        ESP_LOGI(TAG, "gyr_x = %.1d  gyr_y = %.1d gyr_z = %.1d ",QMI8658C.gyr_x, QMI8658C.gyr_y, QMI8658C.gyr_z);
        if (abs(QMI8658C.gyr_x)>10000 || abs(QMI8658C.gyr_y)>10000 || abs(QMI8658C.gyr_z) > 10000) {
            esp_mqtt_client_publish(s_mqtt_client, "/warmming", "警告，疑似摔倒", 0, 1, 0);
            
        }
        vTaskDelay(pdMS_TO_TICKS(1000));

    }
}