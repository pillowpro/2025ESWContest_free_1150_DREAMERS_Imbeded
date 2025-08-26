#ifndef UTILS_H
#define UTILS_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t utils_get_mac_based_device_id(char* device_id, size_t max_len);
esp_err_t utils_generate_ap_password(const char* device_id, char* password, size_t max_len);
void utils_format_mac_address(const uint8_t* mac, char* formatted_mac);
bool utils_validate_wifi_credentials(const char* ssid, const char* password);
esp_err_t utils_url_encode(const char* input, char* output, size_t max_len);
esp_err_t utils_json_get_string(const char* json, const char* key, char* value, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif
