#include "api_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "API_CLIENT";

static char response_buffer[MAX_HTTP_RESPONSE_BUFFER];

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static int output_len;
    
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (evt->user_data) {
                    memcpy(evt->user_data + output_len, evt->data, evt->data_len);
                    output_len += evt->data_len;
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            output_len = 0;
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t api_client_init(void)
{
    ESP_LOGI(TAG, "API client initialized");
    return ESP_OK;
}

esp_err_t api_client_register_device(const char* device_id, const char* token, api_response_t* response)
{
    if (!device_id || !token || !response) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(response, 0, sizeof(api_response_t));
    memset(response_buffer, 0, sizeof(response_buffer));
    
    char url[256];
    snprintf(url, sizeof(url), "%s/device/new", API_BASE_URL);
    
    cJSON *json = cJSON_CreateObject();
    cJSON *deviceId = cJSON_CreateString(device_id);
    cJSON_AddItemToObject(json, "deviceId", deviceId);
    
    char *json_string = cJSON_Print(json);
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
        .user_data = response_buffer,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    
    char auth_header[300];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", token);
    esp_http_client_set_header(client, "Authorization", auth_header);
    
    esp_http_client_set_post_field(client, json_string, strlen(json_string));
    
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        response->status_code = status_code;
        
        if (status_code == 200) {
            response->success = true;
            strncpy(response->message, "Device registered successfully", sizeof(response->message) - 1);
        } else {
            response->success = false;
            snprintf(response->message, sizeof(response->message), "HTTP error: %d", status_code);
        }
        
        ESP_LOGI(TAG, "Device registration response: %d", status_code);
    } else {
        response->success = false;
        snprintf(response->message, sizeof(response->message), "HTTP request failed: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    free(json_string);
    cJSON_Delete(json);
    
    return err;
}

esp_err_t api_client_get_home_data(const char* device_id, const char* token, home_data_t* home_data, api_response_t* response)
{
    if (!device_id || !token || !home_data || !response) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(response, 0, sizeof(api_response_t));
    memset(home_data, 0, sizeof(home_data_t));
    memset(response_buffer, 0, sizeof(response_buffer));
    
    char url[256];
    snprintf(url, sizeof(url), "%s/home", API_BASE_URL);
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = _http_event_handler,
        .user_data = response_buffer,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    char auth_header[300];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", token);
    esp_http_client_set_header(client, "Authorization", auth_header);
    
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        response->status_code = status_code;
        
        if (status_code == 200) {
            cJSON *json = cJSON_Parse(response_buffer);
            if (json) {
                cJSON *current_time = cJSON_GetObjectItem(json, "currentTime");
                if (current_time && cJSON_IsString(current_time)) {
                    strncpy(home_data->current_time, current_time->valuestring, sizeof(home_data->current_time) - 1);
                }
                
                cJSON *weather = cJSON_GetObjectItem(json, "weather");
                if (weather && cJSON_IsString(weather)) {
                    strncpy(home_data->weather_info, weather->valuestring, sizeof(home_data->weather_info) - 1);
                }
                
                cJSON *temperature = cJSON_GetObjectItem(json, "temperature");
                if (temperature && cJSON_IsString(temperature)) {
                    strncpy(home_data->temperature, temperature->valuestring, sizeof(home_data->temperature) - 1);
                }
                
                cJSON *humidity = cJSON_GetObjectItem(json, "humidity");
                if (humidity && cJSON_IsString(humidity)) {
                    strncpy(home_data->humidity, humidity->valuestring, sizeof(home_data->humidity) - 1);
                }
                
                cJSON *sleep_score = cJSON_GetObjectItem(json, "sleepScore");
                if (sleep_score && cJSON_IsString(sleep_score)) {
                    strncpy(home_data->sleep_score, sleep_score->valuestring, sizeof(home_data->sleep_score) - 1);
                }
                
                cJSON *noise_level = cJSON_GetObjectItem(json, "noiseLevel");
                if (noise_level && cJSON_IsString(noise_level)) {
                    strncpy(home_data->noise_level, noise_level->valuestring, sizeof(home_data->noise_level) - 1);
                }
                
                cJSON *alarm_time = cJSON_GetObjectItem(json, "alarmTime");
                if (alarm_time && cJSON_IsString(alarm_time)) {
                    strncpy(home_data->alarm_time, alarm_time->valuestring, sizeof(home_data->alarm_time) - 1);
                }
                
                cJSON *status = cJSON_GetObjectItem(json, "status");
                if (status && cJSON_IsString(status)) {
                    strncpy(home_data->status_message, status->valuestring, sizeof(home_data->status_message) - 1);
                }
                
                cJSON_Delete(json);
                
                response->success = true;
                strncpy(response->message, "Home data received successfully", sizeof(response->message) - 1);
            } else {
                response->success = false;
                strncpy(response->message, "Invalid JSON response", sizeof(response->message) - 1);
            }
        } else {
            response->success = false;
            snprintf(response->message, sizeof(response->message), "HTTP error: %d", status_code);
        }
        
        ESP_LOGI(TAG, "Home data response: %d", status_code);
    } else {
        response->success = false;
        snprintf(response->message, sizeof(response->message), "HTTP request failed: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "Home data request failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    
    return err;
}

esp_err_t api_client_send_heartbeat(const char* device_id, const char* token, api_response_t* response)
{
    if (!device_id || !token || !response) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(response, 0, sizeof(api_response_t));
    memset(response_buffer, 0, sizeof(response_buffer));
    
    char url[256];
    snprintf(url, sizeof(url), "%s/device/%s/heartbeat", API_BASE_URL, device_id);
    
    cJSON *json = cJSON_CreateObject();
    cJSON *uptime = cJSON_CreateNumber(esp_timer_get_time() / 1000000);
    cJSON_AddItemToObject(json, "uptime", uptime);
    
    char *json_string = cJSON_Print(json);
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
        .user_data = response_buffer,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    
    char auth_header[300];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", token);
    esp_http_client_set_header(client, "Authorization", auth_header);
    
    esp_http_client_set_post_field(client, json_string, strlen(json_string));
    
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        response->status_code = status_code;
        
        if (status_code == 200) {
            response->success = true;
            strncpy(response->message, "Heartbeat sent successfully", sizeof(response->message) - 1);
        } else {
            response->success = false;
            snprintf(response->message, sizeof(response->message), "HTTP error: %d", status_code);
        }
        
        ESP_LOGD(TAG, "Heartbeat response: %d", status_code);
    } else {
        response->success = false;
        snprintf(response->message, sizeof(response->message), "HTTP request failed: %s", esp_err_to_name(err));
        ESP_LOGD(TAG, "Heartbeat failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    free(json_string);
    cJSON_Delete(json);
    
    return err;
}

esp_err_t api_client_update_status(const char* device_id, const char* token, const char* status, api_response_t* response)
{
    if (!device_id || !token || !status || !response) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(response, 0, sizeof(api_response_t));
    memset(response_buffer, 0, sizeof(response_buffer));
    
    char url[256];
    snprintf(url, sizeof(url), "%s/device/%s/status", API_BASE_URL, device_id);
    
    cJSON *json = cJSON_CreateObject();
    cJSON *status_json = cJSON_CreateString(status);
    cJSON_AddItemToObject(json, "status", status_json);
    
    char *json_string = cJSON_Print(json);
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
        .user_data = response_buffer,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    
    char auth_header[300];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", token);
    esp_http_client_set_header(client, "Authorization", auth_header);
    
    esp_http_client_set_post_field(client, json_string, strlen(json_string));
    
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        response->status_code = status_code;
        
        if (status_code == 200) {
            response->success = true;
            strncpy(response->message, "Status updated successfully", sizeof(response->message) - 1);
        } else {
            response->success = false;
            snprintf(response->message, sizeof(response->message), "HTTP error: %d", status_code);
        }
        
        ESP_LOGI(TAG, "Status update response: %d", status_code);
    } else {
        response->success = false;
        snprintf(response->message, sizeof(response->message), "HTTP request failed: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "Status update failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    free(json_string);
    cJSON_Delete(json);
    
    return err;
}
