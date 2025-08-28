#ifndef EVENT_HANDLERS_H
#define EVENT_HANDLERS_H

#include "wifi_manager.h"
#include "web_server.h"
#include "nextion_hmi.h"

#define STATUS_DELAY_MS 2000

void nextion_event_handler(nextion_event_t* event);
void wifi_event_handler(wifi_manager_event_t event, void* data);
void wifi_config_handler(const wifi_credentials_t* credentials);

#endif
