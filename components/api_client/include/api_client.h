#ifndef API_CLIENT_H
#define API_CLIENT_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define API_BASE_URL "https://pillow.ijw.app/api/v1"
#define MAX_HTTP_RESPONSE_BUFFER 1024

typedef struct {
    char device_id[16];
    char token[256];
} device_registration_request_t;

typedef struct {
    char provisioning_code[16];
    char device_id[64];
    char device_type[32];
    char firmware_version[16];
    char mac_address[32];
} provisioning_request_t;

typedef struct {
    char device_token[512];
    int expires_in;
} provisioning_response_t;

typedef struct {
    uint64_t uptime;
    uint32_t free_memory;
    int wifi_rssi;
    int battery_level;
    int pump_angle;
    float room_temp;
    float room_humidity;
    char last_pump_action[32];
} heartbeat_data_t;

typedef struct {
    char server_time[32];
    char current_time_kr[64];
    char next_home_update[32];
    struct {
        char next_alarm[32];
        char alarm_time_display[32];
        bool enabled;
        bool smart_wake;
    } alarm_info;
    struct {
        int pump_sensitivity;
        int max_angle;
        bool night_mode_enabled;
    } config_updates;
    struct {
        char type[32];
        int pump;
        char reason[64];
    } commands[5];
    int command_count;
} heartbeat_response_t;

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
esp_err_t api_client_provision_device(const provisioning_request_t* request, provisioning_response_t* response, api_response_t* api_response);
esp_err_t api_client_send_heartbeat_v2(const char* device_id, const char* device_token, const heartbeat_data_t* data, heartbeat_response_t* response, api_response_t* api_response);
esp_err_t api_client_register_device(const char* device_id, const char* token, api_response_t* response);
esp_err_t api_client_send_heartbeat(const char* device_id, const char* token, api_response_t* response);
esp_err_t api_client_update_status(const char* device_id, const char* token, const char* status, api_response_t* response);
esp_err_t api_client_get_home_data(const char* device_id, const char* token, home_data_t* home_data, api_response_t* response);
esp_err_t api_client_check_firmware_update(const char* device_id, const char* token, api_response_t* response);

#ifdef __cplusplus
}
#endif

#endif
