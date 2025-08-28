#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_partition.h"
#include "cJSON.h"
#include "fota_manager.h"
#include "api_client.h"

#define TAG "FOTA_MANAGER"
#define FOTA_TASK_STACK_SIZE 8192
#define FOTA_TASK_PRIORITY 5
#define FOTA_CHECK_INTERVAL_MS (60 * 60 * 1000)

static fota_status_t g_fota_status = {0};
static fota_progress_callback_t g_progress_callback = NULL;
static TaskHandle_t g_fota_task_handle = NULL;
static bool g_fota_initialized = false;
static bool g_fota_update_pending = false;

static const char* FIRMWARE_VERSION = "1.0.0";

static esp_err_t validate_firmware_image(const esp_partition_t* update_partition)
{
    esp_app_desc_t new_app_info;
    if (esp_ota_get_partition_description(update_partition, &new_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);
        
        const esp_app_desc_t* running_app_info = esp_app_get_description();
        if (strcmp(new_app_info.version, running_app_info->version) > 0) {
            ESP_LOGI(TAG, "Firmware validation passed");
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "New version is not newer than current");
            return ESP_ERR_INVALID_VERSION;
        }
    }
    return ESP_FAIL;
}

static void update_progress(uint32_t bytes_downloaded, uint32_t total_bytes)
{
    g_fota_status.bytes_downloaded = bytes_downloaded;
    g_fota_status.total_bytes = total_bytes;
    
    if (total_bytes > 0) {
        g_fota_status.progress_percent = (bytes_downloaded * 100) / total_bytes;
    }
    
    if (g_progress_callback) {
        g_progress_callback(&g_fota_status);
    }
}

static esp_err_t download_and_install_firmware(const char* download_url)
{
    esp_err_t ret = ESP_OK;
    esp_https_ota_handle_t ota_handle = 0;
    const esp_partition_t* update_partition = NULL;
    
    ESP_LOGI(TAG, "Starting OTA update from: %s", download_url);
    
    g_fota_status.state = FOTA_STATE_DOWNLOADING;
    update_progress(0, 0);
    
    esp_http_client_config_t config = {
        .url = download_url,
        .timeout_ms = 30000,
        .keep_alive_enable = true,
    };
    
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    
    ret = esp_https_ota_begin(&ota_config, &ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_begin failed: %s", esp_err_to_name(ret));
        g_fota_status.state = FOTA_STATE_ERROR;
        g_fota_status.last_error = FOTA_ERROR_NETWORK;
        return ret;
    }
    
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition found");
        esp_https_ota_abort(ota_handle);
        g_fota_status.state = FOTA_STATE_ERROR;
        g_fota_status.last_error = FOTA_ERROR_FLASH;
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    
    int total_size = esp_https_ota_get_image_len_read(ota_handle);
    int downloaded = 0;
    
    while (1) {
        ret = esp_https_ota_perform(ota_handle);
        if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        
        downloaded = esp_https_ota_get_image_len_read(ota_handle);
        if (total_size == 0) {
            total_size = downloaded + 100000; // Estimate if size unknown
        }
        update_progress(downloaded, total_size);
        
        ESP_LOGI(TAG, "Downloaded %d of %d bytes", downloaded, total_size);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_perform failed: %s", esp_err_to_name(ret));
        g_fota_status.state = FOTA_STATE_ERROR;
        g_fota_status.last_error = FOTA_ERROR_NETWORK;
        goto cleanup;
    }
    
    g_fota_status.state = FOTA_STATE_VERIFYING;
    
    if (validate_firmware_image(update_partition) != ESP_OK) {
        ESP_LOGE(TAG, "Firmware validation failed");
        g_fota_status.state = FOTA_STATE_ERROR;
        g_fota_status.last_error = FOTA_ERROR_VERIFICATION;
        ret = ESP_ERR_INVALID_VERSION;
        goto cleanup;
    }
    
    g_fota_status.state = FOTA_STATE_INSTALLING;
    
    ret = esp_https_ota_finish(ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_finish failed: %s", esp_err_to_name(ret));
        g_fota_status.state = FOTA_STATE_ERROR;
        g_fota_status.last_error = FOTA_ERROR_FLASH;
        return ret;
    }
    
    g_fota_status.state = FOTA_STATE_COMPLETE;
    g_fota_status.progress_percent = 100;
    g_fota_update_pending = true;
    
    ESP_LOGI(TAG, "OTA update completed successfully. Update will be applied on next restart.");
    return ESP_OK;

cleanup:
    if (ota_handle) {
        esp_https_ota_abort(ota_handle);
    }
    return ret;
}

esp_err_t fota_manager_init(void)
{
    if (g_fota_initialized) {
        return ESP_OK;
    }
    
    memset(&g_fota_status, 0, sizeof(fota_status_t));
    g_fota_status.state = FOTA_STATE_IDLE;
    
    esp_ota_mark_app_valid_cancel_rollback();
    
    g_fota_initialized = true;
    ESP_LOGI(TAG, "FOTA Manager initialized");
    
    return ESP_OK;
}

esp_err_t fota_manager_deinit(void)
{
    if (g_fota_task_handle) {
        vTaskDelete(g_fota_task_handle);
        g_fota_task_handle = NULL;
    }
    
    g_fota_initialized = false;
    return ESP_OK;
}

esp_err_t fota_check_for_updates(const char* device_id, const char* auth_token, fota_info_t* info)
{
    if (!g_fota_initialized || !info) {
        return ESP_ERR_INVALID_ARG;
    }
    
    g_fota_status.state = FOTA_STATE_CHECKING;
    
    api_response_t response;
    esp_err_t ret = api_client_check_firmware_update(device_id, auth_token, &response);
    
    if (ret == ESP_OK && response.success) {
        cJSON* json = cJSON_Parse(response.data);
        if (json) {
            cJSON* version = cJSON_GetObjectItem(json, "version");
            cJSON* url = cJSON_GetObjectItem(json, "download_url");
            cJSON* hash = cJSON_GetObjectItem(json, "sha256");
            cJSON* size = cJSON_GetObjectItem(json, "file_size");
            
            if (version && url && hash && size) {
                strncpy(info->current_version, FIRMWARE_VERSION, FOTA_VERSION_MAX_LEN - 1);
                strncpy(info->available_version, version->valuestring, FOTA_VERSION_MAX_LEN - 1);
                strncpy(info->download_url, url->valuestring, FOTA_URL_MAX_LEN - 1);
                strncpy(info->sha256_hash, hash->valuestring, FOTA_HASH_MAX_LEN - 1);
                info->file_size = size->valueint;
                
                info->update_available = (strcmp(info->current_version, info->available_version) < 0);
                
                ESP_LOGI(TAG, "Current: %s, Available: %s, Update needed: %s",
                        info->current_version, info->available_version,
                        info->update_available ? "Yes" : "No");
            }
            
            cJSON_Delete(json);
        }
    } else {
        g_fota_status.last_error = FOTA_ERROR_SERVER;
    }
    
    g_fota_status.state = FOTA_STATE_IDLE;
    return ret;
}

static void fota_update_task(void* pvParameters)
{
    char* download_url = (char*)pvParameters;
    
    esp_err_t ret = download_and_install_firmware(download_url);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "FOTA download completed. Device will restart in 5 seconds...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "FOTA update failed");
        g_fota_status.state = FOTA_STATE_ERROR;
    }
    
    free(download_url);
    g_fota_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t fota_start_update(const char* device_id, const char* auth_token)
{
    if (!g_fota_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_fota_task_handle != NULL) {
        ESP_LOGW(TAG, "FOTA update already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    
    fota_info_t info;
    esp_err_t ret = fota_check_for_updates(device_id, auth_token, &info);
    
    if (ret != ESP_OK || !info.update_available) {
        ESP_LOGI(TAG, "No update available");
        return ESP_ERR_NOT_FOUND;
    }
    
    char* url_copy = malloc(strlen(info.download_url) + 1);
    if (!url_copy) {
        return ESP_ERR_NO_MEM;
    }
    strcpy(url_copy, info.download_url);
    
    BaseType_t task_ret = xTaskCreate(
        fota_update_task,
        "fota_update",
        FOTA_TASK_STACK_SIZE,
        url_copy,
        FOTA_TASK_PRIORITY,
        &g_fota_task_handle
    );
    
    if (task_ret != pdPASS) {
        free(url_copy);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "FOTA update started");
    return ESP_OK;
}

esp_err_t fota_set_progress_callback(fota_progress_callback_t callback)
{
    g_progress_callback = callback;
    return ESP_OK;
}

esp_err_t fota_get_status(fota_status_t* status)
{
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(status, &g_fota_status, sizeof(fota_status_t));
    return ESP_OK;
}

esp_err_t fota_rollback_if_needed(void)
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGW(TAG, "Pending firmware found, rolling back");
            return esp_ota_mark_app_invalid_rollback_and_reboot();
        }
    }
    
    return ESP_OK;
}

esp_err_t fota_mark_running_partition_valid(void)
{
    return esp_ota_mark_app_valid_cancel_rollback();
}

const char* fota_get_current_version(void)
{
    return FIRMWARE_VERSION;
}

bool fota_is_running_from_factory(void)
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* factory = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    
    return (running == factory);
}

bool fota_is_update_pending(void)
{
    return g_fota_update_pending;
}

void fota_clear_pending_flag(void)
{
    g_fota_update_pending = false;
}

bool fota_is_update_in_progress(void)
{
    return (g_fota_status.state == FOTA_STATE_DOWNLOADING ||
            g_fota_status.state == FOTA_STATE_VERIFYING ||
            g_fota_status.state == FOTA_STATE_INSTALLING);
}
