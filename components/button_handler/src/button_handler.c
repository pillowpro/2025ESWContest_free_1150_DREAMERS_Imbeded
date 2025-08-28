#include "button_handler.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "BUTTON_HANDLER";

static button_callback_t button_callback = NULL;
static TaskHandle_t button_task_handle = NULL;
static bool button_initialized = false;
static bool button_running = false;

static void IRAM_ATTR button_isr_handler(void* arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (button_task_handle) {
        vTaskNotifyGiveFromISR(button_task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

static void button_task(void* args)
{
    uint32_t press_start_time = 0;
    bool button_pressed = false;
    bool long_press_triggered = false;
    
    while (button_running) {
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) > 0) {
            uint32_t current_time = esp_timer_get_time() / 1000;
            
            if (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                if (!button_pressed) {
                    button_pressed = true;
                    press_start_time = current_time;
                    long_press_triggered = false;
                    ESP_LOGI(TAG, "Button pressed");
                }
            } else {
                if (button_pressed) {
                    uint32_t press_duration = current_time - press_start_time;
                    button_pressed = false;
                    
                    if (!long_press_triggered) {
                        if (press_duration < LONG_PRESS_TIME_MS) {
                            ESP_LOGI(TAG, "Short press detected (%lu ms)", press_duration);
                            if (button_callback) {
                                button_callback(BUTTON_EVENT_SHORT_PRESS);
                            }
                        } else {
                            ESP_LOGI(TAG, "Long press detected (%lu ms)", press_duration);
                            if (button_callback) {
                                button_callback(BUTTON_EVENT_LONG_PRESS);
                            }
                        }
                    }
                }
            }
        } else {
            if (button_pressed && !long_press_triggered) {
                uint32_t current_time = esp_timer_get_time() / 1000;
                uint32_t press_duration = current_time - press_start_time;
                
                if (press_duration >= LONG_PRESS_TIME_MS) {
                    long_press_triggered = true;
                    ESP_LOGI(TAG, "Long press detected during hold (%lu ms)", press_duration);
                    if (button_callback) {
                        button_callback(BUTTON_EVENT_LONG_PRESS);
                    }
                }
            }
        }
    }
    
    button_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t button_handler_init(void)
{
    if (button_initialized) {
        ESP_LOGW(TAG, "Button handler already initialized");
        return ESP_OK;
    }
    
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = gpio_isr_handler_add(BOOT_BUTTON_GPIO, button_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    button_initialized = true;
    ESP_LOGI(TAG, "Button handler initialized (GPIO%d)", BOOT_BUTTON_GPIO);
    
    return ESP_OK;
}

esp_err_t button_handler_set_callback(button_callback_t callback)
{
    button_callback = callback;
    return ESP_OK;
}

esp_err_t button_handler_start(void)
{
    if (!button_initialized) {
        ESP_LOGE(TAG, "Button handler not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (button_running) {
        ESP_LOGW(TAG, "Button handler already running");
        return ESP_OK;
    }
    
    button_running = true;
    
    BaseType_t ret = xTaskCreate(
        button_task,
        "button_task",
        2048,
        NULL,
        5,
        &button_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button task");
        button_running = false;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Button handler started");
    return ESP_OK;
}

esp_err_t button_handler_stop(void)
{
    if (!button_running) {
        ESP_LOGW(TAG, "Button handler not running");
        return ESP_OK;
    }
    
    button_running = false;
    
    if (button_task_handle) {
        xTaskNotifyGive(button_task_handle);
        while (button_task_handle != NULL) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    
    ESP_LOGI(TAG, "Button handler stopped");
    return ESP_OK;
}
