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
        provisioning_mode_start(device_id);
    } else {
        normal_mode_start();
    }
    
    main_loop_run(device_id);
}
