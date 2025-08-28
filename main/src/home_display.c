#include <string.h>
#include "esp_log.h"
#include "home_display.h"
#include "nextion_hmi.h"
#include "app_state.h"
#include "fota_manager.h"

static const char *TAG = "HOME_DISPLAY";

static void extract_home_data_values(const home_data_t* home_data, 
                                   char* temperature, char* weather, 
                                   char* sleep_score, char* noise_level, 
                                   char* alarm_time)
{
    strncpy(temperature, (strlen(home_data->temperature) > 0) ? 
            home_data->temperature : TEMPERATURE_DEFAULT, 15);
    strncpy(weather, (strlen(home_data->weather_info) > 0) ? 
            home_data->weather_info : WEATHER_DEFAULT, 63);
    strncpy(sleep_score, SLEEP_SCORE_DEFAULT, 15);
    strncpy(noise_level, NOISE_LEVEL_DEFAULT, 15);
    strncpy(alarm_time, ALARM_TIME_DEFAULT, 15);
}

void home_display_update(void)
{
    home_data_t home_data;
    api_response_t response;
    
    if (api_client_get_home_data(g_app_state.device_id, g_app_state.auth_token, 
                                &home_data, &response) == ESP_OK && response.success) {
        char temperature[16], weather[64], sleep_score[16], noise_level[16], alarm_time[16];
        extract_home_data_values(&home_data, temperature, weather, sleep_score, noise_level, alarm_time);
        
        nextion_show_home_data(temperature, weather, sleep_score, noise_level, alarm_time);
        ESP_LOGI(TAG, "Home screen updated successfully");
    } else {
        ESP_LOGW(TAG, "Failed to get home data: %s", response.message);
        nextion_show_home_data(OFFLINE_DISPLAY, "Offline", OFFLINE_DISPLAY, 
                              OFFLINE_DISPLAY, "--:--");
    }
    
    // FOTA 상태 확인 및 표시
    if (fota_is_update_in_progress()) {
        fota_status_t fota_status;
        if (fota_get_status(&fota_status) == ESP_OK) {
            switch (fota_status.state) {
                case FOTA_STATE_DOWNLOADING:
                    nextion_show_fota_status("Downloading...");
                    break;
                case FOTA_STATE_VERIFYING:
                    nextion_show_fota_status("Verifying...");
                    break;
                case FOTA_STATE_INSTALLING:
                    nextion_show_fota_status("Installing...");
                    break;
                default:
                    nextion_show_fota_status("FOTA");
                    break;
            }
        } else {
            nextion_show_fota_status("FOTA");
        }
    } else if (fota_is_update_pending()) {
        nextion_show_fota_status("Ready to Update");
    } else {
        nextion_clear_fota_status();
    }
}
