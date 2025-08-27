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
#include "nextion_hmi.h"

static const char *TAG = "BAEGAEPRO_MAIN";

static char g_device_id[16] = {0};
static char g_auth_token[256] = {0};
static bool g_home_mode_active = false;

static void update_home_display(const char* device_id, const char* token)
{
    home_data_t home_data;
    api_response_t response;
    
    if (api_client_get_home_data(device_id, token, &home_data, &response) == ESP_OK && response.success) {
        // Extract temperature value (remove °C suffix if present)
        char temperature[16] = "22";
        char weather[64] = "Clear";
        char sleep_score[16] = "85";
        char noise_level[16] = "24";
        char alarm_time[16] = "7:20";
        
        if (strlen(home_data.temperature) > 0) {
            strncpy(temperature, home_data.temperature, sizeof(temperature) - 1);
        }
        
        if (strlen(home_data.weather_info) > 0) {
            strncpy(weather, home_data.weather_info, sizeof(weather) - 1);
        }
        
        // Extract additional data from status_message or use defaults
        if (strlen(home_data.status_message) > 0) {
            // Parse status message for sleep score, noise level, alarm time
            // For now, use defaults - can be enhanced later
        }
        
        nextion_show_home_data(temperature, weather, sleep_score, noise_level, alarm_time);
        ESP_LOGI(TAG, "Home screen updated successfully");
    } else {
        ESP_LOGW(TAG, "Failed to get home data: %s", response.message);
        nextion_show_home_data("--", "Offline", "--", "--", "--:--");
    }
}

static void nextion_event_handler(nextion_event_t* event)
{
    ESP_LOGI(TAG, "NEXTION Event: Page %d, Component %d, Command %d", 
             event->page_id, event->component_id, event->command);
    
    if (event->page_id == 1) { // Home screen events
        switch (event->command) {
            case NEXTION_CMD_GO_SETTINGS:
                ESP_LOGI(TAG, "Settings button pressed");
                nextion_show_setup_status("Opening Settings...");
                // TODO: Implement settings navigation
                break;
                
            case NEXTION_CMD_GO_WIFI_LIST:
                ESP_LOGI(TAG, "WiFi list button pressed");
                nextion_show_setup_status("Opening WiFi Settings...");
                // TODO: Implement WiFi list navigation
                break;
                
            case NEXTION_CMD_REFRESH_DATA:
                ESP_LOGI(TAG, "Refresh data button pressed");
                if (strlen(g_device_id) > 0 && strlen(g_auth_token) > 0) {
                    update_home_display(g_device_id, g_auth_token);
                }
                break;
                
            case NEXTION_CMD_FACTORY_RESET:
                ESP_LOGI(TAG, "Factory reset button pressed");
                nextion_show_setup_status("Factory Reset...");
                vTaskDelay(pdMS_TO_TICKS(2000));
                device_config_factory_reset();
                esp_restart();
                break;
                
            default:
                ESP_LOGI(TAG, "Unknown command from home screen");
                break;
        }
    }
}

static void wifi_event_handler(wifi_manager_event_t event, void* data)
{
    switch (event) {
        case WIFI_MGR_EVENT_AP_START:
            ESP_LOGI(TAG, "Provisioning AP started");
            nextion_show_setup_status("AP Mode Started");
            g_home_mode_active = false;
            web_server_start();
            break;
            
        case WIFI_MGR_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected successfully");
            nextion_show_setup_status("WiFi Connected");
            
            if (device_config_is_provisioned() && strlen(g_device_id) > 0 && strlen(g_auth_token) > 0) {
                vTaskDelay(pdMS_TO_TICKS(2000));
                g_home_mode_active = true;
                update_home_display(g_device_id, g_auth_token);
            }
            break;
            
        case WIFI_MGR_EVENT_PROVISIONING_SUCCESS:
            ESP_LOGI(TAG, "Provisioning success, registering device");
            char device_id[16];
            char token[256];
            
            device_config_get_device_id(device_id);
            device_config_get_auth_token(token);
            
            api_response_t response;
            nextion_show_setup_status("Registering Device...");
            
            if (api_client_register_device(device_id, token, &response) == ESP_OK) {
                ESP_LOGI(TAG, "Device registered successfully");
                nextion_show_setup_status("Registration Complete");
                device_config_set_provisioned(true);
                
                strncpy(g_device_id, device_id, sizeof(g_device_id) - 1);
                strncpy(g_auth_token, token, sizeof(g_auth_token) - 1);
                
                vTaskDelay(pdMS_TO_TICKS(2000));
                g_home_mode_active = true;
                update_home_display(g_device_id, g_auth_token);
            } else {
                ESP_LOGE(TAG, "Device registration failed: %s", response.message);
                nextion_show_setup_status("Registration Failed");
            }
            
            wifi_manager_stop_provisioning();
            web_server_stop();
            break;
            
        case WIFI_MGR_EVENT_PROVISIONING_FAILED:
            ESP_LOGE(TAG, "Provisioning failed");
            nextion_show_setup_status("WiFi Connection Failed");
            g_home_mode_active = false;
            break;
            
        default:
            break;
    }
}

static void wifi_config_handler(const wifi_credentials_t* credentials)
{
    ESP_LOGI(TAG, "WiFi configuration received");
    
    if (!utils_validate_wifi_credentials(credentials->ssid, credentials->password)) {
        ESP_LOGE(TAG, "Invalid WiFi credentials");
        nextion_show_setup_status("Invalid WiFi Settings");
        return;
    }
    
    nextion_show_setup_status("Connecting to WiFi...");
    
    device_config_save_wifi_credentials(credentials->ssid, credentials->password);
    device_config_save_auth_token(credentials->token);
    
    wifi_manager_connect_wifi(credentials->ssid, credentials->password);
}

void app_main(void)
{
    ESP_LOGI(TAG, "BaegaePro Firmware Starting...");
    
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    ESP_ERROR_CHECK(device_config_init());
    ESP_ERROR_CHECK(nextion_hmi_init());
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(web_server_init());
    ESP_ERROR_CHECK(api_client_init());
    
    wifi_manager_set_event_callback(wifi_event_handler);
    web_server_set_callback(wifi_config_handler);
    nextion_set_event_callback(nextion_event_handler);
    
    char device_id[16];
    utils_get_mac_based_device_id(device_id, sizeof(device_id));
    ESP_LOGI(TAG, "Device ID: %s", device_id);
    
    strncpy(g_device_id, device_id, sizeof(g_device_id) - 1);
    
    if (!device_config_is_provisioned()) {
        ESP_LOGI(TAG, "Device not provisioned, showing initial setup");
        
        // Show initial setup message
        nextion_show_initial_setup_message();
        
        // Start provisioning after showing initial message
        vTaskDelay(pdMS_TO_TICKS(3000));
        
        char ap_password[16];
        utils_generate_ap_password(device_id, ap_password, sizeof(ap_password));
        
        char ap_ssid[32];
        snprintf(ap_ssid, sizeof(ap_ssid), "BaeGaePRO-%s", device_id);
        
        wifi_manager_start_provisioning(ap_ssid, ap_password);
        nextion_show_provisioning_info(ap_ssid, ap_password);
        ESP_LOGI(TAG, "Provisioning AP: %s / %s", ap_ssid, ap_password);
    } else {
        ESP_LOGI(TAG, "Device already provisioned, connecting to WiFi");
        nextion_show_setup_status("Connecting to WiFi...");
        
        device_config_get_auth_token(g_auth_token);
        
        char ssid[33], password[65];
        device_config_get_wifi_credentials(ssid, password);
        wifi_manager_connect_wifi(ssid, password);
    }
    
    while (1) {
        if (wifi_manager_is_connected() && device_config_is_provisioned() && g_home_mode_active) {
            char token[256];
            device_config_get_auth_token(token);
            
            api_response_t response;
            if (api_client_send_heartbeat(device_id, token, &response) != ESP_OK) {
                ESP_LOGW(TAG, "Heartbeat failed");
            }
            
            // Update home data every 10 heartbeats (5 minutes)
            static int update_counter = 0;
            if (update_counter % 10 == 0) {
                update_home_display(g_device_id, g_auth_token);
            }
            update_counter++;
        }
        
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
