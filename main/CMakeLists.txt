idf_component_register(
    SRCS "src/main.c" "src/app_state.c" "src/home_display.c" "src/event_handlers.c" "src/system_init.c" "src/main_loop.c"
    INCLUDE_DIRS "include" "../include"
    REQUIRES wifi_manager device_cfg web_server api_client utils nextion_hmi fota_manager button_handler
             esp_system esp_wifi esp_event log nvs_flash esp_netif
)
