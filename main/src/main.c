#include "esp_log.h"
#include "utils.h"
#include "device_cfg.h"
#include "app_state.h"
#include "system_init.h"
#include "main_loop.h"

#define TAG "BAEGAEPRO_MAIN"

void app_main(void)
{
    ESP_LOGI(TAG, "BaegaePro Firmware Starting...");
    
    app_state_init();
    system_components_init();
    system_callbacks_setup();
    
    char device_id[16];
    utils_get_mac_based_device_id(device_id, sizeof(device_id));
    ESP_LOGI(TAG, "Device ID: %s", device_id);
    
    app_state_set_device_id(device_id);
    
    if (!device_config_is_provisioned()) {
        // Phase 1: AP mode + Web server for WiFi config
        ESP_LOGI(TAG, "Phase 1: Starting provisioning AP mode");
        provisioning_mode_start(device_id);
    } else {
        char device_token[512];
        device_config_get_device_token(device_token);
        
        if (strlen(device_token) == 0) {
            // Phase 2: WiFi connection + Device registration
            ESP_LOGI(TAG, "Phase 2: WiFi connection and device registration");
            phase2_wifi_connect_and_register(device_id);
        } else {
            // Phase 3: Normal mode
            ESP_LOGI(TAG, "Phase 3: Normal mode");
            normal_mode_start();
        }
    }
    
    main_loop_run(device_id);
}
