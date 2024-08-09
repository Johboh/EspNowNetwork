# EspNowNetwork
[![PlatformIO CI](https://github.com/Johboh/EspNowNetwork/actions/workflows/platformio.yaml/badge.svg)](https://registry.platformio.org/libraries/johboh/EspNowNetwork)
[![ESP-IDF CI](https://github.com/Johboh/EspNowNetwork/actions/workflows/espidf.yaml/badge.svg)](https://components.espressif.com/components/johboh/espnownetwork)
[![GitHub release](https://img.shields.io/github/release/Johboh/EspNowNetwork.svg)](https://github.com/Johboh/EspNowNetwork/releases)
[![Clang-format](https://github.com/Johboh/EspNowNetwork/actions/workflows/clang-format.yaml/badge.svg)](https://github.com/Johboh/EspNowNetwork)

### Summary
Arduino (using Arduino IDE or PlatformIO) and ESP-IDF (using Espressif IoT Development Framework or PlatformIO) compatible library for setting up a network of [ESP-NOW](https://www.espressif.com/en/solutions/low-power-solutions/esp-now) nodes.

### Usage/Purpose
One use case for the EspNowNetwork is to run a [ESP-NOW](https://www.espressif.com/en/solutions/low-power-solutions/esp-now) network of battery powered nodes with sensors, where the nodes will sleep most of the time and low power is an important factor. Nodes will wake up either due to external interrupt (like a PIR sensor or switch) or perodically based on time. Upon wakeup, they will send their sensors values and go back to sleep. On the receiving side, there is a always powered router board that will receive the sensor values and act on or forward them for consumption somewhere else, like MQTT and/or [Home Assistant](https://www.home-assistant.io).

### Features
- **Encryption**: ESP-NOW have built in encryption, but that relies on that the host adds all peers to be able to decrypt messages. There is a limit on how many peers one can have when using encryption (17). So instead there is an application layer encryption applied using [GCM](https://en.wikipedia.org/wiki/Galois/Counter_Mode). Each message is unique and valid only once to prevent replay attacks.
- **Generic firmware**: For boards that do the same thing (e.g. they have the same hardware), the same firmware can be used for all of them. No unique ID is required to be programmed into each board/node.
- **Over The Air/OTA**: A node can be updated Over The Air. The node report their firmware version upon handsake, and the host can send back wifi credentials and an URL where to download the new firmware. The node will download the firmware, flash it and restart.

### Installation
There are a set if different variants of this library you can use.
- **EspNowNetworkNode**: Use this for your nodes. This library provide a way to setup ESP-NOW and for sending messages, as well as doing OTA updates when the host indicates that a new firmware version is available.
  - PlatformIO: Add the following to `libs_deps`:
    ```
    Johboh/EspNowNetworkNode@^0.6.7
    ```
  - Add to `idf_component.yml` next to your main component:
    ```
    dependencies:
      johboh/EspNowNetworkNode:
        version: ">=0.6.7"
    ```
  See the [Arduino](examples/arduino/node/Node.ino) or [ESP-IDF](examples/espidf/node/main/main.cpp) for full examples. In short (this is nota complete example):
  ```c++
  struct MyApplicationMessage {
    uint8_t id = 0x01;
    double temperature;
  };

  EspNowPreferences _esp_now_preferences;
  EspNowCrypt _esp_now_crypt(esp_now_encryption_key, esp_now_encryption_secret);
  EspNowNode _esp_now_node(_esp_now_crypt, _esp_now_preferences, FIRMWARE_VERSION, _on_status, _on_log,
                         arduino_esp_crt_bundle_attach);

  void main() {
    _esp_now_preferences.initalizeNVS();
    _esp_now_node.setup();
    _esp_now_node.sendMessage(&message, sizeof(MyApplicationMessage));
    esp_deep_sleep(SLEEP_TIME_US);
  }
  ```

- **EspNowNetworkHostDriver**: Use this for your host. This library receives messages from the nodes and forward or handle the received data by handling nodes as Devices. It also provide a way to perform firmware updates by incoperating a [Firmware Checker](src/host_driver/FirmwareChecker.h) which checks for new firmwares on a HTTP server. It is also possible to implement a custom [Firmware Checker](src/host_driver/IFirmwareChecker.h) to match your HTTP server setup. There is an example of a HTTP server to use for the firmware for the default implementation of the [Firmware Checker](src/host_driver/FirmwareChecker.h) located [here](firmware%20http%20server).
  - PlatformIO: Add the following to `libs_deps`:
    ```
    Johboh/EspNowNetworkHostDriver@^0.6.7
    ```
  - Add to `idf_component.yml` next to your main component:
    ```
    dependencies:
      johboh/EspNowNetworkHostDriver:
        version: ">=0.6.7"
    ```
  See the [Arduino](examples/arduino/host_driver/HostDriver.ino) or [ESP-IDF](examples/espidf/host_driver/main/main.cpp) for full examples. In short (this is nota complete example):
  ```c++
  DeviceFootPedal _device_foot_pedal_left(0x543204017648, "Left");
  DeviceFootPedal _device_foot_pedal_right(0x543204016bfc, "Right");

  std::vector<std::reference_wrapper<Device>> _devices{_device_foot_pedal_left, _device_foot_pedal_right};
  DeviceManager _device_manager(_devices, []() { return _mqtt_remote.connected(); });
  FirmwareChecker _firmware_checker(firmware_update_base_url, _devices);
  HostDriver _host_driver(_device_manager, wifi_ssid, wifi_password, esp_now_encryption_key esp_now_encryption_secret,
                        [](const std::string message, const std::string sub_path, const bool retain) {
                          _mqtt_remote.publishMessage(_mqtt_remote.clientId() + sub_path, message, retain);
                        });

  void setup() {
    _host_driver.setup(_firmware_checker);
  }

  void loop() {
    _device_manager.handle();
    _firmware_checker.handle();
  }
  ```

- **EspNowNetworkHost**: This is just the bare host library, without a Device Manager, Host Driver nor Firmware Checker. I still recommend using the **EspNowNetworkHostDriver**, but if you want to roll the host fully on your own, this is the library to use.
  - PlatformIO: Add the following to `libs_deps`:
    ```
    Johboh/EspNowNetworkHost@^0.6.7
    ```
  - Add to `idf_component.yml` next to your main component:
    ```
    dependencies:
      johboh/EspNowNetworkHost:
        version: ">=0.6.7"
    ```

- **EspNowNetwork**: This is the legacy full library consiting of both the node and the host code (but not the host driver). Not recommended for new projects. Instead, use the induvidual libraries listed above.

### Examples
- [Arduino: Host](examples/arduino/host/Host.ino)
- [Arduino: Host Driver](examples/arduino/host_driver/HostDriver.ino)
- [Arduino: Node](examples/arduino/node/Node.ino)
- [ESP-IDF: Host](examples/espidf/host/main/main.cpp)
- [ESP-IDF: Host Driver](examples/espidf/host_driver/main/main.cpp)
- [ESP-IDF: Node](examples/espidf/node/main/main.cpp)

### Parition table (for the Node (and for the host/host driver if using OTA for the host))
You need to have two app partitions in your parition table to be able to swap between otas. This is an example:
```
# Name,   Type,  SubType, Offset,          Size, Flags
nvs,      data,      nvs,       ,           16K
otadata,  data,      ota,       ,            8K
phy_init, data,      phy,       ,            4K
coredump, data, coredump,       ,           64K
ota_0,     app,    ota_0,       ,         1500K
ota_1,     app,    ota_1,       ,         1500K
spiffs,   data,   spiffs,       ,          800K
```
To set partition table, save above in a file called `partitions_with_ota.csv`. For ESP-IDF, specify to use this one using menuconfig. For platformIO, add the following to your `platformio.ini`: `board_build.partitions = partitions_with_ota.csv`

### Functionallity verified on the following platforms and frameworks
- ESP32 (tested with platformIO [espressif32@6.4.0](https://github.com/platformio/platform-espressif32) / [arduino-esp32@2.0.11](https://github.com/espressif/arduino-esp32) / [ESP-IDF@4.4.6](https://github.com/espressif/esp-idf) / [ESP-IDF@5.1.2](https://github.com/espressif/esp-idf) / [ESP-IDF@5.2.0](https://github.com/espressif/esp-idf) on ESP32-S2, ESP32-C3 and ESP32-C6)

Newer version most probably work too, but they have not been verified.

### Dependencies
- Needs C++17 for `std::optional`.
  - For platformIO in `platformio.ini`:
    ```C++
    build_unflags=-std=gnu++11 # "Disable" C++11
    build_flags=-std=gnu++17 # "Enable" C++17
    ```
