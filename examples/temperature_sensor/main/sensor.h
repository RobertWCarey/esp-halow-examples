/*
 * Copyright 2025 Robert Carey
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#pragma once

/** Sensor reading structure for temperature and humidity. */
struct sensor_data
{
    float temperature_c;    /**< Temperature in Celsius */
    float humidity_percent; /**< Relative humidity in percentage */
};

/**
 * Initialize the environmental sensor subsystem.
 *
 * This function configures the I2C bus and sets up the sensor.
 *
 * @warning This can only be called once.
 */
void sensor_init(void);

/**
 * Read temperature and humidity data from the sensor.
 *
 * @return @c struct sensor_data containing the current measurement.
 */
struct sensor_data sensor_get(void);
