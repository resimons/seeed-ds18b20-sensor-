#pragma once

#include <stdbool.h>

void mqtt_client_manager_setup(void);
void mqtt_client_manager_publish_heartbeat(void);
void mqtt_client_manager_publish_sensor_reading(int sensor_index, float temperature_celsius);
