#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "wifi_manager.h"
#include "device_cfg.h"
#include "web_server.h"
#include "api_client.h"
#include "utils.h"

static const char *TAG = "BAEGAEPRO_MAIN";

static void wifi_event_handler(wifi_manager_event_t event, void* data)
{
    switch (event) {
        case WIFI_MGR_EVENT_AP_START:
            ESP_LOGI(TAG, "프로비저닝 AP 시작됨");
            web_server_start();
            break;
            
        case WIFI_MGR_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi 연결 성공");
            break;
            
        case WIFI_MGR_EVENT_PROVISIONING_SUCCESS:
            ESP_LOGI(TAG, "프로비저닝 성공, 기기 등록 시도");
            char device_id[16];
            char token[256];
            
            device_config_get_device_id(device_id);
            device_config_get_auth_token(token);
            
            api_response_t response;
            if (api_client_register_device(device_id, token, &response) == ESP_OK) {
                ESP_LOGI(TAG, "기기 등록 성공");
                device_config_set_provisioned(true);
            } else {
                ESP_LOGE(TAG, "기기 등록 실패: %s", response.message);
            }
            
            wifi_manager_stop_provisioning();
            web_server_stop();
            break;
            
        case WIFI_MGR_EVENT_PROVISIONING_FAILED:
            ESP_LOGE(TAG, "프로비저닝 실패");
            break;
            
        default:
            break;
    }
}

static void wifi_config_handler(const wifi_credentials_t* credentials)
{
    ESP_LOGI(TAG, "WiFi 설정 수신됨");
    
    if (!utils_validate_wifi_credentials(credentials->ssid, credentials->password)) {
        ESP_LOGE(TAG, "잘못된 WiFi 자격증명");
        return;
    }
    
    device_config_save_wifi_credentials(credentials->ssid, credentials->password);
    device_config_save_auth_token(credentials->token);
    
    wifi_manager_connect_wifi(credentials->ssid, credentials->password);
}

void app_main(void)
{
    ESP_LOGI(TAG, "BaegaePro 펌웨어 시작");
    
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    ESP_ERROR_CHECK(device_config_init());
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(web_server_init());
    ESP_ERROR_CHECK(api_client_init());
    
    wifi_manager_set_event_callback(wifi_event_handler);
    web_server_set_callback(wifi_config_handler);
    
    char device_id[16];
    utils_get_mac_based_device_id(device_id, sizeof(device_id));
    ESP_LOGI(TAG, "기기 ID: %s", device_id);
    
    if (!device_config_is_provisioned()) {
        ESP_LOGI(TAG, "프로비저닝되지 않음, AP 모드 시작");
        char ap_password[16];
        utils_generate_ap_password(device_id, ap_password, sizeof(ap_password));
        
        char ap_ssid[32];
        snprintf(ap_ssid, sizeof(ap_ssid), "BaeGaePRO-%s", device_id);
        
        wifi_manager_start_provisioning(ap_ssid, ap_password);
        ESP_LOGI(TAG, "프로비저닝 AP: %s / %s", ap_ssid, ap_password);
    } else {
        ESP_LOGI(TAG, "이미 프로비저닝됨, WiFi 연결 시도");
        char ssid[33], password[65];
        device_config_get_wifi_credentials(ssid, password);
        wifi_manager_connect_wifi(ssid, password);
    }
    
    while (1) {
        if (wifi_manager_is_connected() && device_config_is_provisioned()) {
            char token[256];
            device_config_get_auth_token(token);
            
            api_response_t response;
            if (api_client_send_heartbeat(device_id, token, &response) != ESP_OK) {
                ESP_LOGW(TAG, "하트비트 전송 실패");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
