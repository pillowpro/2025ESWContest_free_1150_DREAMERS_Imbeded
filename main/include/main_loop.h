#ifndef MAIN_LOOP_H
#define MAIN_LOOP_H

#define HEARTBEAT_INTERVAL_MS 30000
#define HOME_UPDATE_FREQUENCY 10
#define FOTA_CHECK_FREQUENCY 120

void main_loop_run(const char* device_id);

#endif
