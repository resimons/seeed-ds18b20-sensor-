#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ds18b20_sensor.h"
#include "mqtt_client_manager.h"
#include "wifi_manager.h"

#define COMMAND_POLL_INTERVAL_MS 100
#define HEARTBEAT_INTERVAL_MINUTES 90
#define HEARTBEAT_INTERVAL_MS (HEARTBEAT_INTERVAL_MINUTES * 60 * 1000)
#define SENSOR_READ_INTERVAL_MS (30 * 1000)

static void publish_sensor_reading(int sensor_index, float temperature_celsius);

extern "C" void app_main(void)
{
    wifi_manager_setup();
    mqtt_client_manager_setup();
    mqtt_client_manager_publish_heartbeat();
    ds18b20_sensor_setup();

    uint32_t ms_since_heartbeat = 0;
    uint32_t ms_since_sensor_read = 0;

    while (1) {
        ms_since_heartbeat += COMMAND_POLL_INTERVAL_MS;
        if (ms_since_heartbeat >= HEARTBEAT_INTERVAL_MS) {
            mqtt_client_manager_publish_heartbeat();
            ms_since_heartbeat = 0;
        }

        ms_since_sensor_read += COMMAND_POLL_INTERVAL_MS;
        if (ms_since_sensor_read >= SENSOR_READ_INTERVAL_MS) {
            ds18b20_sensor_read_all(publish_sensor_reading);
            ms_since_sensor_read = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(COMMAND_POLL_INTERVAL_MS));
    }
}

static void publish_sensor_reading(int sensor_index, float temperature_celsius)
{
    mqtt_client_manager_publish_sensor_reading(sensor_index, temperature_celsius);
}
