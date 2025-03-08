#ifndef WIFI_STATION_H
#define WIFI_STATION_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* WiFi station configuration */
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* Function declarations */
void wifi_init_sta(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_STATION_H */