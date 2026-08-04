# ESP32-LaptopFan Firmware

[中文](README.md) | **English**

> Firmware for the open-source hardware by UP主 [垃圾研究社](https://www.bilibili.com/video/BV1Lr421M7u2/), working with the host app [anlerways/Bluetooth-FanControl](https://github.com/anlerways/Bluetooth-FanControl) to form a complete laptop fan control solution.

![ESP32](https://img.shields.io/badge/ESP32-SSD1306-orange) ![Arduino](https://img.shields.io/badge/Arduino-IDE-blue)

## Photos

![Firmware device 1](images/firmware-1.jpg)

![Firmware device 2](images/firmware-2.jpg)

## Features

- **Redesigned OLED UI**: status bar (date/time + Bluetooth state) + 2×2 icon dashboard;
- **Time display**: synced via the host `TIME` command;
- **Dual communication**: classic Bluetooth SPP (COM mode) / BLE (Nordic UART service);
- **Hidden Bluetooth**: switchable hidden/visible to prevent others from connecting;
- **Button controls**: menu / auto / manual / RPM ±, long-press to set sleep;
- Receives PWM / temperature / time commands and answers `STATUS?` queries.

## Wiring (config.h)

| Define | Pin | Function |
| --- | --- | --- |
| `PIN_FAN` | 5 | Fan PWM (LEDC, 15 kHz default) |
| `PIN_RELAY` | 19 | Relay |
| `PIN_BTN1` ~ `PIN_BTN5` | 12 / 13 / 14 / 15 / 16 | Menu / Auto / Manual / RPM+ / RPM- |
| OLED | I2C address `0x3C` | SSD1306 128×64 |

## Environment

- Arduino IDE 2.3.10;
- ESP32 board package: `esp32 by Espressif` (2.0.4);
- Libraries: `Adafruit SSD1306`, `Adafruit GFX` (install via Library Manager);
- Board settings: `ESP32 Dev Module` (4 MB Flash, default partition).

## Build & flash

Source code: [anlerways/ESP32-LaptopFan](https://github.com/anlerways/ESP32-LaptopFan)

1. Open `LAPTOP_FAN.ino` in Arduino IDE;
2. Tools → Board → `ESP32 Dev Module`, select the correct port;
3. Click Upload.

Or flash the prebuilt artifacts directly (`build/esp32.esp32.esp32/`):

Prebuilt artifacts: GitHub Releases (<https://github.com/anlerways/ESP32-LaptopFan/releases>)

```
esptool.py --chip esp32 --port COMx write_flash \
  0x1000  LAPTOP_FAN.ino.bootloader.bin \
  0x8000  LAPTOP_FAN.ino.partitions.bin \
  0x10000 LAPTOP_FAN.ino.bin
```

## Protocol

One ASCII line per frame, terminated with `\r\n`:

- Host → firmware: `PWM:<0-100>`, `TIME:yyyy/MM/dd HH:mm:ss`, `TEMP:<cpu>[,<gpu>]`;
- Firmware → host: `READY` (handshake), `STATUS:...` (query reply).

## Working with the host app

- **BLE mode**: device name `LAPTOP-FAN`, discoverable from the host;
- **COM mode**: USB serial or classic Bluetooth SPP virtual port (115200 baud default);
- The host settings page can switch Bluetooth modes; time sync happens automatically.

## Disclaimer

Fan control affects hardware cooling; adjust curves carefully. Firmware is provided as-is; use at your own risk.
