#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

#include "esp_err.h"

#define SETUP_DELAY_MS 3000

esp_err_t system_components_init(void);
void system_callbacks_setup(void);
void provisioning_mode_start(const char* device_id);
void normal_mode_start(void);

#endif
