#ifndef DEVICE_CFG_H
#define DEVICE_CFG_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEVICE_ID_LENGTH 8
#define SSID_MAX_LENGTH 32
#define PASSWORD_MAX_LENGTH 64

typedef struct {
    char device_id[DEVICE_ID_LENGTH + 1];
    char wifi_ssid[SSID_MAX_LENGTH + 1];
    char wifi_password[PASSWORD_MAX_LENGTH + 1];
    bool is_provisioned;
    char auth_token[256];
    char provisioning_code[16];
    char device_token[512];
} device_config_t;

esp_err_t device_config_init(void);
esp_err_t device_config_get_device_id(char* device_id);
esp_err_t device_config_save_wifi_credentials(const char* ssid, const char* password);
esp_err_t device_config_get_wifi_credentials(char* ssid, char* password);
esp_err_t device_config_set_provisioned(bool provisioned);
bool device_config_is_provisioned(void);
esp_err_t device_config_save_auth_token(const char* token);
esp_err_t device_config_get_auth_token(char* token);
esp_err_t device_config_save_provisioning_code(const char* code);
esp_err_t device_config_get_provisioning_code(char* code);
esp_err_t device_config_save_device_token(const char* token);
esp_err_t device_config_get_device_token(char* token);
esp_err_t device_config_factory_reset(void);

#ifdef __cplusplus
}
#endif

#endif
