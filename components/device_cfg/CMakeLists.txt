idf_component_register(
    SRCS "src/device_config.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_system log utils
)
