#include "ds18b20_sensor.h"

#include "ds18b20.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "onewire_bus.h"
#include "onewire_device.h"

#define DS18B20_ONEWIRE_BUS_GPIO GPIO_NUM_21
#define DS18B20_ONEWIRE_MAX_RX_BYTES 10

static const char *TAG = "ds18b20_sensor";
static onewire_bus_handle_t s_bus;
static ds18b20_device_handle_t s_devices[DS18B20_SENSOR_MAX_COUNT];
static int s_device_count;

void ds18b20_sensor_setup(void)
{
    onewire_bus_config_t bus_config = {};
    bus_config.bus_gpio_num = DS18B20_ONEWIRE_BUS_GPIO;
    bus_config.flags.en_pull_up = true;

    onewire_bus_rmt_config_t rmt_config = {};
    rmt_config.max_rx_bytes = DS18B20_ONEWIRE_MAX_RX_BYTES;

    ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_config, &rmt_config, &s_bus));

    onewire_device_iter_handle_t iter;
    ESP_ERROR_CHECK(onewire_new_device_iter(s_bus, &iter));

    onewire_device_t device;
    esp_err_t search_result;
    do {
        search_result = onewire_device_iter_get_next(iter, &device);
        if (search_result != ESP_OK) {
            continue;
        }

        ds18b20_config_t sensor_config = {};
        if (ds18b20_new_device_from_enumeration(&device, &sensor_config, &s_devices[s_device_count]) == ESP_OK) {
            ESP_LOGI(TAG, "found DS18B20[%d], address: %016llX", s_device_count,
                     (unsigned long long)device.address);
            s_device_count++;
            if (s_device_count >= DS18B20_SENSOR_MAX_COUNT) {
                break;
            }
        } else {
            ESP_LOGW(TAG, "ignoring unknown 1-Wire device, address: %016llX", (unsigned long long)device.address);
        }
    } while (search_result != ESP_ERR_NOT_FOUND);
    ESP_ERROR_CHECK(onewire_del_device_iter(iter));

    ESP_LOGI(TAG, "found %d DS18B20 sensor(s) on GPIO%d", s_device_count, DS18B20_ONEWIRE_BUS_GPIO);

    for (int i = 0; i < s_device_count; i++) {
        ESP_ERROR_CHECK(ds18b20_set_resolution(s_devices[i], DS18B20_RESOLUTION_12B));
    }
}

void ds18b20_sensor_read_all(ds18b20_sensor_reading_cb on_reading)
{
    esp_err_t err = ds18b20_trigger_temperature_conversion_for_all(s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to trigger temperature conversion: %s", esp_err_to_name(err));
        return;
    }

    for (int i = 0; i < s_device_count; i++) {
        float temperature;
        err = ds18b20_get_temperature(s_devices[i], &temperature);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "failed to read DS18B20[%d]: %s", i, esp_err_to_name(err));
            continue;
        }
        on_reading(i, temperature);
    }
}