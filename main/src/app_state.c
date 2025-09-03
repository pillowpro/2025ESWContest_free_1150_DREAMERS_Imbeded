#include <string.h>
#include "app_state.h"

app_state_t g_app_state = {0};

void app_state_init(void)
{
    memset(&g_app_state, 0, sizeof(app_state_t));
}

void app_state_set_device_id(const char* device_id)
{
    strncpy(g_app_state.device_id, device_id, sizeof(g_app_state.device_id) - 1);
}

void app_state_set_auth_token(const char* token)
{
    strncpy(g_app_state.auth_token, token, sizeof(g_app_state.auth_token) - 1);
}

void app_state_set_device_token(const char* token)
{
    strncpy(g_app_state.device_token, token, sizeof(g_app_state.device_token) - 1);
}

void app_state_set_home_mode(bool active)
{
    g_app_state.home_mode_active = active;
}
