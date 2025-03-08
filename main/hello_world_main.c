#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"
#include "wifi_station.h"
#include "http_client.h"
#include "lcd_init.h"
#define UART_NUM UART_NUM_1
#define RX_PIN 18
#define TX_PIN UART_PIN_NO_CHANGE
#define BUF_SIZE 1024

static QueueHandle_t uart1_queue;
static const char *TAG = "BEIDOU_MAIN"; 
void update_gps_info(const char *lat, char lat_dir, const char *lon, char lon_dir, const char *time);

// 修改GGA数据解析函数
void parse_gga(const char* sentence) {
    char time[20] = {0};  // 增加缓冲区大小
    char lat[20] = {0};   // 增加缓冲区大小
    char lon[20] = {0};   // 增加缓冲区大小
    char lat_dir = 0, lon_dir = 0;
    int fix_quality = 0, num_satellites = 0;
    float altitude = 0.0;
    
    // 使用更宽松的格式匹配
    int result = sscanf(sentence, "$GNGGA,%[^,],%[^,],%c,%[^,],%c,%d,%d,%*[^,],%f",
        time, lat, &lat_dir, lon, &lon_dir, &fix_quality, &num_satellites, &altitude);
    
    // 放宽结果检查条件
    if(result >= 5) {  // 只要能获取到基本的时间和位置信息就显示
        printf("\n=== GGA解析结果 ===\n");
        // 仅显示时间的前6位
        if(strlen(time) >= 6) {
            printf("时间: %c%c:%c%c:%c%c\n", 
                time[0], time[1],    // 时
                time[2], time[3],    // 分
                time[4], time[5]);   // 秒
        } else {
            printf("时间: %s\n", time);
        }
        
        printf("纬度: %s %c\n", lat, lat_dir ? lat_dir : 'N');
        printf("经度: %s %c\n", lon, lon_dir ? lon_dir : 'E');
        if(result >= 6) {
            printf("定位质量: %d (0=无效定位 1=GPS定位 2=DGPS定位)\n", fix_quality);
        }
        if(result >= 7) {
            printf("卫星数量: %d\n", num_satellites);
        }
        if(result >= 8) {
            printf("海拔高度: %.1f米\n", altitude);
        }
        printf("================\n");


        update_gps_info(lat, lat_dir ? lat_dir : 'N',
            lon, lon_dir ? lon_dir : 'E',
            time);
        
        
        // 创建数据结构并发送
        gps_data_t gps_data = {0};
        strncpy(gps_data.time, time, sizeof(gps_data.time)-1);
        strncpy(gps_data.latitude, lat, sizeof(gps_data.latitude)-1);
        strncpy(gps_data.longitude, lon, sizeof(gps_data.longitude)-1);
        gps_data.lat_dir = lat_dir;
        gps_data.lon_dir = lon_dir;
        gps_data.fix_quality = fix_quality;
        gps_data.num_satellites = num_satellites;
        gps_data.altitude = altitude;
        
        esp_err_t err = send_gps_data_to_server(&gps_data);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "发送数据失败: %s", esp_err_to_name(err));
        }
    }
}

// 修改ZDA数据解析函数
void parse_zda(const char* sentence) {
    char time[20] = {0};  // 增加缓冲区大小
    int day = 0, month = 0, year = 0;
    int hour_zone = 0, minute_zone = 0;
    
    // 使用更宽松的格式匹配
    int result = sscanf(sentence, "$GNZDA,%[^,],%d,%d,%d,%d,%d",
        time, &day, &month, &year, &hour_zone, &minute_zone);
    
    // 放宽结果检查条件
    if(result >= 1) {  // 只要能获取到时间就显示
        printf("\n=== ZDA解析结果 ===\n");
        if(strlen(time) >= 6) {
            printf("时间: %c%c:%c%c:%c%c\n",
                time[0], time[1],    // 时
                time[2], time[3],    // 分
                time[4], time[5]);   // 秒
        } else {
            printf("时间: %s\n", time);
        }
        
        if(result >= 4) {
            printf("日期: %04d年%02d月%02d日\n", year, month, day);
        }
        if(result >= 6) {
            printf("时区: UTC %+d:%02d\n", hour_zone, minute_zone);
        }
        printf("================\n");
    }
}

void uart_event_task(void *pvParameters)
{    // 将当前任务添加到看门狗监控
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    uart_event_t event;
    uint8_t* dtmp = (uint8_t*) malloc(BUF_SIZE);
    static char buffer[BUF_SIZE * 2] = {0};
    static int buffer_len = 0;

    for(;;) {
        // 添加延时，让出CPU时间
        vTaskDelay(pdMS_TO_TICKS(5));
        // 重置看门狗计时器
        esp_task_wdt_reset();

        if(xQueueReceive(uart1_queue, (void *)&event, 5 / portTICK_PERIOD_MS)) {
            if(event.type == UART_DATA) {
                bzero(dtmp, BUF_SIZE);
                int len = uart_read_bytes(UART_NUM, dtmp, event.size, 5 / portTICK_PERIOD_MS);
                
                if(len > 0) {
                    // 将新数据添加到缓冲区
                    if(buffer_len + len < sizeof(buffer)) {
                        memcpy(buffer + buffer_len, dtmp, len);
                        buffer_len += len;
                        buffer[buffer_len] = '\0';
                    } else {
                        // 缓冲区满，清空重新开始
                        buffer_len = 0;
                        continue;
                    }
                    
                    // 查找并处理完整的NMEA语句
                    char *start = buffer;
                    char *end;
                    while((end = strstr(start, "\r\n")) != NULL) {
                        *end = '\0';
                        if(strncmp(start, "$GNGGA", 6) == 0) {
                            parse_gga(start);
                        } else if(strncmp(start, "$GNZDA", 6) == 0) {
                            parse_zda(start);
                        }
                        start = end + 2;
                        
                      
                        vTaskDelay(1);
                    }
                    
                    // 移动剩余的数据到缓冲区开始
                    if(start < buffer + buffer_len) {
                        buffer_len -= (start - buffer);
                        memmove(buffer, start, buffer_len);
                    } else {
                        buffer_len = 0;
                    }

                    // 在处理数据的循环中也添加喂狗操作
                    while((end = strstr(start, "\r\n")) != NULL) {
                        esp_task_wdt_reset();  // 在循环中喂狗
                        
                        *end = '\0';
                        if(strncmp(start, "$GNGGA", 6) == 0) {
                            parse_gga(start);
                        } else if(strncmp(start, "$GNZDA", 6) == 0) {
                            parse_zda(start);
                        }
                        start = end + 2;
                        
                        // 增加延时时间，给其他任务更多执行机会
                        vTaskDelay(pdMS_TO_TICKS(5));
                    }
                }
            }
        }
                // 每次循环结束也喂狗
                esp_task_wdt_reset();
    }
    free(dtmp);
    vTaskDelete(NULL);
}

void app_main(void)
{   
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化LCD
    lcd_init();
    
    // 初始化LVGL
    lvgl_init();

    ESP_LOGI("main", "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    // 添加初始化日志
    printf("\n正在初始化北斗模块...\n");

    // 先删除已有的看门狗配置
    esp_task_wdt_deinit();
    
    // 然后重新配置看门狗
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 30000,                // 10秒超时
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,    // 监视所有核心
        .trigger_panic = true              // 超时时触发 panic
    };
    ESP_ERROR_CHECK(esp_task_wdt_init(&twdt_config));

    const uart_config_t uart_config = {
        .baud_rate = 38400,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    // 配置UART参数
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart1_queue, 0));

    // 创建UART事件处理任务，增加堆栈大小
    if (xTaskCreate(uart_event_task, "uart_event_task", 4096, NULL, 12, NULL) == pdPASS) {
        printf("UART事件处理任务创建成功\n");
        printf("等待北斗数据...\n\n");
    } else {
        printf("错误：UART事件处理任务创建失败\n");
    }
    // 将主任务添加到看门狗监控
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    // 主循环中喂狗
    while(1) {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(1000));  // 每秒喂一次狗
        lv_timer_handler();
    }
}