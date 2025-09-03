#include "nextion_hmi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include <string.h>

static const char* TAG = "NEXTION_HMI";

static bool nextion_initialized = false;
static nextion_event_callback_t event_callback = NULL;
static TaskHandle_t nextion_task_handle = NULL;

static void nextion_task(void* pvParameters)
{
    uint8_t data[NEXTION_BUF_SIZE];
    
    while (1) {
        int len = uart_read_bytes(NEXTION_UART_NUM, data, NEXTION_BUF_SIZE - 1, pdMS_TO_TICKS(100));
        
        if (len > 0) {
            data[len] = '\0';
            
            for (int i = 0; i < len; i++) {
                if (data[i] == 0x65) {
                    if (i + 6 < len && data[i + 3] == 0xFF && data[i + 4] == 0xFF && data[i + 5] == 0xFF) {
                        nextion_event_t event = {0};
                        event.event = NEXTION_EVENT_TOUCH_PRESS;
                        event.page_id = data[i + 1];
                        event.component_id = data[i + 2];
                        
                        // Map component events to commands
                        if (event.page_id == 1) {
                            switch (event.component_id) {
                                case 1:
                                    event.command = NEXTION_CMD_GO_SETTINGS;
                                    break;
                                case 2:
                                    event.command = NEXTION_CMD_GO_WIFI_LIST;
                                    break;
                                case 3:
                                    event.command = NEXTION_CMD_REFRESH_DATA;
                                    break;
                                case 4:
                                    event.command = NEXTION_CMD_FACTORY_RESET;
                                    break;
                                default:
                                    event.command = NEXTION_CMD_NONE;
                                    break;
                            }
                        }
                        
                        if (event_callback) {
                            event_callback(&event);
                        }
                        
                        ESP_LOGI(TAG, "Touch event: Page %d, Component %d, Command %d", 
                                event.page_id, event.component_id, event.command);
                    }
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t nextion_hmi_init(void)
{
    if (nextion_initialized) {
        return ESP_OK;
    }
    
    uart_config_t uart_config = {
        .baud_rate = NEXTION_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    ESP_ERROR_CHECK(uart_param_config(NEXTION_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(NEXTION_UART_NUM, NEXTION_TX_PIN, NEXTION_RX_PIN, 
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(NEXTION_UART_NUM, NEXTION_BUF_SIZE * 2, 0, 0, NULL, 0));
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    nextion_send_command("");
    vTaskDelay(pdMS_TO_TICKS(100));
    
    xTaskCreate(nextion_task, "nextion_task", 2048, NULL, 5, &nextion_task_handle);
    
    nextion_initialized = true;
    
    ESP_LOGI(TAG, "NEXTION HMI initialized successfully (RX:%d, TX:%d)", NEXTION_RX_PIN, NEXTION_TX_PIN);
    
    return ESP_OK;
}

esp_err_t nextion_hmi_deinit(void)
{
    if (!nextion_initialized) {
        return ESP_OK;
    }
    
    if (nextion_task_handle) {
        vTaskDelete(nextion_task_handle);
        nextion_task_handle = NULL;
    }
    
    uart_driver_delete(NEXTION_UART_NUM);
    nextion_initialized = false;
    
    return ESP_OK;
}

esp_err_t nextion_send_command(const char* command)
{
    if (!nextion_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    int len = uart_write_bytes(NEXTION_UART_NUM, command, strlen(command));
    uart_write_bytes(NEXTION_UART_NUM, "\xFF\xFF\xFF", 3);
    
    ESP_LOGD(TAG, "명령 전송: %s", command);
    
    return (len > 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t nextion_set_text(const char* component, const char* text)
{
    char command[256];
    snprintf(command, sizeof(command), "%s.txt=\"%s\"", component, text);
    return nextion_send_command(command);
}

esp_err_t nextion_set_number(const char* component, int value)
{
    char command[256];
    snprintf(command, sizeof(command), "%s.val=%d", component, value);
    return nextion_send_command(command);
}

esp_err_t nextion_change_page(uint8_t page_id)
{
    char command[32];
    snprintf(command, sizeof(command), "page %d", page_id);
    return nextion_send_command(command);
}

esp_err_t nextion_set_event_callback(nextion_event_callback_t callback)
{
    event_callback = callback;
    return ESP_OK;
}

esp_err_t nextion_show_provisioning_message(const char* ssid, const char* password)
{
    return nextion_show_provisioning_info(ssid, password);
}

esp_err_t nextion_show_status(const char* status)
{
    return nextion_set_text("t0", status);
}

// Page 0 - Setup/Configuration Mode Functions
esp_err_t nextion_show_initial_setup_message(void)
{
    esp_err_t ret;
    
    ret = nextion_change_page(0);
    if (ret != ESP_OK) return ret;
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ret = nextion_set_text("t0", "BaegaePro need App Configutation");
    if (ret != ESP_OK) return ret;
    
    ret = nextion_set_text("t1", "Please Open App");
    if (ret != ESP_OK) return ret;
    
    ESP_LOGI(TAG, "Initial setup message displayed");
    
    return ESP_OK;
}

esp_err_t nextion_show_provisioning_info(const char* ssid, const char* password)
{
    esp_err_t ret;
    
    ret = nextion_change_page(0);
    if (ret != ESP_OK) return ret;
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ret = nextion_set_text("t0", "Application Config Mode");
    if (ret != ESP_OK) return ret;
    
    char ap_info[128];
    snprintf(ap_info, sizeof(ap_info), "DEVICE ID : %s", ssid);
    ret = nextion_set_text("t1", ap_info);
    if (ret != ESP_OK) return ret;
    
    ESP_LOGI(TAG, "Provisioning info displayed: %s", ssid);
    
    return ESP_OK;
}

esp_err_t nextion_show_setup_status(const char* status)
{
    esp_err_t ret;
    
    ret = nextion_change_page(0);
    if (ret != ESP_OK) return ret;
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    ret = nextion_set_text("t0", status);
    if (ret != ESP_OK) return ret;
    
    ret = nextion_set_text("t1", "Please wait...");
    if (ret != ESP_OK) return ret;
    
    ESP_LOGI(TAG, "Setup status: %s", status);
    
    return ESP_OK;
}

// Page 1 - Home Mode Functions
esp_err_t nextion_show_home_data(const char* temperature, const char* weather, const char* sleep_score, const char* noise_level, const char* alarm_time)
{
    esp_err_t ret;
    
    ret = nextion_change_page(1);
    if (ret != ESP_OK) return ret;
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Fixed dummy data for home display
    ret = nextion_set_text("t0", "13°C");
    if (ret != ESP_OK) return ret;
    
    ret = nextion_set_text("t1", "--");
    if (ret != ESP_OK) return ret;
    
    ret = nextion_set_text("t2", "--");
    if (ret != ESP_OK) return ret;
    
    ret = nextion_set_text("t3", "--");
    if (ret != ESP_OK) return ret;
    
    ret = nextion_set_text("t4", "--");
    if (ret != ESP_OK) return ret;
    
    // Commented out original dynamic content
    /*
    // t0: Temperature display
    char temp_display[64];
    snprintf(temp_display, sizeof(temp_display), "%s°C", temperature);
    ret = nextion_set_text("t0", temp_display);
    if (ret != ESP_OK) return ret;
    
    // t1: Weather info
    ret = nextion_set_text("t1", weather);
    if (ret != ESP_OK) return ret;
    
    // t2: Sleep score
    char score_display[64];
    snprintf(score_display, sizeof(score_display), "Sleep: %s points", sleep_score);
    ret = nextion_set_text("t2", score_display);
    if (ret != ESP_OK) return ret;
    
    // t3: Environment info
    char env_display[64];
    snprintf(env_display, sizeof(env_display), "Noise: %sdB", noise_level);
    ret = nextion_set_text("t3", env_display);
    if (ret != ESP_OK) return ret;
    
    // t4: Alarm time
    char alarm_display[64];
    snprintf(alarm_display, sizeof(alarm_display), "Alarm: %s", alarm_time);
    ret = nextion_set_text("t4", alarm_display);
    if (ret != ESP_OK) return ret;
    */
    
    ESP_LOGI(TAG, "Home data updated with dummy values: t0=13°C, t1-t4=--");
    
    return ESP_OK;
}

esp_err_t nextion_show_fota_status(const char* status)
{
    esp_err_t ret = nextion_set_text("t49", status);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "FOTA status displayed: %s", status);
    }
    return ret;
}

esp_err_t nextion_clear_fota_status(void)
{
    esp_err_t ret = nextion_set_text("t49", "");
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "FOTA status cleared");
    }
    return ret;
}

esp_err_t nextion_show_heartbeat_data_detailed(const char* current_time_kr, const char* alarm_time_display, float temperature, float humidity)
{
    esp_err_t ret;
    
    ret = nextion_change_page(1);
    if (ret != ESP_OK) return ret;
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Extract time components from current_time_kr
    // Expected format: "2025년 8월 29일 오후 2:30" -> extract "14:30"
    char time_str[16] = "00:00";
    if (current_time_kr) {
        // Look for time pattern like "오전 7:20" or "오후 2:30"
        const char* time_part = strstr(current_time_kr, "오전 ");
        if (!time_part) {
            time_part = strstr(current_time_kr, "오후 ");
        }
        if (time_part) {
            // Skip "오전 " or "오후 " (3 bytes each in UTF-8)
            time_part += 6;
            int hour, minute;
            if (sscanf(time_part, "%d:%d", &hour, &minute) == 2) {
                // Convert to 24-hour format if needed
                if (strstr(current_time_kr, "오후 ") && hour != 12) {
                    hour += 12;
                } else if (strstr(current_time_kr, "오전 ") && hour == 12) {
                    hour = 0;
                }
                snprintf(time_str, sizeof(time_str), "%02d:%02d", hour, minute);
            }
        }
    }
    
    // Extract alarm time components
    // Expected format: "오전 7:20" -> extract hour and minute separately
    char alarm_hour[8] = "00";
    char alarm_minute[8] = "00";
    if (alarm_time_display) {
        const char* alarm_part = strstr(alarm_time_display, "오전 ");
        bool is_pm = false;
        if (!alarm_part) {
            alarm_part = strstr(alarm_time_display, "오후 ");
            is_pm = true;
        }
        if (alarm_part) {
            alarm_part += 6; // Skip "오전 " or "오후 "
            int hour, minute;
            if (sscanf(alarm_part, "%d:%d", &hour, &minute) == 2) {
                // Convert to 24-hour format
                if (is_pm && hour != 12) {
                    hour += 12;
                } else if (!is_pm && hour == 12) {
                    hour = 0;
                }
                snprintf(alarm_hour, sizeof(alarm_hour), "%d", hour);
                snprintf(alarm_minute, sizeof(alarm_minute), "%02d", minute);
            }
        }
    }
    
    // Format temperature and humidity as plain numbers
    char temp_str[16];
    char humidity_str[16];
    snprintf(temp_str, sizeof(temp_str), "%.1f", temperature);
    snprintf(humidity_str, sizeof(humidity_str), "%.0f", humidity);
    
    // Fixed dummy data for heartbeat display
    ret = nextion_set_text("t5", "1:24");
    if (ret != ESP_OK) return ret;
    
    ret = nextion_set_text("t6", "4");
    if (ret != ESP_OK) return ret;
    
    ret = nextion_set_text("t7", "--");
    if (ret != ESP_OK) return ret;
    
    ret = nextion_set_text("t8", "--");
    if (ret != ESP_OK) return ret;
    
    ret = nextion_set_text("t9", "24");
    if (ret != ESP_OK) return ret;
    
    // Commented out original dynamic content
    /*
    // t5: Current time (HH:MM format)
    ret = nextion_set_text("t5", time_str);
    if (ret != ESP_OK) return ret;
    
    // t6: Alarm hour
    ret = nextion_set_text("t6", alarm_hour);
    if (ret != ESP_OK) return ret;
    
    // t7: Temperature (plain number)
    ret = nextion_set_text("t7", temp_str);
    if (ret != ESP_OK) return ret;
    
    // t8: Humidity (plain number)
    ret = nextion_set_text("t8", humidity_str);
    if (ret != ESP_OK) return ret;
    
    // t9: Alarm minute (if available)
    ret = nextion_set_text("t9", alarm_minute);
    if (ret != ESP_OK) return ret;
    */
    
    ESP_LOGI(TAG, "Heartbeat data updated with dummy values: t5=1:24, t6=4, t7-t8=--, t9=24");
    
    return ESP_OK;
}
