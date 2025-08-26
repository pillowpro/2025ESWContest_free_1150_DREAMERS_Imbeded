idf_component_register(
    SRCS "src/api_client.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client log json esp-tls esp_timer
)
