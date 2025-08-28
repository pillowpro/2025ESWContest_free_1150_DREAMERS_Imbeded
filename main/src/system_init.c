#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "system_init.h"
#include "device_cfg.h"
#include "nextion_hmi.h"
#include "wifi_manager.h"
#include "web_server.h"
#include "api_client.h"
#include "event_handlers.h"
#include "utils.h"
#include "app_state.h"
#include "fota_manager.h"
#include "button_handler.h"

static const char *TAG = "SYSTEM_INIT";

esp_err_t system_components_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    ESP_ERROR_CHECK(device_config_init());
    ESP_ERROR_CHECK(nextion_hmi_init());
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(web_server_init());
    ESP_ERROR_CHECK(api_client_init());
    ESP_ERROR_CHECK(fota_manager_init());
    ESP_ERROR_CHECK(button_handler_init());
    
    return ESP_OK;
}

static void button_event_handler(button_event_t event)
{
    switch (event) {
        case BUTTON_EVENT_SHORT_PRESS:
            ESP_LOGI(TAG, "BOOT button short press");
            break;
        case BUTTON_EVENT_LONG_PRESS:
            ESP_LOGI(TAG, "BOOT button long press - Factory reset!");
            nextion_change_page(0);  // Page 0으로 이동
            device_config_factory_reset();
            nextion_show_setup_status("Factory reset! Restarting...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart();
            break;
        default:
            break;
    }
}

void system_callbacks_setup(void)
{
    wifi_manager_set_event_callback(wifi_event_handler);
    web_server_set_callback(wifi_config_handler);
    nextion_set_event_callback(nextion_event_handler);
    button_handler_set_callback(button_event_handler);
    button_handler_start();
}

static void generate_ap_credentials(const char* device_id, char* ap_ssid, char* ap_password)
{
    utils_generate_ap_password(device_id, ap_password, 16);
    snprintf(ap_ssid, 32, "BaeGaePRO-%s", device_id);
}

void provisioning_mode_start(const char* device_id)
{
    ESP_LOGI(TAG, "Device not provisioned, showing initial setup");
    
    nextion_show_initial_setup_message();
    vTaskDelay(pdMS_TO_TICKS(SETUP_DELAY_MS));
    
    char ap_password[16];
    char ap_ssid[32];
    generate_ap_credentials(device_id, ap_ssid, ap_password);
    
    wifi_manager_start_provisioning(ap_ssid, ap_password);
    nextion_show_provisioning_info(ap_ssid, ap_password);
    ESP_LOGI(TAG, "Provisioning AP: %s / %s", ap_ssid, ap_password);
}

void normal_mode_start(void)
{
    ESP_LOGI(TAG, "Device already provisioned, connecting to WiFi");
    nextion_show_setup_status("Connecting to WiFi...");
    
    device_config_get_auth_token(g_app_state.auth_token);
    
    char ssid[33], password[65];
    device_config_get_wifi_credentials(ssid, password);
    wifi_manager_connect_wifi(ssid, password);
}
