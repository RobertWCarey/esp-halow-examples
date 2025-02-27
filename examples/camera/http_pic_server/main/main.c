/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "camera_pin.h"
#include "esp_camera.h"
#include "mm_app_common.h"

#define TEST_ESP_OK(ret) assert(ret == ESP_OK)
#define TEST_ASSERT_NOT_NULL(ret) assert(ret != NULL)

static bool auto_jpeg_support =
    false; // whether the camera sensor support compression or JPEG encode

static const char *TAG = "pic server";

esp_err_t start_pic_server(void);

static esp_err_t init_camera(uint32_t xclk_freq_hz, pixformat_t pixel_format,
                             framesize_t frame_size, uint8_t fb_count)
{
    camera_config_t camera_config = {
        .pin_pwdn = CAMERA_PIN_PWDN,
        .pin_reset = CAMERA_PIN_RESET,
        .pin_xclk = CAMERA_PIN_XCLK,
        .pin_sscb_sda = CAMERA_PIN_SIOD,
        .pin_sscb_scl = CAMERA_PIN_SIOC,

        .pin_d7 = CAMERA_PIN_D7,
        .pin_d6 = CAMERA_PIN_D6,
        .pin_d5 = CAMERA_PIN_D5,
        .pin_d4 = CAMERA_PIN_D4,
        .pin_d3 = CAMERA_PIN_D3,
        .pin_d2 = CAMERA_PIN_D2,
        .pin_d1 = CAMERA_PIN_D1,
        .pin_d0 = CAMERA_PIN_D0,
        .pin_vsync = CAMERA_PIN_VSYNC,
        .pin_href = CAMERA_PIN_HREF,
        .pin_pclk = CAMERA_PIN_PCLK,

        // EXPERIMENTAL: Set to 16MHz on ESP32-S2 or ESP32-S3 to enable EDMA mode
        .xclk_freq_hz = xclk_freq_hz,
        .ledc_timer = LEDC_TIMER_0, // // This is only valid on ESP32/ESP32-S2. ESP32-S3 use LCD_CAM
                                    // interface.
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = pixel_format, // YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size = frame_size, // QQVGA-UXGA, sizes above QVGA are not been recommended when not
                                  // JPEG format.

        .jpeg_quality = 12,   // 0-63
        .fb_count = fb_count, // For ESP32/ESP32-S2, if more than one, i2s runs in continuous mode.
                              // Use only with JPEG.
        .grab_mode = CAMERA_GRAB_LATEST,
        .fb_location = CAMERA_FB_IN_PSRAM};

    // initialize the camera
    esp_err_t ret = esp_camera_init(&camera_config);

    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1); // flip it back
    // initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID)
    {
        s->set_saturation(s, -2); // lower the saturation
    }

    if (s->id.PID == OV3660_PID || s->id.PID == OV2640_PID)
    {
        s->set_vflip(s, 1); // flip it back
    }
    else if (s->id.PID == GC0308_PID)
    {
        s->set_hmirror(s, 0);
    }
    else if (s->id.PID == GC032A_PID)
    {
        s->set_vflip(s, 1);
    }

    camera_sensor_info_t *s_info = esp_camera_sensor_get_info(&(s->id));

    if (ESP_OK == ret && PIXFORMAT_JPEG == pixel_format && s_info->support_jpeg == true)
    {
        auto_jpeg_support = true;
    }

    return ret;
}

void app_main()
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Initialize and connect to Wi-Fi, blocks till connected */
    app_wlan_init();
    app_wlan_start();
    /* Check the image format supported by your camera sensor.
     * Typically, when you need to obtain a larger resolution image, increase the xclk clock
     * frequency. Similarly, when you need a smaller resolution image, please use a smaller xclk
     * clock frequency, otherwise the warning `EV-EOF-OVF` may be triggered.
     */
    TEST_ESP_OK(init_camera(20000000, PIXFORMAT_YUV422, FRAMESIZE_VGA, 2));

    TEST_ESP_OK(start_pic_server());

    ESP_LOGI(TAG, "Begin capture frame");
}
