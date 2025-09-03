#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_http_server.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char ssid[33];
    char password[65];
    char token[256];
    char provisioning_code[16];
} wifi_credentials_t;

typedef void (*wifi_config_callback_t)(const wifi_credentials_t* credentials);

esp_err_t web_server_init(void);
esp_err_t web_server_start(void);
esp_err_t web_server_stop(void);
esp_err_t web_server_set_callback(wifi_config_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif
