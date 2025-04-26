#ifndef UART_PROCESS_H
#define UART_PROCESS_H

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"

#define UART_NUM UART_NUM_1
#define RX_PIN 18
#define TX_PIN 19
#define BUF_SIZE 1024

// GPS数据结构
typedef struct {
    double latitude;
    double longitude;
    char lat_dir;
    char lon_dir;
    bool data_ready;
} gps_data_t;

// 对外接口函数声明
void uart_init(void);
void uart_process_task_start(void);
gps_data_t* get_gps_data(void);

#endif // UART_PROCESS_H