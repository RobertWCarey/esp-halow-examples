# ESP HaLow Examples

**Note:** This repository is a work in progress. There is no guarantee that the structure or content
won’t undergo significant changes until the 1.0.0 release.

---

This repository contains examples demonstrating the use of ESP32 series chips alongside Morse Micro
HaLow chips. The primary goal is to showcase how Wi-Fi HaLow can be integrated into Espressif’s
extensive library of networking applications.

### Reference Examples
Here are some excellent starting points for reference:
- [esp-iot-solution](https://github.com/espressif/esp-iot-solution)
- [esp-idf](https://github.com/espressif/esp-idf)
  - Specifically, the `examples/protocols/` directory contains many useful examples.

Most examples include a function that initializes networking (Wi-Fi or Ethernet). For example, the
following snippet is from [esp-idf](https://github.com/espressif/esp-idf):

```c
/* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
 * Read "Establishing Wi-Fi or Ethernet Connection" section in
 * examples/protocols/README.md for more information about this function.
 */
ESP_ERROR_CHECK(example_connect());
```

This repository includes a [halow](components/halow) component that facilitates connections over
Wi-Fi HaLow. You can configure the connection settings using `idf.py menuconfig`.

```c
/* Initialize and connect to Wi-Fi, blocks till connected */
app_wlan_init();
app_wlan_start();
```

---

## Getting Started

To get started, you need to clone the [mm-iot-esp32](https://github.com/MorseMicro/mm-iot-esp32)
repository and set the `MMIOT_ROOT` environment variable. If you cloned the repository into your
home directory, you can do this with the following command:

```bash
export MMIOT_ROOT=~/mm-iot-esp32
```

> **Note:** This guide does not cover setting up `esp-idf`. Please refer to the
> [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/v5.1.1/esp32/get-started/index.html)
> for detailed instructions.

You will need ESP-IDF v5.1.1, as this is the version currently tested with
[mm-iot-esp32](https://github.com/MorseMicro/mm-iot-esp32). After setting up `esp-idf`, verify the
version:

```bash
idf.py --version
ESP-IDF v5.1.1
```

Now, you can build and flash one of the example applications onto your device:

```bash
cd examples/camera/http_pic_server
idf.py build
idf.py flash monitor
```
---

## Reference Boards
I have knocked up a couple of boards for the purposes of testing, the main one using being the
`Xiao ESP32S3 Wi-Fi HaLow Carrier (MM6108, 3-0062)`. This uses the [Seeed Studio XIAO ESP32S3](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/) board.
However, any ESP32S3 board connected to a Morse Micro chip should work.

- [Xiao ESP32S3 Wi-Fi HaLow Carrier (MM6108, 3-0062)](https://oshwlab.com/robcarey/stockman-3-0062)
- [Firebeetle 2 Wi-Fi HaLow Carrier (MM6108, 3-0058)](https://oshwlab.com/robcarey/3-0058-firebeetle-2-mm-udash-carrier-pro)

---

## Future Work

Here is a list of potential improvements and features to be worked on, in no particular order:

### Make Using Different Boards Easier
Currently, the pin configuration is hardcoded for a specific board in `sdkconfig.defaults`. It would
be useful to allow users to select supported boards with a single flag, similar to how the
`CAMERA_MODULE` option works.

### Integrate with ESP-NETIF
Currently, the `mmipal` component provided by [mm-iot-esp32](https://github.com/MorseMicro/mm-iot-esp32)
hooks directly into LWIP, bypassing `ESP-NETIF`. While functional, this approach may complicate
coexistence with other native ESP interfaces like traditional Wi-Fi and Ethernet. Based on the
[ESP-NETIF Developer's Manual](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_netif_driver.html),
integration does not seem overly complex (famous last words 😅) and may align better with Espressif’s
architecture.

### Add an Example for Provisioning Wi-Fi HaLow Using the 2.4GHz Radio
It would be beneficial to provide an example that provisions the Wi-Fi HaLow interface using either
Bluetooth or 2.4GHz Wi-Fi. Espressif already has several examples using a web interface in Soft-AP
mode for this purpose.

### Low Power Applications
Creating a basic sensor application that utilizes ESP32’s low-power modes and the power-saving
features of Wi-Fi HaLow would be valuable for battery-powered use cases.
