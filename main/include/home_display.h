#ifndef HOME_DISPLAY_H
#define HOME_DISPLAY_H

#include "api_client.h"

#define TEMPERATURE_DEFAULT "22"
#define WEATHER_DEFAULT "Clear"
#define SLEEP_SCORE_DEFAULT "85"
#define NOISE_LEVEL_DEFAULT "24"
#define ALARM_TIME_DEFAULT "7:20"
#define OFFLINE_DISPLAY "--"

void home_display_update(void);

#endif
