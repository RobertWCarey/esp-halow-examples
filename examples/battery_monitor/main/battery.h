/*
 * Copyright 2025 Robert Carey
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */
#pragma once

/** Battery status structure. */
struct battery_status
{
    int voltage_mv;    /**< Battery voltage in millivolts */
    int level_percent; /**< Battery level in percentage (0–100) */
};

/**
 * Initialize the battery monitoring subsystem.
 *
 * This function configures the ADC and calibration handles
 * necessary to measure battery voltage.
 *
 * @warning This can only be called once.
 */
void battery_init(void);

/**
 * Read battery voltage and estimate battery level percentage.
 *
 * This function reads the raw ADC value, converts it to millivolts,
 * applies the required scaling, and calculates a linear battery percentage
 * based on predefined minimum and maximum voltages.
 *
 * @return @c struct battery_status.
 */
struct battery_status battery_get_status(void);
