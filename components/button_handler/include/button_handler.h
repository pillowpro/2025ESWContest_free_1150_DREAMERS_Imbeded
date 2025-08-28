#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BOOT_BUTTON_GPIO    9
#define LONG_PRESS_TIME_MS  5000

typedef enum {
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_SHORT_PRESS,
    BUTTON_EVENT_LONG_PRESS
} button_event_t;

typedef void (*button_callback_t)(button_event_t event);

esp_err_t button_handler_init(void);
esp_err_t button_handler_set_callback(button_callback_t callback);
esp_err_t button_handler_start(void);
esp_err_t button_handler_stop(void);

#ifdef __cplusplus
}
#endif

#endif
