# Firmware HTTP Server
HTTP firmware server compatible with the default implementation of the [Firmware Checker](../src/host_driver/FirmwareChecker.h), used by the [Host Driver](../src/host_driver/HostDriver.h).

To upload firmware to the server, one can use the including `upload.py` script as follows:

```bash
python3 upload.py --base_url http://192.168.1.100:8080/foot_pedal/esp32s2 --firmware_version_file ./build/foot_pedal_firmware_version.txt ./build/foot_pedal.bin
```
In this example, the script will upload `foot_pedal.bin`, a `firmware_version.txt` and a `firmware.md5` to the server at `http://192.168.1.100:8080/foot_pedal/esp32s2`.

The hardware part (`esp32s2`) is not needed if you only have one type of hardware for your device type. In this case, the `hardware` should not be set in your [Device](../src/host_driver/Device.h) implementation.

### Firmware Kicker
If using the [Firmware Kicker](../src/host_driver/FirmwareKicker.h) to nudge the host that there is a new firmware (instead of waiting for the next perodic check), you can use the `upload.py` script as follows to pass a kicker URL to your host (`192.168.1.200`):

```bash
python3 upload.py --base_url http://192.168.1.100:8080/foot_pedal/esp32s2 --kicker_url http://192.168.1.200:82?device=foot_pedal&hardware=esp32s2 --firmware_version_file ./build/foot_pedal_firmware_version.txt ./build/foot_pedal.bin
```