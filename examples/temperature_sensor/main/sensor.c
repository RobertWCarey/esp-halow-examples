#include "sensor.h"
#include "driver/i2c_master.h"
#include <assert.h>
#include <esp_log.h>
#include <sht4x.h>

#define TAG "sensor"

/** GPIO number for I2C master clock (SCL) line, defined via Kconfig: CONFIG_I2C_MASTER_SCL_IO */
#define I2C_MASTER_SCL_IO CONFIG_I2C_MASTER_SCL_IO

/** GPIO number for I2C master data (SDA) line, defined via Kconfig: CONFIG_I2C_MASTER_SDA_IO */
#define I2C_MASTER_SDA_IO CONFIG_I2C_MASTER_SDA_IO

/** Handle for the I2C master bus */
static i2c_master_bus_handle_t bus_handle;

/** Handle for the SHT4x temperature and humidity sensor device */
static sht4x_handle_t sht4x_dev_hdl = NULL;

void sensor_init(void)
{
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

    sht4x_config_t dev_cfg = I2C_SHT4X_CONFIG_DEFAULT;
    esp_err_t err = sht4x_init(bus_handle, &dev_cfg, &sht4x_dev_hdl);
    if (err != ESP_OK || sht4x_dev_hdl == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize SHT4X device: %s", esp_err_to_name(err));
        assert(false);
    }

    ESP_LOGI(TAG, "SHT4X sensor initialized successfully");
}

struct sensor_data sensor_get(void)
{
    struct sensor_data data = {0};

    if (sht4x_dev_hdl == NULL)
    {
        ESP_LOGE(TAG, "SHT4X device not initialized");
        return data;
    }

    esp_err_t err =
        sht4x_get_measurement(sht4x_dev_hdl, &data.temperature_c, &data.humidity_percent);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read SHT4X sensor: %s", esp_err_to_name(err));
    }

    return data;
}
