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

esp_err_t api_client_provision_device(const provisioning_request_t* request, provisioning_response_t* response, api_response_t* api_response)
{
    if (!request || !response || !api_response) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(response, 0, sizeof(provisioning_response_t));
    memset(api_response, 0, sizeof(api_response_t));
    memset(response_buffer, 0, sizeof(response_buffer));
    
    char url[256];
    snprintf(url, sizeof(url), "%s/devices/provision", API_BASE_URL);
    
    cJSON *json = cJSON_CreateObject();
    cJSON *provisioning_code = cJSON_CreateString(request->provisioning_code);
    cJSON *device_id = cJSON_CreateString(request->device_id);
    cJSON *device_type = cJSON_CreateString(request->device_type);
    cJSON *firmware_version = cJSON_CreateString(request->firmware_version);
    cJSON *mac_address = cJSON_CreateString(request->mac_address);
    
    cJSON_AddItemToObject(json, "provisioning_code", provisioning_code);
    cJSON_AddItemToObject(json, "device_id", device_id);
    cJSON_AddItemToObject(json, "device_type", device_type);
    cJSON_AddItemToObject(json, "firmware_version", firmware_version);
    cJSON_AddItemToObject(json, "mac_address", mac_address);
    
    char *json_string = cJSON_Print(json);
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
        .user_data = response_buffer,
        .timeout_ms = 10000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_string, strlen(json_string));
    
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        api_response->status_code = status_code;
        
        if (status_code == 200 || status_code == 201) {
            cJSON *response_json = cJSON_Parse(response_buffer);
            if (response_json) {
                cJSON *success = cJSON_GetObjectItem(response_json, "success");
                if (success && cJSON_IsTrue(success)) {
                    cJSON *data = cJSON_GetObjectItem(response_json, "data");
                    if (data) {
                        cJSON *token = cJSON_GetObjectItem(data, "device_token");
                        if (token && cJSON_IsString(token)) {
                            strncpy(response->device_token, token->valuestring, sizeof(response->device_token) - 1);
                        }
                        
                        cJSON *expires = cJSON_GetObjectItem(data, "expires_in");
                        if (expires && cJSON_IsNumber(expires)) {
                            response->expires_in = expires->valueint;
                        }
                    }
                    api_response->success = true;
                    strncpy(api_response->message, "Device provisioned successfully", sizeof(api_response->message) - 1);
                } else {
                    api_response->success = false;
                    strncpy(api_response->message, "Provisioning failed", sizeof(api_response->message) - 1);
                }
                cJSON_Delete(response_json);
            }
        } else {
            api_response->success = false;
            snprintf(api_response->message, sizeof(api_response->message), "HTTP error: %d", status_code);
        }
        
        ESP_LOGI(TAG, "Provisioning response: %d", status_code);
    } else {
        api_response->success = false;
        snprintf(api_response->message, sizeof(api_response->message), "HTTP request failed: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "Provisioning request failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    free(json_string);
    cJSON_Delete(json);
    
    return err;
}

esp_err_t api_client_send_heartbeat_v2(const char* device_id, const char* device_token, const heartbeat_data_t* data, heartbeat_response_t* response, api_response_t* api_response)
{
    if (!device_id || !device_token || !data || !response || !api_response) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(response, 0, sizeof(heartbeat_response_t));
    memset(api_response, 0, sizeof(api_response_t));
    memset(response_buffer, 0, sizeof(response_buffer));
    
    char url[256];
    snprintf(url, sizeof(url), "%s/devices/%s/heartbeat", API_BASE_URL, device_id);
    
    cJSON *json = cJSON_CreateObject();
    cJSON *uptime = cJSON_CreateNumber(data->uptime);
    cJSON *free_memory = cJSON_CreateNumber(data->free_memory);
    cJSON *wifi_rssi = cJSON_CreateNumber(data->wifi_rssi);
    cJSON *battery_level = cJSON_CreateNumber(data->battery_level);
    cJSON *pump_angle = cJSON_CreateNumber(data->pump_angle);
    cJSON *room_temp = cJSON_CreateNumber(data->room_temp);
    cJSON *room_humidity = cJSON_CreateNumber(data->room_humidity);
    cJSON *last_pump_action = cJSON_CreateString(data->last_pump_action);
    
    cJSON_AddItemToObject(json, "uptime", uptime);
    cJSON_AddItemToObject(json, "free_memory", free_memory);
    cJSON_AddItemToObject(json, "wifi_rssi", wifi_rssi);
    cJSON_AddItemToObject(json, "battery_level", battery_level);
    cJSON_AddItemToObject(json, "pump_angle", pump_angle);
    cJSON_AddItemToObject(json, "room_temp", room_temp);
    cJSON_AddItemToObject(json, "room_humidity", room_humidity);
    cJSON_AddItemToObject(json, "last_pump_action", last_pump_action);
    
    char *json_string = cJSON_Print(json);
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
        .user_data = response_buffer,
        .timeout_ms = 10000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    
    char auth_header[600];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", device_token);
    esp_http_client_set_header(client, "Authorization", auth_header);
    
    esp_http_client_set_post_field(client, json_string, strlen(json_string));
    
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        api_response->status_code = status_code;
        
        if (status_code == 200) {
            cJSON *response_json = cJSON_Parse(response_buffer);
            if (response_json) {
                cJSON *success = cJSON_GetObjectItem(response_json, "success");
                if (success && cJSON_IsTrue(success)) {
                    cJSON *data_obj = cJSON_GetObjectItem(response_json, "data");
                    if (data_obj) {
                        // Parse server time
                        cJSON *server_time = cJSON_GetObjectItem(data_obj, "server_time");
                        if (server_time && cJSON_IsString(server_time)) {
                            strncpy(response->server_time, server_time->valuestring, sizeof(response->server_time) - 1);
                        }
                        
                        // Parse current time KR
                        cJSON *current_time_kr = cJSON_GetObjectItem(data_obj, "current_time_kr");
                        if (current_time_kr && cJSON_IsString(current_time_kr)) {
                            strncpy(response->current_time_kr, current_time_kr->valuestring, sizeof(response->current_time_kr) - 1);
                        }
                        
                        // Parse alarm info
                        cJSON *alarm_info = cJSON_GetObjectItem(data_obj, "alarm_info");
                        if (alarm_info) {
                            cJSON *next_alarm = cJSON_GetObjectItem(alarm_info, "next_alarm");
                            if (next_alarm && cJSON_IsString(next_alarm)) {
                                strncpy(response->alarm_info.next_alarm, next_alarm->valuestring, sizeof(response->alarm_info.next_alarm) - 1);
                            }
                            
                            cJSON *alarm_time_display = cJSON_GetObjectItem(alarm_info, "alarm_time_display");
                            if (alarm_time_display && cJSON_IsString(alarm_time_display)) {
                                strncpy(response->alarm_info.alarm_time_display, alarm_time_display->valuestring, sizeof(response->alarm_info.alarm_time_display) - 1);
                            }
                            
                            cJSON *enabled = cJSON_GetObjectItem(alarm_info, "enabled");
                            if (enabled && cJSON_IsBool(enabled)) {
                                response->alarm_info.enabled = cJSON_IsTrue(enabled);
                            }
                            
                            cJSON *smart_wake = cJSON_GetObjectItem(alarm_info, "smart_wake");
                            if (smart_wake && cJSON_IsBool(smart_wake)) {
                                response->alarm_info.smart_wake = cJSON_IsTrue(smart_wake);
                            }
                        }
                        
                        // Parse commands
                        cJSON *commands = cJSON_GetObjectItem(data_obj, "commands");
                        if (commands && cJSON_IsArray(commands)) {
                            int command_count = cJSON_GetArraySize(commands);
                            response->command_count = (command_count > 5) ? 5 : command_count;
                            
                            for (int i = 0; i < response->command_count; i++) {
                                cJSON *command = cJSON_GetArrayItem(commands, i);
                                if (command) {
                                    cJSON *type = cJSON_GetObjectItem(command, "type");
                                    if (type && cJSON_IsString(type)) {
                                        strncpy(response->commands[i].type, type->valuestring, sizeof(response->commands[i].type) - 1);
                                    }
                                    
                                    cJSON *pump = cJSON_GetObjectItem(command, "pump");
                                    if (pump && cJSON_IsNumber(pump)) {
                                        response->commands[i].pump = pump->valueint;
                                    }
                                    
                                    cJSON *reason = cJSON_GetObjectItem(command, "reason");
                                    if (reason && cJSON_IsString(reason)) {
                                        strncpy(response->commands[i].reason, reason->valuestring, sizeof(response->commands[i].reason) - 1);
                                    }
                                }
                            }
                        }
                    }
                    api_response->success = true;
                    strncpy(api_response->message, "Heartbeat sent successfully", sizeof(api_response->message) - 1);
                } else {
                    api_response->success = false;
                    strncpy(api_response->message, "Heartbeat failed", sizeof(api_response->message) - 1);
                }
                cJSON_Delete(response_json);
            }
        } else {
            api_response->success = false;
            snprintf(api_response->message, sizeof(api_response->message), "HTTP error: %d", status_code);
        }
        
        ESP_LOGD(TAG, "Heartbeat v2 response: %d", status_code);
    } else {
        api_response->success = false;
        snprintf(api_response->message, sizeof(api_response->message), "HTTP request failed: %s", esp_err_to_name(err));
        ESP_LOGD(TAG, "Heartbeat v2 failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    free(json_string);
    cJSON_Delete(json);
    
    return err;
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

esp_err_t api_client_check_firmware_update(const char* device_id, const char* token, api_response_t* response)
{
    if (!device_id || !token || !response) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memset(response, 0, sizeof(api_response_t));
    memset(response_buffer, 0, sizeof(response_buffer));
    
    char url[256];
    snprintf(url, sizeof(url), "%s/firmware/check/%s", API_BASE_URL, device_id);
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = _http_event_handler,
        .user_data = response_buffer,
        .timeout_ms = 10000,
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
            response->success = true;
            strncpy(response->message, "Firmware check successful", sizeof(response->message) - 1);
            strncpy(response->data, response_buffer, sizeof(response->data) - 1);
        } else {
            response->success = false;
            snprintf(response->message, sizeof(response->message), "HTTP error: %d", status_code);
        }
        
        ESP_LOGI(TAG, "Firmware check response: %d", status_code);
    } else {
        response->success = false;
        snprintf(response->message, sizeof(response->message), "HTTP request failed: %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "Firmware check failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    
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
