idf_component_register(
    SRCS "src/fota_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp_https_ota esp_partition app_update log json api_client
)
