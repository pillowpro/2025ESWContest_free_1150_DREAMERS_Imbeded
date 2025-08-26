#include "utils.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static const char *TAG = "UTILS";

esp_err_t utils_get_mac_based_device_id(char* device_id, size_t max_len)
{
    if (!device_id || max_len < 9) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MAC address");
        return ret;
    }
    
    snprintf(device_id, max_len, "%02X%02X%02X%02X", 
             mac[2], mac[3], mac[4], mac[5]);
    
    return ESP_OK;
}

esp_err_t utils_generate_ap_password(const char* device_id, char* password, size_t max_len)
{
    if (!device_id || !password || max_len < 12) {
        return ESP_ERR_INVALID_ARG;
    }
    
    snprintf(password, max_len, "%sPSWR", device_id);
    return ESP_OK;
}

void utils_format_mac_address(const uint8_t* mac, char* formatted_mac)
{
    if (!mac || !formatted_mac) return;
    
    snprintf(formatted_mac, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool utils_validate_wifi_credentials(const char* ssid, const char* password)
{
    if (!ssid || strlen(ssid) == 0 || strlen(ssid) > 32) {
        return false;
    }
    
    if (password && strlen(password) > 64) {
        return false;
    }
    
    return true;
}

esp_err_t utils_url_encode(const char* input, char* output, size_t max_len)
{
    if (!input || !output || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    size_t input_len = strlen(input);
    size_t output_pos = 0;
    
    for (size_t i = 0; i < input_len && output_pos < max_len - 1; i++) {
        char c = input[i];
        
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            output[output_pos++] = c;
        } else {
            if (output_pos + 3 < max_len) {
                snprintf(&output[output_pos], 4, "%%%02X", (unsigned char)c);
                output_pos += 3;
            } else {
                break;
            }
        }
    }
    
    output[output_pos] = '\0';
    return ESP_OK;
}

esp_err_t utils_json_get_string(const char* json, const char* key, char* value, size_t max_len)
{
    if (!json || !key || !value || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char search_key[64];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    const char* key_pos = strstr(json, search_key);
    if (!key_pos) {
        return ESP_ERR_NOT_FOUND;
    }
    
    const char* value_start = key_pos + strlen(search_key);
    while (*value_start == ' ' || *value_start == '\t') value_start++;
    
    if (*value_start != '"') {
        return ESP_ERR_INVALID_STATE;
    }
    value_start++;
    
    const char* value_end = strchr(value_start, '"');
    if (!value_end) {
        return ESP_ERR_INVALID_STATE;
    }
    
    size_t value_len = value_end - value_start;
    if (value_len >= max_len) {
        value_len = max_len - 1;
    }
    
    strncpy(value, value_start, value_len);
    value[value_len] = '\0';
    
    return ESP_OK;
}
