#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "main_loop.h"
#include "wifi_manager.h"
#include "device_cfg.h"
#include "api_client.h"
#include "app_state.h"
#include "home_display.h"
#include "fota_manager.h"

static const char *TAG = "MAIN_LOOP";

static void send_heartbeat_if_needed(const char* device_id)
{
    char token[256];
    device_config_get_auth_token(token);
    
    api_response_t response;
    if (api_client_send_heartbeat(device_id, token, &response) != ESP_OK) {
        ESP_LOGW(TAG, "Heartbeat failed");
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
            g_app_state.home_mode_active) {
            
            send_heartbeat_if_needed(device_id);
            handle_periodic_updates(device_id);
        }
        
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
    }
}
