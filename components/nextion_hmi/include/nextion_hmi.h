#ifndef NEXTION_HMI_H
#define NEXTION_HMI_H

#include "esp_err.h"
#include "driver/uart.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEXTION_UART_NUM UART_NUM_1
#define NEXTION_RX_PIN 13
#define NEXTION_TX_PIN 12
#define NEXTION_BAUD_RATE 9600
#define NEXTION_BUF_SIZE 1024

typedef enum {
    NEXTION_EVENT_NONE = 0x00,
    NEXTION_EVENT_TOUCH_PRESS = 0x65,
    NEXTION_EVENT_TOUCH_RELEASE = 0x66,
    NEXTION_EVENT_CURRENT_PAGE = 0x67,
    NEXTION_EVENT_STRING_DATA = 0x70
} nextion_event_type_t;

typedef enum {
    NEXTION_CMD_NONE = 0,
    NEXTION_CMD_GO_SETTINGS = 1,
    NEXTION_CMD_GO_WIFI_LIST = 2,
    NEXTION_CMD_REFRESH_DATA = 3,
    NEXTION_CMD_FACTORY_RESET = 4
} nextion_command_type_t;

typedef struct {
    nextion_event_type_t event;
    uint8_t page_id;
    uint8_t component_id;
    uint8_t touch_event;
    nextion_command_type_t command;
    char string_data[256];
} nextion_event_t;

typedef void (*nextion_event_callback_t)(nextion_event_t* event);

esp_err_t nextion_hmi_init(void);
esp_err_t nextion_hmi_deinit(void);
esp_err_t nextion_send_command(const char* command);
esp_err_t nextion_set_text(const char* component, const char* text);
esp_err_t nextion_set_number(const char* component, int value);
esp_err_t nextion_change_page(uint8_t page_id);
esp_err_t nextion_set_event_callback(nextion_event_callback_t callback);
esp_err_t nextion_show_provisioning_message(const char* ssid, const char* password);
esp_err_t nextion_show_status(const char* status);

// Page 0 - Setup/Configuration Mode
esp_err_t nextion_show_initial_setup_message(void);
esp_err_t nextion_show_provisioning_info(const char* ssid, const char* password);
esp_err_t nextion_show_setup_status(const char* status);

// Page 1 - Home Mode
esp_err_t nextion_show_home_data(const char* temperature, const char* weather, const char* sleep_score, const char* noise_level, const char* alarm_time);

#ifdef __cplusplus
}
#endif

#endif
