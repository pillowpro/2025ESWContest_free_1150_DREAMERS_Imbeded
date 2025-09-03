#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "main_loop.h"
#include "wifi_manager.h"
#include "device_cfg.h"
#include "api_client.h"
#include "app_state.h"
#include "home_display.h"
#include "fota_manager.h"
#include "nextion_hmi.h"

static const char *TAG = "MAIN_LOOP";

static void send_heartbeat_if_needed(const char* device_id)
{
    char device_token[512];
    device_config_get_device_token(device_token);
    
    if (strlen(device_token) == 0) {
        ESP_LOGW(TAG, "No device token available for heartbeat");
        return;
    }
    
    // Prepare heartbeat data with dummy values as requested
    heartbeat_data_t heartbeat_data = {0};
    heartbeat_data.uptime = esp_timer_get_time() / 1000000; // seconds
    heartbeat_data.free_memory = esp_get_free_heap_size();
    heartbeat_data.wifi_rssi = -45; // dummy value
    heartbeat_data.battery_level = 85; // dummy value
    heartbeat_data.pump_angle = 5; // pump_level as per API spec
    heartbeat_data.room_temp = 24.2f; // dummy value
    heartbeat_data.room_humidity = 58.0f; // dummy value
    strcpy(heartbeat_data.last_pump_action, "2025-08-29T03:25:12+09:00"); // dummy value
    
    heartbeat_response_t response;
    api_response_t api_response;
    
    esp_err_t ret = api_client_send_heartbeat_v2(device_id, device_token, &heartbeat_data, &response, &api_response);
    
    if (ret == ESP_OK && api_response.success) {
        ESP_LOGI(TAG, "Heartbeat sent successfully");
        
        // Update display with response data using detailed formatting
        nextion_show_heartbeat_data_detailed(
            response.current_time_kr,
            response.alarm_info.alarm_time_display,
            heartbeat_data.room_temp,
            heartbeat_data.room_humidity
        );
        
        ESP_LOGI(TAG, "Display updated with heartbeat response");
    } else {
        ESP_LOGW(TAG, "Heartbeat failed: %s", api_response.message);
    }
}

static void fota_progress_callback(fota_status_t* status)
{
    ESP_LOGI(TAG, "FOTA Progress: %d%% - State: %d", status->progress_percent, status->state);
    
    // 진행 상황을 홈 화면에 즉시 업데이트
    home_display_update();
}

static void check_fota_updates(const char* device_id)
{
    char token[256];
    device_config_get_auth_token(token);
    
    fota_info_t fota_info;
    esp_err_t ret = fota_check_for_updates(device_id, token, &fota_info);
    
    if (ret == ESP_OK && fota_info.update_available) {
        ESP_LOGI(TAG, "FOTA update available: %s -> %s", 
                fota_info.current_version, fota_info.available_version);
        
        esp_err_t update_ret = fota_start_update(device_id, token);
        if (update_ret == ESP_OK) {
            ESP_LOGI(TAG, "FOTA update started");
        } else {
            ESP_LOGW(TAG, "Failed to start FOTA update: %s", esp_err_to_name(update_ret));
        }
    } else if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to check FOTA updates: %s", esp_err_to_name(ret));
    }
}

static void handle_periodic_updates(const char* device_id)
{
    static int update_counter = 0;
    
    if (update_counter % HOME_UPDATE_FREQUENCY == 0) {
        home_display_update();
    }
    
    if (update_counter % FOTA_CHECK_FREQUENCY == 0) {
        check_fota_updates(device_id);
    }
    
    update_counter++;
}

void main_loop_run(const char* device_id)
{
    // FOTA 진행 상황 콜백 설정
    fota_set_progress_callback(fota_progress_callback);
    
    while (1) {
        if (wifi_manager_is_connected() && 
            device_config_is_provisioned() && 
            g_app_state.home_mode_active &&
            strlen(g_app_state.device_token) > 0) {
            
            send_heartbeat_if_needed(device_id);
            handle_periodic_updates(device_id);
        }
        
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
    }
}
