#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "esp_err.h"
// 错误代码定义
#define ESP_ERR_WIFI_NOT_CONNECT ESP_FAIL
// 定义GPS数据结构
typedef struct {
    char time[20];
    char latitude[20];
    char longitude[20];
    char lat_dir;
    char lon_dir;
    int fix_quality;
    int num_satellites;
    float altitude;
} gps_data_t;


esp_err_t send_gps_data_to_server(const gps_data_t* data);

#endif /* HTTP_CLIENT_H */