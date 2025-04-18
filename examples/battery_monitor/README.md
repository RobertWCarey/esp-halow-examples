# ESP HaLow Battery Monitoring with MQTT

## Overview

This example demonstrates how to use an ESP32 with a Morse Micro HaLow chip to monitor battery voltage
and publish the data to an MQTT broker. The example is designed to work seamlessly with Home Assistant
through MQTT auto-discovery, allowing for easy integration into your smart home system.

The application performs the following functions:

1. Connects to a Wi-Fi network using the HaLow chip
2. Reads battery voltage using the ESP32's ADC
3. Calculates battery level percentage
4. Publishes battery data to an MQTT broker at configurable intervals using ESP timer
5. Supports Home Assistant auto-discovery for zero-configuration integration

> **Note**
> This example uses a simple linear mapping between voltage and battery percentage, which is
> straightforward to implement but not highly accurate. Most batteries, especially Lithium-Ion, have
> non-linear discharge curves where voltage doesn't decrease uniformly. For production applications,
> consider either:
>
> - Implementing a more sophisticated algorithm that accounts for the battery's specific discharge
>   curve
> - Using a dedicated battery fuel gauge IC for the most accurate measurements

## Hardware Requirements

- ESP32-S3 development board
- Morse Micro HaLow chip (configured according to sdkconfig.defaults)
- Battery connected to a GPIO pin through a voltage divider (default: GPIO4)

## Configuration

The example can be configured through the ESP-IDF menuconfig system. The main configuration options
can be found in the following sections:

- "Example Configuration" menu: Contains settings for the MQTT broker URL, username, password, and
  update interval.
- "Example Configuration" > "Battery Configuration" submenu: Contains settings for the GPIO pin
  connected to the battery voltage divider, and the battery voltage levels corresponding to 0% and
  100% charge.

Wi-Fi and IP configuration can be found in the "Wi-Fi HaLow Connection Manager" menu. See the
`mm_app_common.h` documentation for details on configuring the Wi-Fi connection.

## Battery Monitoring

The example uses the ESP32's ADC to measure battery voltage. The ADC is configured to use the GPIO pin
specified by `CONFIG_BATTERY_GPIO_PIN` (default: GPIO4) with 12dB attenuation. The raw ADC reading is
calibrated and converted to millivolts, then adjusted based on the voltage divider configuration.

Battery level percentage is calculated linearly between the defined empty voltage
(`CONFIG_BATTERY_EMPTY_MV`) and full voltage (`CONFIG_BATTERY_FULL_MV`).

## MQTT Integration

The example publishes two main types of MQTT messages:

1. **Discovery Message**: Sent once on connection to the broker, this message contains device metadata
   and sensor configurations that allow Home Assistant to automatically discover and configure the device.

2. **State Updates**: Sent periodically (every `CONFIG_UPDATE_INTERVAL_MS` milliseconds) using an ESP timer
   and event loop, these messages contain the current battery voltage and level percentage.

### MQTT Topics

- Discovery topic: `homeassistant/device/<device_id>/config`
- State topic: `<device_id>/state`

Where `<device_id>` is a unique identifier generated from the device's MAC address.

## Home Assistant Integration

When the device connects to the configured MQTT broker, it will automatically be discovered by Home
Assistant if you have the MQTT integration enabled. The device will appear with two sensors:

1. Battery Voltage (mV)
2. Battery Level (%)

No manual configuration is required in Home Assistant beyond having the MQTT integration set up.

## Building and Flashing

```bash
idf.py build
idf.py -p (PORT) flash
```

## Monitoring

```bash
idf.py -p (PORT) monitor
```

## Troubleshooting

- If the device fails to connect to Wi-Fi, check the HaLow configuration in the config store.
- If the device connects to Wi-Fi but fails to connect to the MQTT broker, verify the broker URL, username,
  and password.
- If Home Assistant doesn't discover the device, ensure that the MQTT integration is properly configured in
  Home Assistant and that the broker is accessible from both the device and Home Assistant.
