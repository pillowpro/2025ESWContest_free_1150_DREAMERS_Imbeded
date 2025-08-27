idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "." "../include"
    REQUIRES wifi_manager device_cfg web_server api_client utils nextion_hmi
             esp_system esp_wifi esp_event log nvs_flash esp_netif
)
