/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_camera.h"
#include "esp_event.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "MQTT_EXAMPLE";

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

// Adjust topic name as needed
#define CAMERA_TOPIC CONFIG_CAMERA_IMAGE_TOPIC

// Adjust publish interval (in milliseconds)
#define PUBLISH_INTERVAL_MS 5000

/**
 * @brief Task to capture a picture from the camera and publish it to MQTT.
 *
 * @param pvParameters pointer to the MQTT client handle passed in from the creator of the task.
 */
static void camera_publish_task(void *pvParameters)
{
    esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t)pvParameters;

    esp_err_t res = ESP_OK;
    while (1)
    {
        size_t image_data_buf_len = 0;
        uint8_t *image_data_buf = NULL;
        // Get frame buffer from camera
        camera_fb_t *frame = esp_camera_fb_get();
        if (!frame)
        {
            ESP_LOGE(TAG, "Camera capture failed");
            // Optionally delay to avoid spamming if camera is not ready
            vTaskDelay(pdMS_TO_TICKS(PUBLISH_INTERVAL_MS));
            continue;
        }

        if (frame)
        {
#if CONFIG_IMAGE_JPEG_FORMAT
            if (frame->format == PIXFORMAT_JPEG)
            {
                image_data_buf = frame->buf;
                image_data_buf_len = frame->len;
            }
            else if (!frame2jpg(frame, 60, &image_data_buf, &image_data_buf_len))
            {
                ESP_LOGE(TAG, "JPEG compression failed");
                res = ESP_FAIL;
            }
#elif CONFIG_IMAGE_BMP_FORMAT
            if (frame2bmp(frame, &image_data_buf, &image_data_buf_len) != true)
            {
                res = ESP_FAIL;
            }
#endif
        }
        else
        {
            res = ESP_FAIL;
        }

        if (res == ESP_OK)
        {
            // Publish the raw image buffer directly over MQTT.
            // NOTE: Large images can cause issues depending on your MQTT broker limits.
            int msg_id = esp_mqtt_client_publish(client, CAMERA_TOPIC, (const char *)image_data_buf,
                                                 image_data_buf_len, 0, 0);
            ESP_LOGI(TAG, "Published camera frame, topic=%s, msg_id=%d, size=%u bytes",
                     CAMERA_TOPIC, msg_id, frame->len);

#if CONFIG_IMAGE_JPEG_FORMAT
            if (frame->format != PIXFORMAT_JPEG)
            {
                free(image_data_buf);
                image_data_buf = NULL;
            }
#elif CONFIG_IMAGE_BMP_FORMAT
            free(image_data_buf);
            image_data_buf = NULL;
#endif
            // Return the frame buffer back to the driver for reuse
            esp_camera_fb_return(frame);
        }
        else
        {

            ESP_LOGW(TAG, "Failed to capture image");
        }
        // Wait until next publish interval
        vTaskDelay(pdMS_TO_TICKS(PUBLISH_INTERVAL_MS));
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
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
        // ===> START the camera publish task (if not already started)
        static bool s_task_started = false;
        if (!s_task_started)
        {
            s_task_started = true;
            xTaskCreate(camera_publish_task, "camera_publish_task", 4096, (void *)client, 5, NULL);
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
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
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
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

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
        .credentials.username = CONFIG_BROKER_USERNAME,
        .credentials.authentication.password = CONFIG_BROKER_PASSWORD,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example
     * mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}
