# ESP HaLow Temperature & Humidity Sensor with MQTT

## Overview

This example demonstrates how to use an ESP32 with a Morse Micro HaLow chip to monitor temperature
and humidity, and publish the data to an MQTT broker. The example is designed to work seamlessly
with Home Assistant through MQTT auto-discovery, allowing for easy integration into your smart home
system.

The application performs the following functions:

1. Connects to a Wi-Fi network using the HaLow chip
2. Reads temperature and humidity using a sensor (see `sensor.h`)
3. Publishes sensor data to an MQTT broker at configurable intervals using ESP timer
4. Supports Home Assistant auto-discovery for zero-configuration integration

## Hardware Requirements

- ESP32-S3 development board
- Morse Micro HaLow chip (configured according to sdkconfig.defaults)
- Supported temperature/humidity sensor connected as per your `sensor.h` implementation
  - Current example uses an SHT4x I2C temperature/humidity sensor.

## Configuration

The example can be configured through the ESP-IDF menuconfig system. The main configuration options
can be found in the following sections:

- "Example Configuration" menu: Contains settings for the MQTT broker URL, username, password, and
  update interval.
- "Sensor Configuration" submenu: Contains settings for the GPIO pins or I2C bus used by the
  temperature/humidity sensor.

Wi-Fi and IP configuration can be found in the "Wi-Fi HaLow Connection Manager" menu. See the
`mm_app_common.h` documentation for details on configuring the Wi-Fi connection.

## Temperature & Humidity Monitoring

The example uses the ESP32 to read temperature and humidity values from the configured sensor.
The sensor is initialized and read using the functions provided in `sensor.h` and `sensor.c`.

## MQTT Integration

The example publishes two main types of MQTT messages:

1. **Discovery Message**: Sent once on connection to the broker, this message contains device
   metadata and sensor configurations that allow Home Assistant to automatically discover and
   configure the device.

2. **State Updates**: Sent periodically (every `CONFIG_UPDATE_INTERVAL_MS` milliseconds) using an
   ESP timer and event loop, these messages contain the current temperature and humidity readings.

### MQTT Topics

- Discovery topic: `homeassistant/device/<device_id>/config`
- State topic: `<device_id>/state`

Where `<device_id>` is a unique identifier generated from the device's MAC address.

## Home Assistant Integration

When the device connects to the configured MQTT broker, it will automatically be discovered by Home
Assistant if you have the MQTT integration enabled. The device will appear with two sensors:

1. Temperature (°C)
2. Humidity (%)

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
- If the device connects to Wi-Fi but fails to connect to the MQTT broker, verify the broker URL,
  username, and password.
- If Home Assistant doesn't discover the device, ensure that the MQTT integration is properly
  configured in Home Assistant and that the broker is accessible from both the device and Home
  Assistant.
