#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_wifi.h"
#include "esp_event.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_MGR_EVENT_AP_START,
    WIFI_MGR_EVENT_AP_STOP,
    WIFI_MGR_EVENT_STA_CONNECTED,
    WIFI_MGR_EVENT_STA_DISCONNECTED,
    WIFI_MGR_EVENT_PROVISIONING_SUCCESS,
    WIFI_MGR_EVENT_PROVISIONING_FAILED
} wifi_manager_event_t;

typedef void (*wifi_event_callback_t)(wifi_manager_event_t event, void* data);

esp_err_t wifi_manager_init(void);
esp_err_t wifi_manager_start_provisioning(const char* device_id, const char* password);
esp_err_t wifi_manager_stop_provisioning(void);
esp_err_t wifi_manager_connect_wifi(const char* ssid, const char* password);
esp_err_t wifi_manager_set_event_callback(wifi_event_callback_t callback);
bool wifi_manager_is_connected(void);
esp_err_t wifi_manager_get_ip_info(esp_netif_ip_info_t* ip_info);

#ifdef __cplusplus
}
#endif

#endif
