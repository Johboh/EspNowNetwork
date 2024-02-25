# EspNowNetwork
[![Platform I/O CI](https://github.com/Johboh/EspNowNetwork/actions/workflows/platformio.yaml/badge.svg)](https://registry.platformio.org/libraries/johboh/EspNowNetwork)
[![ESP-IDF CI](https://github.com/Johboh/EspNowNetwork/actions/workflows/espidf.yaml/badge.svg)](https://components.espressif.com/components/johboh/espnownetwork)
[![GitHub release](https://img.shields.io/github/release/Johboh/EspNowNetwork.svg)](https://github.com/Johboh/EspNowNetwork/releases)
[![Clang-format](https://github.com/Johboh/EspNowNetwork/actions/workflows/clang-format.yaml/badge.svg)](https://github.com/Johboh/EspNowNetwork)


Arduino (using Arduino IDE or Platform I/O) and ESP-IDF (using Espressif IoT Development Framework or Platform I/O) compatible library for setting up a network of ESP NOW nodes

### Usage/Purpose
The use case for the EspNowNetwork is to run a a [ESP-NOW](https://www.espressif.com/en/solutions/low-power-solutions/esp-now) network for battery powered sensors.

The sensors are low power boards that are in deep sleep most of the time, and wake up either due to external interrupt (like a PIR sensor or switch) or perodically based on time. Upon wakeup, they will send their sensors values and go back to sleep. On the receiving side, there is a always powered router board that will receive the sensor values and act on or forward them. In my setup, I forward these to MQTT which I later consumer from [Home Assistant](https://www.home-assistant.io).

Features:
- **Encryption**: ESP-NOW have built in encryption, but that relies on that the host adds all peers to be able to decrypt messages. There is a limit on how many peers one can have when using encryption (17). So instead there is an application layer encryption applied using [GCM](https://en.wikipedia.org/wiki/Galois/Counter_Mode). Each message is unique and valid only once to prevent replay attacks.
- **Generic firmware**: For boards that do the same thing (e.g. they have the same hardware), the same firmware can be used for all of them. No unique ID is required to be programmed into each board/node.
- **Over The Air/OTA**: A node can be updated Over The Air. The node report their firmware version upon handsake, and the host can send back wifi credentials and an URL where to download the new firmware. The node will download the firmware, write it and restart.

### Installation
#### Platform I/O (Arduino or ESP-IDF):
Add the following to `libs_deps`:
```
   Johboh/EspNowNetwork
```
#### Espressif IoT Development Framework:
You have three options here. You can either use the full library with both the host and the node code, or only the host or the node variant. Suggestion is to only use the host library on the host, and only the node for the node.

In your existing `idf_component.yml` or in a new `idf_component.yml` next to your main component:

*Host only*
```
dependencies:
  johboh/EspNowNetworkHost:
    version: ">=0.6.1"
```
*Node only*
```
dependencies:
  johboh/EspNowNetworkNode:
    version: ">=0.6.1"
```
*Full*
```
dependencies:
  johboh/EspNowNetwork:
    version: ">=0.6.1"
```

### Examples
- [Arduino: Host](examples/arduino/host/Host.ino)
- [Arduino: Node](examples/arduino/node/Node.ino)
- [ESP-IDF: Host](examples/espidf/host/main/main.cpp)
- [ESP-IDF: Host](examples/espidf/node/main/main.cpp)

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
To set partition table, save above in a file called `partitions_with_ota.csv`. For ESP-IDF, specify to use this one using menuconfig. For platform I/O, add the following to your `platformio.ini`: `board_build.partitions = partitions_with_ota.csv`

### Functionallity verified on the following platforms and frameworks
- ESP32 (tested with platform I/O [espressif32@6.4.0](https://github.com/platformio/platform-espressif32) / [arduino-esp32@2.0.11](https://github.com/espressif/arduino-esp32) / [ESP-IDF@4.4.6](https://github.com/espressif/esp-idf) / [ESP-IDF@5.1.2](https://github.com/espressif/esp-idf) / [ESP-IDF@5.2.0](https://github.com/espressif/esp-idf) on ESP32-S2, ESP32-C3 and ESP32-C6)

Newer version most probably work too, but they have not been verified.

### Dependencies
- Needs C++17 for `std::optional`.
  - For platform I/O in `platformio.ini`:
    ```C++
    build_unflags=-std=gnu++11 # "Disable" C++11
    build_flags=-std=gnu++17 # "Enable" C++17
    ```
