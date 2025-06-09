/*
 * Copyright 2025 Robert Carey
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <assert.h>

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "hal/adc_types.h"
#include "soc/adc_channel.h"

#include "battery.h"

static const char *TAG = "battery";

/* Compile-time mapping of GPIO pins to ADC channels. Note this was validated with the ESP32-S3
 * datasheet. Your milage may vary if you are using a different chip. */
#if CONFIG_BATTERY_GPIO_PIN == 1
#define BATTERY_ADC_CHANNEL ADC1_GPIO1_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_1
#elif CONFIG_BATTERY_GPIO_PIN == 2
#define BATTERY_ADC_CHANNEL ADC1_GPIO2_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_1
#elif CONFIG_BATTERY_GPIO_PIN == 3
#define BATTERY_ADC_CHANNEL ADC1_GPIO3_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_1
#elif CONFIG_BATTERY_GPIO_PIN == 4
#define BATTERY_ADC_CHANNEL ADC1_GPIO4_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_1
#elif CONFIG_BATTERY_GPIO_PIN == 5
#define BATTERY_ADC_CHANNEL ADC1_GPIO5_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_1
#elif CONFIG_BATTERY_GPIO_PIN == 6
#define BATTERY_ADC_CHANNEL ADC1_GPIO6_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_1
#elif CONFIG_BATTERY_GPIO_PIN == 7
#define BATTERY_ADC_CHANNEL ADC1_GPIO7_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_1
#elif CONFIG_BATTERY_GPIO_PIN == 8
#define BATTERY_ADC_CHANNEL ADC1_GPIO8_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_1
#elif CONFIG_BATTERY_GPIO_PIN == 9
#define BATTERY_ADC_CHANNEL ADC1_GPIO9_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_1
#elif CONFIG_BATTERY_GPIO_PIN == 10
#define BATTERY_ADC_CHANNEL ADC1_GPIO10_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_1
#elif CONFIG_BATTERY_GPIO_PIN == 11
#define BATTERY_ADC_CHANNEL ADC2_GPIO11_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_2
#elif CONFIG_BATTERY_GPIO_PIN == 12
#define BATTERY_ADC_CHANNEL ADC2_GPIO12_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_2
#elif CONFIG_BATTERY_GPIO_PIN == 13
#define BATTERY_ADC_CHANNEL ADC2_GPIO13_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_2
#elif CONFIG_BATTERY_GPIO_PIN == 14
#define BATTERY_ADC_CHANNEL ADC2_GPIO14_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_2
#elif CONFIG_BATTERY_GPIO_PIN == 15
#define BATTERY_ADC_CHANNEL ADC2_GPIO15_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_2
#elif CONFIG_BATTERY_GPIO_PIN == 16
#define BATTERY_ADC_CHANNEL ADC2_GPIO16_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_2
#elif CONFIG_BATTERY_GPIO_PIN == 17
#define BATTERY_ADC_CHANNEL ADC2_GPIO17_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_2
#elif CONFIG_BATTERY_GPIO_PIN == 18
#define BATTERY_ADC_CHANNEL ADC2_GPIO18_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_2
#elif CONFIG_BATTERY_GPIO_PIN == 19
#define BATTERY_ADC_CHANNEL ADC2_GPIO19_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_2
#elif CONFIG_BATTERY_GPIO_PIN == 20
#define BATTERY_ADC_CHANNEL ADC2_GPIO20_CHANNEL
#define BATTERY_ADC_UNIT ADC_UNIT_2
#else
#error \
    "Unsupported GPIO pin for battery measurement. Please use a GPIO pin that supports ADC functionality."
#endif

/** Minimum battery voltage (mV) corresponding to 0% level. */
#define BATTERY_EMPTY_MV CONFIG_BATTERY_EMPTY_MV

/** Maximum battery voltage (mV) corresponding to 100% level. */
#define BATTERY_FULL_MV CONFIG_BATTERY_FULL_MV

/**
 * Clamp a value between a lower and upper bound.
 *
 * @param x Value to clamp.
 * @param low Minimum allowable value.
 * @param high Maximum allowable value.
 *
 * @return Clamped value.
 */
#define CLAMP(x, low, high) ((x) < (low) ? (low) : ((x) > (high) ? (high) : (x)))

static struct battery_state
{
    bool started;
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t adc_cali_handle;
} state = {};

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten,
                                 adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Calibration Success");
    }
    else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated)
    {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    }
    else
    {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

/* From ESP-IDF version 5.1.3 and on `ADC_ATTEN_DB_11` was deprecated in favour of
 * `ADC_ATTEN_DB_12`. According to the ESP-IDF docs they behave the same. */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 3)
#define BATTERY_ADC_ATTEN ADC_ATTEN_DB_12
#else
#define BATTERY_ADC_ATTEN ADC_ATTEN_DB_11
#endif

void battery_init(void)
{
    /* This shall only every be initialised once. */
    assert(state.started == false);
    state.started = true;

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = BATTERY_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &state.adc_handle));

    adc_oneshot_chan_cfg_t config = {
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(state.adc_handle, BATTERY_ADC_CHANNEL, &config));

    bool ok = adc_calibration_init(BATTERY_ADC_UNIT, BATTERY_ADC_CHANNEL, BATTERY_ADC_ATTEN,
                                   &state.adc_cali_handle);
    if (!ok)
    {
        ESP_LOGE(TAG, "Failed to calibrate ADC for battery measurment\n");
        /* We rely on the ADC being calibrated for the voltage measurements. If we are unable to
         * calibrate it means there is something wrong with the HW or the SW is misconfigured. */
        assert(false);
    }
}

struct battery_status battery_get_status(void)
{
    assert(state.started);

    struct battery_status status = {};
    int adc_raw;
    int raw_voltage;

    ESP_ERROR_CHECK(adc_oneshot_read(state.adc_handle, BATTERY_ADC_CHANNEL, &adc_raw));
    ESP_LOGD(TAG, "ADC raw data: %d (GPIO%d)", adc_raw, CONFIG_BATTERY_GPIO_PIN);

    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(state.adc_cali_handle, adc_raw, &raw_voltage));
    ESP_LOGD(TAG, "ADC voltage: %d mV", raw_voltage);

    /* Adjust based on voltage divider */
    status.voltage_mv = 2 * raw_voltage;

    /* Estimate battery percentage linearly */
    int percent =
        (status.voltage_mv - BATTERY_EMPTY_MV) * 100 / (BATTERY_FULL_MV - BATTERY_EMPTY_MV);
    status.level_percent = CLAMP(percent, 0, 100);

    return status;
}
