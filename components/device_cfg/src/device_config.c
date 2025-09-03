#include "device_cfg.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "utils.h"
#include <string.h>

static const char *TAG = "DEVICE_CFG";
static const char *NVS_NAMESPACE = "device_config";

static device_config_t g_device_config = {0};
static bool g_initialized = false;

esp_err_t device_config_init(void)
{
    if (g_initialized) {
        return ESP_OK;
    }
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    nvs_handle_t nvs_handle;
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }
    
    size_t required_size = sizeof(device_config_t);
    ret = nvs_get_blob(nvs_handle, "config", &g_device_config, &required_size);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Device config not found, initializing defaults");
        memset(&g_device_config, 0, sizeof(device_config_t));
        utils_get_mac_based_device_id(g_device_config.device_id, sizeof(g_device_config.device_id));
        g_device_config.is_provisioned = false;
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error reading device config: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }
    
    nvs_close(nvs_handle);
    g_initialized = true;
    
    ESP_LOGI(TAG, "Device config initialized. Device ID: %s", g_device_config.device_id);
    return ESP_OK;
}

esp_err_t device_config_get_device_id(char* device_id)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!device_id) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strcpy(device_id, g_device_config.device_id);
    return ESP_OK;
}

esp_err_t device_config_save_wifi_credentials(const char* ssid, const char* password)
{
    if (!g_initialized || !ssid) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(g_device_config.wifi_ssid, ssid, sizeof(g_device_config.wifi_ssid) - 1);
    g_device_config.wifi_ssid[sizeof(g_device_config.wifi_ssid) - 1] = '\0';
    
    if (password) {
        strncpy(g_device_config.wifi_password, password, sizeof(g_device_config.wifi_password) - 1);
        g_device_config.wifi_password[sizeof(g_device_config.wifi_password) - 1] = '\0';
    } else {
        g_device_config.wifi_password[0] = '\0';
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = nvs_set_blob(nvs_handle, "config", &g_device_config, sizeof(device_config_t));
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t device_config_get_wifi_credentials(char* ssid, char* password)
{
    if (!g_initialized || !ssid) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strcpy(ssid, g_device_config.wifi_ssid);
    if (password) {
        strcpy(password, g_device_config.wifi_password);
    }
    
    return ESP_OK;
}

esp_err_t device_config_set_provisioned(bool provisioned)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    g_device_config.is_provisioned = provisioned;
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = nvs_set_blob(nvs_handle, "config", &g_device_config, sizeof(device_config_t));
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    return ret;
}

bool device_config_is_provisioned(void)
{
    if (!g_initialized) {
        return false;
    }
    
    return g_device_config.is_provisioned;
}

esp_err_t device_config_save_auth_token(const char* token)
{
    if (!g_initialized || !token) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(g_device_config.auth_token, token, sizeof(g_device_config.auth_token) - 1);
    g_device_config.auth_token[sizeof(g_device_config.auth_token) - 1] = '\0';
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = nvs_set_blob(nvs_handle, "config", &g_device_config, sizeof(device_config_t));
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t device_config_get_auth_token(char* token)
{
    if (!g_initialized || !token) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strcpy(token, g_device_config.auth_token);
    return ESP_OK;
}

esp_err_t device_config_factory_reset(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = nvs_erase_all(nvs_handle);
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    
    if (ret == ESP_OK) {
        memset(&g_device_config, 0, sizeof(device_config_t));
        utils_get_mac_based_device_id(g_device_config.device_id, sizeof(g_device_config.device_id));
        g_device_config.is_provisioned = false;
    }
    
    return ret;
}

esp_err_t device_config_save_provisioning_code(const char* code)
{
    if (!g_initialized || !code) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(g_device_config.provisioning_code, code, sizeof(g_device_config.provisioning_code) - 1);
    g_device_config.provisioning_code[sizeof(g_device_config.provisioning_code) - 1] = '\0';
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = nvs_set_blob(nvs_handle, "config", &g_device_config, sizeof(device_config_t));
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t device_config_get_provisioning_code(char* code)
{
    if (!g_initialized || !code) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strcpy(code, g_device_config.provisioning_code);
    return ESP_OK;
}

esp_err_t device_config_save_device_token(const char* token)
{
    if (!g_initialized || !token) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(g_device_config.device_token, token, sizeof(g_device_config.device_token) - 1);
    g_device_config.device_token[sizeof(g_device_config.device_token) - 1] = '\0';
    
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }
    
    ret = nvs_set_blob(nvs_handle, "config", &g_device_config, sizeof(device_config_t));
    if (ret == ESP_OK) {
        ret = nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t device_config_get_device_token(char* token)
{
    if (!g_initialized || !token) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strcpy(token, g_device_config.device_token);
    return ESP_OK;
}
