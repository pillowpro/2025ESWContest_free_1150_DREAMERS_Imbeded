#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdbool.h>

#define DEVICE_ID_MAX_LEN 16
#define AUTH_TOKEN_MAX_LEN 256
#define DEVICE_TOKEN_MAX_LEN 512

typedef struct {
    char device_id[DEVICE_ID_MAX_LEN];
    char auth_token[AUTH_TOKEN_MAX_LEN];
    char device_token[DEVICE_TOKEN_MAX_LEN];
    bool home_mode_active;
} app_state_t;

extern app_state_t g_app_state;

void app_state_init(void);
void app_state_set_device_id(const char* device_id);
void app_state_set_auth_token(const char* token);
void app_state_set_device_token(const char* token);
void app_state_set_home_mode(bool active);

#endif
