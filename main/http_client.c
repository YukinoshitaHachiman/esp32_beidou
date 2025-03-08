#include "http_client.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_log.h"

#define TAG "HTTP_CLIENT"
#define SERVER_URL "http://192.168.172.214:8080/api/gps"
//#define SERVER_URL "http://192.168.31.13:8080/api/gps"
//#define SERVER_URL "http://210.35.56.181:8080/api/gps"
esp_err_t send_gps_data_to_server(const gps_data_t* data)
{
    esp_http_client_config_t config = {
        .url = SERVER_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "time", data->time);
    cJSON_AddStringToObject(root, "latitude", data->latitude);
    cJSON_AddStringToObject(root, "longitude", data->longitude);
    cJSON_AddNumberToObject(root, "fix_quality", data->fix_quality);
    cJSON_AddNumberToObject(root, "satellites", data->num_satellites);
    cJSON_AddNumberToObject(root, "altitude", data->altitude);
    
    char *post_data = cJSON_PrintUnformatted(root);
    ESP_LOGI(TAG, "Sending data: %s", post_data);
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d", status_code);
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    
    cJSON_Delete(root);
    free(post_data);
    esp_http_client_cleanup(client);
    
    return err;
}