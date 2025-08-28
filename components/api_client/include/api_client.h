#ifndef API_CLIENT_H
#define API_CLIENT_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define API_BASE_URL "https://baegaepro.ncloud.sbs/api/v1"
#define MAX_HTTP_RESPONSE_BUFFER 1024

typedef struct {
    char device_id[16];
    char token[256];
} device_registration_request_t;

typedef struct {
    char current_time[64];
    char weather_info[128];
    char temperature[16];
    char humidity[16];
    char sleep_score[16];
    char noise_level[16];
    char alarm_time[16];
    char status_message[128];
} home_data_t;

typedef struct {
    bool success;
    char message[256];
    char data[512];
    int status_code;
} api_response_t;

esp_err_t api_client_init(void);
esp_err_t api_client_register_device(const char* device_id, const char* token, api_response_t* response);
esp_err_t api_client_send_heartbeat(const char* device_id, const char* token, api_response_t* response);
esp_err_t api_client_update_status(const char* device_id, const char* token, const char* status, api_response_t* response);
esp_err_t api_client_get_home_data(const char* device_id, const char* token, home_data_t* home_data, api_response_t* response);
esp_err_t api_client_check_firmware_update(const char* device_id, const char* token, api_response_t* response);

#ifdef __cplusplus
}
#endif

#endif
