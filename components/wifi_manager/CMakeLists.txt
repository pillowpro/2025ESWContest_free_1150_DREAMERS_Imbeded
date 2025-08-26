idf_component_register(
    SRCS "src/wifi_manager.c"
         "src/wifi_ap.c"
         "src/wifi_sta.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event log nvs_flash esp_netif
)
