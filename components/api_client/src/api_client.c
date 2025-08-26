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
