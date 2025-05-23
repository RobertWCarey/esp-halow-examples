/*
 * Copyright 2025 Robert Carey
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

#include "mm_app_common.h"
#include "sensor.h"

static const char *TAG = "main";

/* Macro for cJSON error checking */
#define CJSON_CHECK(x)                                                 \
    do                                                                 \
    {                                                                  \
        if ((x) == NULL)                                               \
        {                                                              \
            ESP_LOGE(TAG, "cJSON error at %s:%d", __FILE__, __LINE__); \
            goto exit;                                                 \
        }                                                              \
    } while (0)

/** Device state structure. */
struct device
{
    /** MQTT client handle. */
    esp_mqtt_client_handle_t client;
    /** Timer handle for periodic updates. */
    esp_timer_handle_t update_timer;
};

/** Publish interval (in milliseconds) from configuration */
#define UPDATE_INTERVAL_MS CONFIG_UPDATE_INTERVAL_MS

/** MQTT discovery topic format used for Home Assistant integration. */
#define DISCOVERY_TOPIC_FORMAT "homeassistant/device/%s/config"

/** MQTT state topic format used to publish sensor data. */
#define STATE_TOPIC_FORMAT "%s/state"

/** Home Assistant status topic for birth and last will messages. */
#define HA_STATUS_TOPIC "homeassistant/status"

/** Application version string */
#define APP_VERSION "0.1.0"

/**
 * Get the device ID string. Generates a 12-character uppercase hex string without separators.
 *
 * @param[out] device_id_buf Buffer to store the device ID.
 * @param[in] buf_len Length of the buffer; must be >= 13 bytes.
 */
static void get_device_id(char *device_id_buf, size_t buf_len)
{
    assert(buf_len >= 13);

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));

    snprintf(device_id_buf, buf_len, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3],
             mac[4], mac[5]);
}

/**
 * Generate MQTT discovery components.
 *
 * @param[in,out] cmps The "cmps" cJSON object to add the sensor to.
 * @param[in] device_id Unique device ID string.
 * @return ESP_OK on success, ESP_FAIL on error
 */
static esp_err_t generate_components(cJSON *cmps, const char *device_id)
{
    char name_buf[64];
    cJSON *temperature_sensor = NULL;
    cJSON *humidity_sensor = NULL;

    /* Temperature */
    snprintf(name_buf, sizeof(name_buf), "%s_temperature", device_id);

    temperature_sensor = cJSON_CreateObject();
    CJSON_CHECK(temperature_sensor);
    cJSON_AddItemToObject(cmps, "temperature", temperature_sensor);

    CJSON_CHECK(cJSON_AddStringToObject(temperature_sensor, "p", "sensor"));
    CJSON_CHECK(cJSON_AddStringToObject(temperature_sensor, "device_class", "temperature"));
    CJSON_CHECK(cJSON_AddStringToObject(temperature_sensor, "unit_of_measurement", "°C"));
    CJSON_CHECK(cJSON_AddStringToObject(temperature_sensor, "value_template",
                                        "{{ value_json.temperature }}"));
    CJSON_CHECK(cJSON_AddNumberToObject(temperature_sensor, "suggested_display_precision", 2));
    CJSON_CHECK(cJSON_AddStringToObject(temperature_sensor, "unique_id", name_buf));
    CJSON_CHECK(cJSON_AddStringToObject(temperature_sensor, "object_id", name_buf));

    /* Humidity */
    snprintf(name_buf, sizeof(name_buf), "%s_humidity", device_id);

    humidity_sensor = cJSON_CreateObject();
    CJSON_CHECK(humidity_sensor);
    cJSON_AddItemToObject(cmps, "humidity", humidity_sensor);

    CJSON_CHECK(cJSON_AddStringToObject(humidity_sensor, "p", "sensor"));
    CJSON_CHECK(cJSON_AddStringToObject(humidity_sensor, "device_class", "humidity"));
    CJSON_CHECK(cJSON_AddStringToObject(humidity_sensor, "unit_of_measurement", "%"));
    CJSON_CHECK(
        cJSON_AddStringToObject(humidity_sensor, "value_template", "{{ value_json.humidity }}"));
    CJSON_CHECK(cJSON_AddNumberToObject(humidity_sensor, "suggested_display_precision", 2));
    CJSON_CHECK(cJSON_AddStringToObject(humidity_sensor, "unique_id", name_buf));
    CJSON_CHECK(cJSON_AddStringToObject(humidity_sensor, "object_id", name_buf));

    return ESP_OK;

exit:
    /* No cleanup needed here as the parent object (cmps) will handle deletion */
    ESP_LOGE(TAG, "Failed to generate components");
    return ESP_FAIL;
}

/**
 * Publish MQTT discovery message for Home Assistant auto-discovery
 *
 * Creates and publishes a JSON message containing device metadata and sensor
 * configurations to the Home Assistant discovery topic. This enables automatic
 * integration of the device's sensors into Home Assistant.
 *
 * @param[in] client MQTT client handle used to publish the message
 */
static void publish_discovery_message(esp_mqtt_client_handle_t client)
{
    char *data_buf = NULL;
    char device_id[32];
    get_device_id(device_id, sizeof(device_id));

    /* Create topic */
    char discovery_topic[128];
    snprintf(discovery_topic, sizeof(discovery_topic), DISCOVERY_TOPIC_FORMAT, device_id);

    /* Create JSON root */
    cJSON *root = cJSON_CreateObject();
    CJSON_CHECK(root);

    /* "dev" section (device metadata) */
    cJSON *dev = cJSON_CreateObject();
    CJSON_CHECK(dev);
    cJSON_AddItemToObject(root, "dev", dev);

    CJSON_CHECK(cJSON_AddStringToObject(dev, "ids", device_id));
    CJSON_CHECK(cJSON_AddStringToObject(dev, "name", "Temperature Sensor"));
    CJSON_CHECK(cJSON_AddStringToObject(dev, "mf", "Carey Co."));
    CJSON_CHECK(cJSON_AddStringToObject(dev, "sn", device_id));
    CJSON_CHECK(cJSON_AddStringToObject(dev, "sa", "Kitchen"));
    CJSON_CHECK(cJSON_AddStringToObject(dev, "sw", APP_VERSION));

    /* "o" section (origin info) */
    cJSON *o = cJSON_CreateObject();
    CJSON_CHECK(o);
    cJSON_AddItemToObject(root, "o", o);

    CJSON_CHECK(cJSON_AddStringToObject(o, "name", "ESP HaLow Sensor"));
    CJSON_CHECK(
        cJSON_AddStringToObject(o, "url", "https://github.com/RobertWCarey/esp-halow-examples"));

    /* "cmps" section (components) */
    cJSON *cmps = cJSON_CreateObject();
    CJSON_CHECK(cmps);
    cJSON_AddItemToObject(root, "cmps", cmps);

    generate_components(cmps, device_id);

    /* Global state topic */
    char state_topic[128];
    snprintf(state_topic, sizeof(state_topic), STATE_TOPIC_FORMAT, device_id);
    CJSON_CHECK(cJSON_AddStringToObject(root, "state_topic", state_topic));

    /* Serialize JSON */
    data_buf = cJSON_PrintUnformatted(root);
    CJSON_CHECK(data_buf);

    /* Publish */
    int msg_id = esp_mqtt_client_publish(client, discovery_topic, data_buf, strlen(data_buf), 0, 1);
    ESP_LOGI(TAG, "Published discovery message: topic=%s, msg_id=%d", discovery_topic, msg_id);

exit:
    /* Cleanup */
    if (root != NULL)
    {
        cJSON_Delete(root);
    }

    if (data_buf != NULL)
    {
        free(data_buf);
    }
}

/**
 * Publish current sensor data to MQTT
 *
 * Retrieves the current sensor data and publishes
 * it as a JSON message to the device's state topic.
 *
 * @param[in] client MQTT client handle used to publish the message
 */
static void publish_state_data(esp_mqtt_client_handle_t client)
{
    char device_id[32];
    char state_topic[128];
    struct sensor_data data;
    cJSON *root = NULL;
    char *data_buf = NULL;
    int msg_id;

    get_device_id(device_id, sizeof(device_id));
    snprintf(state_topic, sizeof(state_topic), STATE_TOPIC_FORMAT, device_id);

    data = sensor_get();

    root = cJSON_CreateObject();
    CJSON_CHECK(root);

    CJSON_CHECK(cJSON_AddNumberToObject(root, "temperature", data.temperature_c));
    CJSON_CHECK(cJSON_AddNumberToObject(root, "humidity", data.humidity_percent));

    data_buf = cJSON_PrintUnformatted(root);
    CJSON_CHECK(data_buf);

    msg_id = esp_mqtt_client_publish(client, state_topic, data_buf, strlen(data_buf), 0, 0);
    ESP_LOGI(TAG, "Published data: topic=%s, msg_id=%d, len=%d", state_topic, msg_id,
             strlen(data_buf));
    ESP_LOGD(TAG, "%s", data_buf);

exit:
    /* Cleanup */
    if (root != NULL)
    {
        cJSON_Delete(root);
    }

    if (data_buf != NULL)
    {
        free(data_buf);
    }
}

/* Forward declarations */
static void update_timer_start(struct device *dev);
static void update_timer_stop(struct device *dev);

ESP_EVENT_DEFINE_BASE(DEVICE_EVENT);

/** Enumeration of device events */
typedef enum
{
    DEVICE_EVENT_UPDATE_STATE, /**< Event to update device state */
    DEVICE_EVENT_CONNECTED,    /**< Event when MQTT is connected or Home Assistant is online */
    DEVICE_EVENT_DISCONNECTED, /**< Event when MQTT is disconnected or Home Assistant is offline */
} device_event_t;

/**
 * Event handler for device events
 *
 * Handles device-specific events such as state updates. When a state update event
 * is received, it publishes the current sensor data to the MQTT broker.
 *
 * @param[in] handler_args User data registered to the event (device state)
 * @param[in] base Event base for the handler
 * @param[in] event_id The id for the received event
 * @param[in] event_data The data for the event (unused in this handler)
 */
static void device_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id,
                                 void *event_data)
{
    struct device *dev = (struct device *)handler_args;

    switch (event_id)
    {
    case DEVICE_EVENT_UPDATE_STATE:
        ESP_LOGD(TAG, "Received state update event");
        publish_state_data(dev->client);
        break;

    case DEVICE_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Received device connected event");
        publish_discovery_message(dev->client);
        publish_state_data(dev->client);
        update_timer_start(dev);
        break;

    case DEVICE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Received device disconnected event");
        update_timer_stop(dev);
        break;

    default:
        ESP_LOGW(TAG, "Unhandled device event: %ld", event_id);
        break;
    }
}

/**
 * Timer callback function that posts an update state event
 *
 * @param[in] arg User data passed to the timer (unused)
 */
static void update_timer_callback(void *arg)
{
    /* We don't need to use the device struct here since we're just posting an event.
     * The device struct will be available in the event handler */
    ESP_ERROR_CHECK(
        esp_event_post(DEVICE_EVENT, DEVICE_EVENT_UPDATE_STATE, NULL, 0, portMAX_DELAY));
}

/**
 * Initialize the timer and event handler for periodic updates
 *
 * This function sets up the event handler for device events and creates a timer
 * for periodic state updates. The timer is not started immediately, but will be
 * started when the MQTT client connects.
 *
 * @param[in] dev Pointer to the device structure
 */
void update_timer_init(struct device *dev)
{
    /* Register the device event handler */
    ESP_ERROR_CHECK(
        esp_event_handler_register(DEVICE_EVENT, ESP_EVENT_ANY_ID, device_event_handler, dev));

    /* Configure the timer for periodic updates (but don't start it yet) */
    const esp_timer_create_args_t timer_args = {.callback = &update_timer_callback,
                                                .name = "update_timer"};

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &dev->update_timer));
}

/**
 * Start the update timer if it's not already running
 *
 * @param[in] dev Pointer to the device structure containing the timer handle
 */
static void update_timer_start(struct device *dev)
{
    if (esp_timer_is_active(dev->update_timer) == false)
    {
        ESP_LOGI(TAG, "Starting update timer");
        ESP_ERROR_CHECK(esp_timer_start_periodic(dev->update_timer, UPDATE_INTERVAL_MS * 1000));
    }
}

/**
 * Stop the update timer if it's running
 *
 * @param[in] dev Pointer to the device structure containing the timer handle
 */
static void update_timer_stop(struct device *dev)
{
    if (esp_timer_is_active(dev->update_timer))
    {
        ESP_LOGI(TAG, "Stopping update timer");
        ESP_ERROR_CHECK(esp_timer_stop(dev->update_timer));
    }
}

/**
 * Log an error message if the error code is non-zero
 *
 * Helper function that logs error messages using ESP's logging system. Only logs
 * if the provided error code is non-zero. The error is logged at the ERROR level
 * with both a descriptive message and the hexadecimal error code.
 *
 * @param[in] message Descriptive message about where/what the error is
 * @param[in] error_code The error code to check and potentially log
 */
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/**
 * Event handler registered to receive MQTT events
 *
 * This function is called by the MQTT client event loop and handles various
 * MQTT events such as connection, disconnection, subscription, and data reception.
 *
 * @param[in] handler_args User data registered to the event
 * @param[in] base Event base for the handler (always MQTT Base in this example)
 * @param[in] event_id The id for the received event
 * @param[in] event_data The data for the event, esp_mqtt_event_handle_t
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id,
                               void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        /* Subscribe to Home Assistant status topic */
        int msg_id = esp_mqtt_client_subscribe(client, HA_STATUS_TOPIC, 0);
        ESP_LOGI(TAG, "Subscribed to %s, msg_id=%d", HA_STATUS_TOPIC, msg_id);

        /* Post connected event to handle discovery and timer start */
        ESP_ERROR_CHECK(
            esp_event_post(DEVICE_EVENT, DEVICE_EVENT_CONNECTED, NULL, 0, portMAX_DELAY));
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");

        /* Post disconnected event to handle timer stop */
        ESP_ERROR_CHECK(
            esp_event_post(DEVICE_EVENT, DEVICE_EVENT_DISCONNECTED, NULL, 0, portMAX_DELAY));
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);

        /* Check if this is a message from Home Assistant status topic */
        if (event->topic_len == strlen(HA_STATUS_TOPIC)
            && strncmp(event->topic, HA_STATUS_TOPIC, event->topic_len) == 0)
        {

            /* Handle 'online' message */
            if (event->data_len == 6 && strncmp(event->data, "online", 6) == 0)
            {
                ESP_LOGI(TAG, "Home Assistant is online");
                ESP_ERROR_CHECK(
                    esp_event_post(DEVICE_EVENT, DEVICE_EVENT_CONNECTED, NULL, 0, portMAX_DELAY));
            }
            /* Handle 'offline' message */
            else if (event->data_len == 7 && strncmp(event->data, "offline", 7) == 0)
            {
                ESP_LOGI(TAG, "Home Assistant is offline");
                ESP_ERROR_CHECK(esp_event_post(DEVICE_EVENT, DEVICE_EVENT_DISCONNECTED, NULL, 0,
                                               portMAX_DELAY));
            }
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls",
                                 event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",
                                 event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)",
                     strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

/**
 * Initialize and start the MQTT client
 *
 * Configures the MQTT client with broker connection details from project configuration,
 * initializes the client, registers the event handler, and starts the client.
 *
 * @param[in] dev Pointer to the device structure
 */
void mqtt_app_start(struct device *dev)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
        .credentials.username = CONFIG_BROKER_USERNAME,
        .credentials.authentication.password = CONFIG_BROKER_PASSWORD,
    };

    dev->client = esp_mqtt_client_init(&mqtt_cfg);
    assert(dev->client);

    /* Pass the device struct to the event handler so it can access the timer */
    esp_mqtt_client_register_event(dev->client, ESP_EVENT_ANY_ID, mqtt_event_handler, dev);
    esp_mqtt_client_start(dev->client);
}

static struct device device = {};

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Initialize and connect to Wi-Fi, blocks till connected */
    app_wlan_init();
    app_wlan_start();

    sensor_init();

    update_timer_init(&device);
    mqtt_app_start(&device);

    ESP_LOGI(TAG, "Sensor initialized with %d ms update interval", UPDATE_INTERVAL_MS);
}
