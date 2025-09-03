#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "event_handlers.h"
#include "nextion_hmi.h"
#include "device_cfg.h"
#include "api_client.h"
#include "utils.h"
#include "app_state.h"
#include "home_display.h"
#include "web_server.h"

static const char *TAG = "EVENT_HANDLERS";

static void handle_home_screen_events(nextion_event_t* event)
{
    switch (event->command) {
        case NEXTION_CMD_GO_SETTINGS:
            ESP_LOGI(TAG, "Settings button pressed");
            nextion_show_setup_status("Opening Settings...");
            break;
            
        case NEXTION_CMD_GO_WIFI_LIST:
            ESP_LOGI(TAG, "WiFi list button pressed");
            nextion_show_setup_status("Opening WiFi Settings...");
            break;
            
        case NEXTION_CMD_REFRESH_DATA:
            ESP_LOGI(TAG, "Refresh data button pressed");
            if (strlen(g_app_state.device_id) > 0 && strlen(g_app_state.auth_token) > 0) {
                home_display_update();
            }
            break;
            
        case NEXTION_CMD_FACTORY_RESET:
            ESP_LOGI(TAG, "Factory reset button pressed");
            nextion_show_setup_status("Factory Reset...");
            vTaskDelay(pdMS_TO_TICKS(STATUS_DELAY_MS));
            device_config_factory_reset();
            esp_restart();
            break;
            
        default:
            ESP_LOGI(TAG, "Unknown command from home screen");
            break;
    }
}

void nextion_event_handler(nextion_event_t* event)
{
    ESP_LOGI(TAG, "NEXTION Event: Page %d, Component %d, Command %d", 
             event->page_id, event->component_id, event->command);
    
    if (event->page_id == 1) {
        handle_home_screen_events(event);
    }
}

static esp_err_t provision_device_new_api(const char* provisioning_code)
{
    // Use dynamic allocation to avoid stack overflow
    provisioning_request_t* request = malloc(sizeof(provisioning_request_t));
    provisioning_response_t* response = malloc(sizeof(provisioning_response_t));
    api_response_t* api_response = malloc(sizeof(api_response_t));
    
    if (!request || !response || !api_response) {
        ESP_LOGE(TAG, "Failed to allocate memory for provisioning");
        if (request) free(request);
        if (response) free(response);
        if (api_response) free(api_response);
        return ESP_ERR_NO_MEM;
    }
    
    memset(request, 0, sizeof(provisioning_request_t));
    memset(response, 0, sizeof(provisioning_response_t));
    memset(api_response, 0, sizeof(api_response_t));
    
    char device_id[64];
    char mac_str[32];
    uint8_t mac[6];
    
    utils_get_mac_based_device_id(device_id, sizeof(device_id));
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    strncpy(request->provisioning_code, provisioning_code, sizeof(request->provisioning_code) - 1);
    strncpy(request->device_id, device_id, sizeof(request->device_id) - 1);
    strncpy(request->device_type, "pillow", sizeof(request->device_type) - 1);
    strncpy(request->firmware_version, "1.2.3", sizeof(request->firmware_version) - 1);
    strncpy(request->mac_address, mac_str, sizeof(request->mac_address) - 1);
    
    nextion_show_setup_status("Provisioning Device...");
    
    esp_err_t ret = api_client_provision_device(request, response, api_response);
    if (ret == ESP_OK && api_response->success) {
        ESP_LOGI(TAG, "Device provisioned successfully, received token");
        nextion_show_setup_status("Provisioning Complete");
        device_config_set_provisioned(true);
        
        // Save device token instead of temp token
        device_config_save_device_token(response->device_token);
        
        app_state_set_device_id(device_id);
        app_state_set_device_token(response->device_token);
        
        vTaskDelay(pdMS_TO_TICKS(STATUS_DELAY_MS));
        
        // Change to page 1 after successful provisioning
        nextion_change_page(1);
        app_state_set_home_mode(true);
        
    } else {
        ESP_LOGE(TAG, "Device provisioning failed: %s", api_response->message);
        nextion_show_setup_status("Provisioning Failed");
    }
    
    // Clean up memory
    free(request);
    free(response);
    free(api_response);
    
    return ret;
}

static esp_err_t register_device_if_needed(const char* device_id, const char* token)
{
    api_response_t response;
    nextion_show_setup_status("Registering Device...");
    
    esp_err_t ret = api_client_register_device(device_id, token, &response);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Device registered successfully");
        nextion_show_setup_status("Registration Complete");
        device_config_set_provisioned(true);
        
        app_state_set_device_id(device_id);
        app_state_set_auth_token(token);
        
        vTaskDelay(pdMS_TO_TICKS(STATUS_DELAY_MS));
        app_state_set_home_mode(true);
        home_display_update();
    } else {
        ESP_LOGE(TAG, "Device registration failed: %s", response.message);
        nextion_show_setup_status("Registration Failed");
    }
    
    return ret;
}

static void handle_provisioning_success(void)
{
    ESP_LOGI(TAG, "Provisioning success - WiFi connected, now provisioning device");
    
    char provisioning_code[16];
    device_config_get_provisioning_code(provisioning_code);
    
    // Retry device provisioning up to 3 times
    esp_err_t ret = ESP_FAIL;
    for (int retry = 0; retry < 3; retry++) {
        char status_msg[64];
        snprintf(status_msg, sizeof(status_msg), "Provisioning Device... (Attempt %d/3)", retry + 1);
        nextion_show_setup_status(status_msg);
        
        ret = provision_device_new_api(provisioning_code);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Device provisioning successful on attempt %d", retry + 1);
            break;
        }
        
        ESP_LOGW(TAG, "Device provisioning failed on attempt %d, retrying...", retry + 1);
        vTaskDelay(pdMS_TO_TICKS(2000)); // Wait 2 seconds before retry
    }
    
    // Only reboot if device provisioning was successful
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Device provisioning successful, rebooting to Phase 3");
        nextion_show_setup_status("Registration Complete - Restarting...");
        vTaskDelay(pdMS_TO_TICKS(2000)); // Show message for 2 seconds
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Device provisioning failed after 3 attempts, staying in current phase");
        nextion_show_setup_status("Provisioning Failed - Please restart device");
    }
}

static void handle_wifi_connected(void)
{
    ESP_LOGI(TAG, "WiFi connected successfully");
    nextion_show_setup_status("WiFi Connected");
    
    if (device_config_is_provisioned() && 
        strlen(g_app_state.device_id) > 0 && 
        strlen(g_app_state.auth_token) > 0) {
        vTaskDelay(pdMS_TO_TICKS(STATUS_DELAY_MS));
        app_state_set_home_mode(true);
        home_display_update();
    }
}

void wifi_event_handler(wifi_manager_event_t event, void* data)
{
    switch (event) {
        case WIFI_MGR_EVENT_AP_START:
            ESP_LOGI(TAG, "Provisioning AP started");
            nextion_show_setup_status("AP Mode Started");
            app_state_set_home_mode(false);
            web_server_start();
            break;
            
        case WIFI_MGR_EVENT_STA_CONNECTED:
            nextion_show_setup_status("WiFi Connected Successfully!");
            // Only handle STA connected in Phase 2 (no device_token yet)
            char device_token[512];
            device_config_get_device_token(device_token);
            if (strlen(device_token) == 0) {
                ESP_LOGI(TAG, "WiFi connected in Phase 2, will handle via provisioning success");
                handle_wifi_connected();
            } else {
                ESP_LOGI(TAG, "WiFi connected in Phase 3, handled by NORMAL_MODE_CONNECTED event");
            }
            break;
            
        case WIFI_MGR_EVENT_STA_DISCONNECTED:
            ESP_LOGE(TAG, "WiFi disconnected");
            if (device_config_is_provisioned()) {
                nextion_show_setup_status("WiFi Disconnected - Reconnecting...");
            } else {
                nextion_show_setup_status("WiFi Disconnected");
            }
            app_state_set_home_mode(false);
            break;
            
        case WIFI_MGR_EVENT_PROVISIONING_SUCCESS:
            nextion_show_setup_status("WiFi Provisioning Success!");
            handle_provisioning_success();
            break;
            
        case WIFI_MGR_EVENT_PROVISIONING_FAILED:
            ESP_LOGE(TAG, "Provisioning failed");
            nextion_show_setup_status("WiFi Connection Failed");
            app_state_set_home_mode(false);
            break;
            
        case WIFI_MGR_EVENT_CONNECTION_MAX_RETRIES_FAILED:
            ESP_LOGE(TAG, "WiFi connection failed after maximum retries - performing factory reset");
            nextion_show_setup_status("Connection Failed - Resetting...");
            vTaskDelay(pdMS_TO_TICKS(2000)); // Show message for 2 seconds
            
            // Factory reset and restart to provisioning mode
            device_config_factory_reset();
            esp_restart();
            break;
            
        case WIFI_MGR_EVENT_NORMAL_MODE_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected in normal mode - Phase 3");
            nextion_show_setup_status("WiFi Connected - Normal Mode");
            vTaskDelay(pdMS_TO_TICKS(STATUS_DELAY_MS));
            nextion_change_page(1);
            app_state_set_home_mode(true);
            break;
            
        default:
            break;
    }
}

static esp_err_t validate_and_save_wifi_config(const wifi_credentials_t* credentials)
{
    if (!utils_validate_wifi_credentials(credentials->ssid, credentials->password)) {
        ESP_LOGE(TAG, "Invalid WiFi credentials");
        nextion_show_setup_status("Invalid WiFi Settings");
        return ESP_ERR_INVALID_ARG;
    }
    
    device_config_save_wifi_credentials(credentials->ssid, credentials->password);
    device_config_save_auth_token(credentials->token);
    device_config_save_provisioning_code(credentials->provisioning_code);
    
    // Mark as provisioned for Phase 2
    device_config_set_provisioned(true);
    
    return ESP_OK;
}

void wifi_config_handler(const wifi_credentials_t* credentials)
{
    ESP_LOGI(TAG, "WiFi configuration received from web server");
    ESP_LOGI(TAG, "SSID: %s", credentials->ssid);
    ESP_LOGI(TAG, "Password: [PROVIDED]");
    ESP_LOGI(TAG, "Provisioning Code: %s", credentials->provisioning_code);
    ESP_LOGI(TAG, "Token length: %d", strlen(credentials->token));
    
    // Show received WiFi info on NEXTION display
    char wifi_info_msg[128];
    snprintf(wifi_info_msg, sizeof(wifi_info_msg), "WiFi: %s\nCode: %s", 
             credentials->ssid, credentials->provisioning_code);
    nextion_show_setup_status(wifi_info_msg);
    vTaskDelay(pdMS_TO_TICKS(2000)); // Show info for 2 seconds
    
    if (validate_and_save_wifi_config(credentials) != ESP_OK) {
        return;
    }
    
    // Save WiFi config and reboot to Phase 2 (WiFi connection + device registration)
    ESP_LOGI(TAG, "WiFi config saved, rebooting to Phase 2...");
    nextion_show_setup_status("WiFi Config Saved - Restarting...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
}
