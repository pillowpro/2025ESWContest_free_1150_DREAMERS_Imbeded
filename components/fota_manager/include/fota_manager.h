#ifndef FOTA_MANAGER_H
#define FOTA_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"
#include "esp_ota_ops.h"

#define FOTA_VERSION_MAX_LEN 32
#define FOTA_URL_MAX_LEN 256
#define FOTA_HASH_MAX_LEN 65

typedef enum {
    FOTA_STATE_IDLE,
    FOTA_STATE_CHECKING,
    FOTA_STATE_DOWNLOADING,
    FOTA_STATE_VERIFYING,
    FOTA_STATE_INSTALLING,
    FOTA_STATE_COMPLETE,
    FOTA_STATE_ERROR
} fota_state_t;

typedef enum {
    FOTA_ERROR_NONE,
    FOTA_ERROR_NETWORK,
    FOTA_ERROR_SERVER,
    FOTA_ERROR_VERIFICATION,
    FOTA_ERROR_FLASH,
    FOTA_ERROR_INVALID_FIRMWARE
} fota_error_t;

typedef struct {
    char current_version[FOTA_VERSION_MAX_LEN];
    char available_version[FOTA_VERSION_MAX_LEN];
    char download_url[FOTA_URL_MAX_LEN];
    char sha256_hash[FOTA_HASH_MAX_LEN];
    bool update_available;
    uint32_t file_size;
} fota_info_t;

typedef struct {
    fota_state_t state;
    fota_error_t last_error;
    uint32_t bytes_downloaded;
    uint32_t total_bytes;
    uint8_t progress_percent;
} fota_status_t;

typedef void (*fota_progress_callback_t)(fota_status_t* status);

esp_err_t fota_manager_init(void);
esp_err_t fota_manager_deinit(void);

esp_err_t fota_check_for_updates(const char* device_id, const char* auth_token, fota_info_t* info);
esp_err_t fota_start_update(const char* device_id, const char* auth_token);

esp_err_t fota_set_progress_callback(fota_progress_callback_t callback);
esp_err_t fota_get_status(fota_status_t* status);

esp_err_t fota_rollback_if_needed(void);
esp_err_t fota_mark_running_partition_valid(void);

const char* fota_get_current_version(void);
bool fota_is_running_from_factory(void);
bool fota_is_update_pending(void);
void fota_clear_pending_flag(void);
bool fota_is_update_in_progress(void);

#endif
