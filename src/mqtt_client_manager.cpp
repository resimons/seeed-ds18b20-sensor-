#include "mqtt_client_manager.h"
#include "mqtt_broker_config.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "device_identity.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "mqtt_client.h"

#define MQTT_CONNECT_TIMEOUT_MS 10000
#define MQTT_CONNECTED_BIT (1 << 0)
#define MQTT_PAYLOAD_MAX_LEN 160

static const char *TAG = "mqtt_client_manager";
static esp_mqtt_client_handle_t s_client;
static EventGroupHandle_t s_mqtt_event_group;
static QueueHandle_t s_command_queue;

static int8_t get_rssi(void);

void mqtt_client_manager_setup(void)
{
    s_mqtt_event_group = xEventGroupCreate();
    // Length-1 queue holding only the latest desired relay state; xQueueOverwrite
    // means a burst of commands collapses to "apply the most recent one".
    s_command_queue = xQueueCreate(1, sizeof(bool));

    char broker_uri[64];
    snprintf(broker_uri, sizeof(broker_uri), MQTT_BROKER_USE_TLS ? "mqtts://%s:%d" : "mqtt://%s:%d",
             MQTT_BROKER_HOST, MQTT_BROKER_PORT);

    esp_mqtt_client_config_t mqtt_config = {};
    mqtt_config.broker.address.uri = broker_uri;
    mqtt_config.credentials.username = MQTT_BROKER_USERNAME;
    mqtt_config.credentials.authentication.password = MQTT_BROKER_PASSWORD;
#if MQTT_BROKER_USE_TLS
#if MQTT_BROKER_VERIFY_CERTIFICATE
    // Validate the broker's server certificate against this root CA.
    mqtt_config.broker.verification.certificate = MQTT_BROKER_ROOT_CA;
#else
    // No root CA set: esp-tls falls back to MBEDTLS_SSL_VERIFY_NONE, accepting
    // any server certificate. TLS still encrypts the connection either way.
#endif
    // Mutual TLS: present this device's own certificate/key to the broker.
    mqtt_config.credentials.authentication.certificate = MQTT_BROKER_CERTIFICATE;
    mqtt_config.credentials.authentication.key = MQTT_BROKER_PRIVATE_KEY;
#endif

    s_client = esp_mqtt_client_init(&mqtt_config);
    esp_mqtt_client_start(s_client);

    ESP_LOGI(TAG, "connecting to MQTT broker %s", broker_uri);

    EventBits_t bits = xEventGroupWaitBits(s_mqtt_event_group, MQTT_CONNECTED_BIT, pdFALSE, pdFALSE,
                                            pdMS_TO_TICKS(MQTT_CONNECT_TIMEOUT_MS));

    if (bits & MQTT_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to MQTT broker");
    } else {
        ESP_LOGE(TAG, "timed out connecting to MQTT broker %s", broker_uri);
    }
}

void mqtt_client_manager_publish_heartbeat(void)
{
    char device_id[DEVICE_IDENTITY_ID_LEN];
    device_identity_get_id(device_id, sizeof(device_id));

    // esp_timer_get_time() is microseconds since boot; heartbeat uptime is reported in minutes.
    int64_t uptime_minutes = esp_timer_get_time() / 1000000LL / 60LL;

    char payload[MQTT_PAYLOAD_MAX_LEN];
    int payload_len = snprintf(payload, sizeof(payload),
                                "{\"device\":\"%s\",\"device_type\":\"Seeed Studio Xiao ESP32 C6\",\"type\":\"heartbeat\",\"uptime\":%lld,\"rssi\":%d}",
                                device_id, uptime_minutes, get_rssi());

    esp_mqtt_client_publish(s_client, MQTT_TOPIC_HEARTBEAT, payload, payload_len, 1, 0);
    ESP_LOGI(TAG, "published to '%s': %s", MQTT_TOPIC_HEARTBEAT, payload);
}

static int8_t get_rssi(void)
{
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        return 0;
    }
    return ap_info.rssi;
}

void mqtt_client_manager_publish_sensor_reading(int sensor_index, float temperature_celsius)
{
    char device_id[DEVICE_IDENTITY_ID_LEN];
    device_identity_get_id(device_id, sizeof(device_id));

    char payload[MQTT_PAYLOAD_MAX_LEN];
    int payload_len = snprintf(payload, sizeof(payload),
                                "{\"device\":\"%s\",\"sensor\":\"ds18b20\",\"index\":%d,\"temperature\":%.2f}",
                                device_id, sensor_index, temperature_celsius);

    esp_mqtt_client_publish(s_client, MQTT_TOPIC_SENSOR, payload, payload_len, 1, 0);
    ESP_LOGI(TAG, "published to '%s': %s", MQTT_TOPIC_SENSOR, payload);
}
