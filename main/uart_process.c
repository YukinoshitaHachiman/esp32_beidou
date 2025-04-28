#include "uart_process.h"
#include "esp_task_wdt.h"
#include <string.h>

static const char *TAG = "UART_PROCESS";
static QueueHandle_t uart_queue;
static gps_data_t gps_data = {0};

// 度分格式转换为十进制度
static double convert_to_decimal_degrees(const char* dm_str) {
    if (!dm_str || strlen(dm_str) == 0) {
        return 0.0;
    }
    
    double value = atof(dm_str);
    int degrees = (int)(value / 100);
    double minutes = value - (degrees * 100);
    return degrees + (minutes / 60.0);
}

// 解析GGA数据
static void parse_gga(const char* sentence) {
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
        
        gps_data.latitude = decimal_lat;
        gps_data.longitude = decimal_lon;
        gps_data.lat_dir = lat_dir ? lat_dir : 'N';
        gps_data.lon_dir = lon_dir ? lon_dir : 'E';
        gps_data.data_ready = true;
        
        // ESP_LOGI(TAG, "GPS数据更新: %.7f°%c, %.7f°%c", 
        //         decimal_lat, gps_data.lat_dir,
        //         decimal_lon, gps_data.lon_dir);
    }
}

// UART事件处理任务
static void uart_event_task(void *pvParameters) {
    uart_event_t event;
    uint8_t* dtmp = (uint8_t*) malloc(BUF_SIZE);
    static char buffer[BUF_SIZE * 2] = {0};
    static int buffer_len = 0;

    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    for(;;) {
        esp_task_wdt_reset();
        if(xQueueReceive(uart_queue, (void *)&event, 5 / portTICK_PERIOD_MS)) {
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
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    free(dtmp);
    vTaskDelete(NULL);
}

// UART初始化
void uart_init(void) {
    const uart_config_t uart_config = {
        .baud_rate = 38400,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &uart_queue, 0));
    
    ESP_LOGI(TAG, "UART初始化完成");
}

// 启动UART处理任务
void uart_process_task_start(void) {
    if (xTaskCreate(uart_event_task, "uart_event_task", 4096, NULL, 12, NULL) == pdPASS) {
        ESP_LOGI(TAG, "UART事件处理任务创建成功");
    } else {
        ESP_LOGE(TAG, "UART事件处理任务创建失败");
    }
}

// 获取GPS数据
gps_data_t* get_gps_data(void) {
    return &gps_data;
}