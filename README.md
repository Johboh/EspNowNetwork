# EspNowNetwork
[![Test](https://github.com/Johboh/EspNowNetwork/actions/workflows/test.yaml/badge.svg)](https://github.com/Johboh/EspNowNetwork/actions/workflows/test.yaml)
[![GitHub release](https://img.shields.io/github/release/Johboh/EspNowNetwork.svg)](https://github.com/Johboh/EspNowNetwork/releases)


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
In your existing `idf_component.yml` or in a new `idf_component.yml` next to your main component:
```
dependencies:
  johboh/EspNowNetwork:
    version: ">=0.4.0"
```

### Example (Arduino framework)
See [host example](examples/Host/Host.ino) and [node example](examples/Node/Node.ino).

### Functionallity verified on the following platforms and frameworks
- ESP32 (tested with platform I/O [espressif32@6.4.0](https://github.com/platformio/platform-espressif32) / [arduino-esp32@2.0.11](https://github.com/espressif/arduino-esp32) / [ESP-IDF@5.1.1](https://github.com/espressif/esp-idf) on ESP32-S2 and ESP32-C3)
- ESP-IDF 5.1.2 using ESP32-C6

Newer version most probably work too, but they have not been verified.

### Dependencies
- Needs C++17 for `std::optional`.
  - For platform I/O in `platformio.ini`:
    ```C++
    build_unflags=-std=gnu++11 # "Disable" C++11
    build_flags=-std=gnu++17 # "Enable" C++17
    ```
