# EspNowNetwork
[![Test](https://github.com/Johboh/EspNowNetwork/actions/workflows/test.yaml/badge.svg)](https://github.com/Johboh/EspNowNetwork/actions/workflows/test.yaml)
[![GitHub release](https://img.shields.io/github/release/Johboh/EspNowNetwork.svg)](https://github.com/Johboh/EspNowNetwork/releases)


Arduino library for setting up a network of ESP NOW nodes

### Usage
TBD

### Example
See [host example](examples/Host/Host.ino) and [node example](examples/Node/Node.ino).

### Functionallity verified on the following platforms and frameworks
- ESP32 (tested with platform I/O [espressif32@5.3.0](https://github.com/platformio/platform-espressif32) / [arduino-esp32@2.0.6](https://github.com/espressif/arduino-esp32) on ESP32-S2 and ESP32-C3)

Newer version most probably work too, but they have not been verified.

### Dependencies
- Needs C++17 for `std::optional`.
  - For platform I/O in `platformio.ini`:
    ```C++
    build_unflags=-std=gnu++11 # "Disable" C++11
    build_flags=-std=gnu++17 # "Enable" C++17
    ```
