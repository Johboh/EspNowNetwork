# EspNowNetwork
[![PlatformIO CI](https://github.com/Johboh/EspNowNetwork/actions/workflows/platformio.yaml/badge.svg)](https://registry.platformio.org/libraries/johboh/EspNowNetwork)
[![ESP-IDF CI](https://github.com/Johboh/EspNowNetwork/actions/workflows/espidf.yaml/badge.svg)](https://components.espressif.com/components/johboh/espnownetwork)
[![GitHub release](https://img.shields.io/github/release/Johboh/EspNowNetwork.svg)](https://github.com/Johboh/EspNowNetwork/releases)
[![Clang-format](https://github.com/Johboh/EspNowNetwork/actions/workflows/clang-format.yaml/badge.svg)](https://github.com/Johboh/EspNowNetwork)


Arduino (using Arduino IDE or PlatformIO) and ESP-IDF (using Espressif IoT Development Framework or PlatformIO) compatible library for setting up a network of ESP NOW nodes

### Usage/Purpose
The use case for the EspNowNetwork is to run a a [ESP-NOW](https://www.espressif.com/en/solutions/low-power-solutions/esp-now) network for battery powered sensors.

The sensors are low power boards that are in deep sleep most of the time, and wake up either due to external interrupt (like a PIR sensor or switch) or perodically based on time. Upon wakeup, they will send their sensors values and go back to sleep. On the receiving side, there is a always powered router board that will receive the sensor values and act on or forward them. In my setup, I forward these to MQTT which I later consumer from [Home Assistant](https://www.home-assistant.io).

Features:
- **Encryption**: ESP-NOW have built in encryption, but that relies on that the host adds all peers to be able to decrypt messages. There is a limit on how many peers one can have when using encryption (17). So instead there is an application layer encryption applied using [GCM](https://en.wikipedia.org/wiki/Galois/Counter_Mode). Each message is unique and valid only once to prevent replay attacks.
- **Generic firmware**: For boards that do the same thing (e.g. they have the same hardware), the same firmware can be used for all of them. No unique ID is required to be programmed into each board/node.
- **Over The Air/OTA**: A node can be updated Over The Air. The node report their firmware version upon handsake, and the host can send back wifi credentials and an URL where to download the new firmware. The node will download the firmware, write it and restart.

### Installation
There are a set if different variants of this library you can use.
- **EspNowNetworkNode**: This is the node only code. Use this in your node project.
  - PlatformIO: Add the following to `libs_deps`:
    ```
    Johboh/EspNowNetworkNode@^0.6.1
    ```
  - Add to `idf_component.yml` next to your main component:
    ```
    dependencies:
      johboh/EspNowNetworkNode:
        version: ">=0.6.1"
    ```
- **EspNowNetworkHost**: This is the host only code. It contains the basics for receiving messages from the node. But for a real life environment, you want to handle different kind of nodes and application messages, as well as firmware/OTA support for the nodes. For that, you can instead use the **EspNowNetworkHostDriver** (see below).
  - PlatformIO: Add the following to `libs_deps`:
    ```
    Johboh/EspNowNetworkHost@^0.6.1
    ```
  - Add to `idf_component.yml` next to your main component:
    ```
    dependencies:
      johboh/EspNowNetworkHost:
        version: ">=0.6.1"
    ```
- **EspNowNetworkHostDriver**: Same as EspNowNetworkHost, but with support for "virtual" nodes and firmware/OTA updates. See the [Arduino](examples/arduino/host_driver/Host.ino) or [ESP-IDF](examples/espidf/host_driver/main/main.cpp) example.
  - PlatformIO: Add the following to `libs_deps`:
    ```
    Johboh/EspNowNetworkHostDriver@^0.6.1
    ```
  - Add to `idf_component.yml` next to your main component:
    ```
    dependencies:
      johboh/EspNowNetworkHostDriver:
        version: ">=0.6.1"
    ```
- **EspNowNetwork**: This is the legacy full library consiting of both the node and the host code (but not the host driver). Not recommended for new projects.

### Examples
- [Arduino: Host](examples/arduino/host/Host.ino)
- [Arduino: Host Driver](examples/arduino/host_driver/Host.ino)
- [Arduino: Node](examples/arduino/node/Node.ino)
- [ESP-IDF: Host](examples/espidf/host/main/main.cpp)
- [ESP-IDF: Host Driver](examples/espidf/host_driver/main/main.cpp)
- [ESP-IDF: Node](examples/espidf/node/main/main.cpp)

### Parition table (for the Node)
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
- *For Host Driver, an MQTT implementation is required.* There is a copy of [IMQTTRemote](https://github.com/Johboh/MQTTRemote/blob/main/includes/IMQTTRemote.h) in this library from [Johboh/MQTTRemote](https://github.com/Johboh/MQTTRemote). You can either add a dependency on [MQTTRemote](https://github.com/Johboh/MQTTRemote) to get a fully working MQTT client (the examples are using this dependency), or you can implement/adapt/forward to your own MQTT implementation. This library only depend on the [IMQTTRemote](https://github.com/Johboh/MQTTRemote/blob/main/includes/IMQTTRemote.h) interface.
- Needs C++17 for `std::optional`.
  - For platformIO in `platformio.ini`:
    ```C++
    build_unflags=-std=gnu++11 # "Disable" C++11
    build_flags=-std=gnu++17 # "Enable" C++17
    ```
