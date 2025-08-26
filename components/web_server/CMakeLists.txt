idf_component_register(
    SRCS "src/web_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server log json esp_netif
)
