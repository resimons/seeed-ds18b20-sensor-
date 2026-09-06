#pragma once

#define DS18B20_SENSOR_MAX_COUNT 3

typedef void (*ds18b20_sensor_reading_cb)(int sensor_index, float temperature_celsius);

void ds18b20_sensor_setup(void);
void ds18b20_sensor_read_all(ds18b20_sensor_reading_cb on_reading);