# ESP32-LaptopFan Firmware

[中文](README.md) | **English**

> Firmware for the open-source hardware by Bilibili UP主 [垃圾研究社](https://www.bilibili.com/video/BV1Lr421M7u2/). Works with the host software [anlerway/Bluetooth-FanControl](https://github.com/anlerway/Bluetooth-FanControl) to form a complete laptop fan control solution.

![ESP32](https://img.shields.io/badge/ESP32-SSD1306-orange) ![Arduino](https://img.shields.io/badge/Arduino-IDE-blue)

## Physical Photos

![Firmware Photo 1](images/firmware-1.jpg)

![Firmware Photo 2](images/firmware-2.jpg)

## Features

- **Redesigned OLED main interface**: status bar (date/time + Bluetooth status) + 2×2 four‑icon dashboard.
- **Time display**: supports `TIME` command from the host for time synchronisation.
- **Dual communication**: Classic Bluetooth SPP (COM mode) / BLE (Nordic UART service).
- **Bluetooth visibility**: can be toggled hidden/visible to prevent unwanted pairing by others.
- **Button interaction**: menu / auto / manual / speed ±, long‑press to enter sleep mode.
- Receives PWM / temperature / time‑sync commands from the host; responds to `STATUS?` queries.

## Hardware Button Logic

The device has **5 buttons** (from left to right) and operates in two modes: home and menu.

### Home Mode (default)

| Button | Function |
| --- | --- |
| Button 1 | Decrease fan speed (long‑press for continuous adjustment) |
| Button 2 | Increase fan speed (long‑press for continuous adjustment) |
| Button 3 | Switch to **Manual Mode** |
| Button 4 | Switch to **Auto Mode** |
| Button 5 | Long‑press 3 seconds to enter **Sleep**; press again to wake up |

### Menu Mode

Long‑press Button 5 to enter the menu:

| Button | Function |
| --- | --- |
| Button 1 | Decrease value (long‑press for continuous adjustment) |
| Button 2 | Increase value (long‑press for continuous adjustment) |
| Button 3 | Move up |
| Button 4 | Move down |
| Button 5 | Exit menu and return to home |

## Menu Options

| Menu Item | Description |
| --- | --- |
| **BT Visible** (Bluetooth visibility) | After pairing, it is recommended to enable **hidden** mode to prevent others from pairing. Existing connections and auto‑reconnection are not affected. |
| **BT Mode** (Connection mode) | It is recommended to select **BLE** mode for low power consumption and no need to manually choose a serial port. |
| **Brightness** (Screen brightness) | Adjust OLED display brightness. |
| **Fan Freq** (PWM frequency) | Keep the default value. If the fan does not spin, try adjusting this parameter. |

## Flashing Guide

### Method 1: Flash Pre‑compiled BIN Files

1. Download the compiled release from GitHub Releases: <https://github.com/anlerway/ESP32-LaptopFan/releases> and extract it.
2. Download the official Espressif flashing tool: [Flash Download Tool](https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/production_stage/tools/flash_download_tool.html).
3. Connect the ESP32 board to your computer via USB and verify that a serial port appears in Device Manager.
4. Open the flashing tool, select chip model **ESP32**, load the extracted `.bin` firmware files, configure the flash addresses, and click start.

| Flash Address | Firmware File |
| --- | --- |
| 0x1000 | LAPTOP_FAN.ino.bootloader.bin |
| 0x8000 | LAPTOP_FAN.ino.partitions.bin |
| 0x10000 | LAPTOP_FAN.ino.bin |

### Method 2: Build and Flash from Source (Arduino IDE)

1. Download the source code from [anlerway/ESP32-LaptopFan](https://github.com/anlerway/ESP32-LaptopFan) and extract it.
2. Install **Arduino IDE 2.3.10**.
3. Install the ESP32 board package: search for `esp32 by Espressif` (version **2.0.4**) in the Boards Manager and install it.
4. Install the required libraries: `Adafruit SSD1306` and `Adafruit GFX`.
5. Open `LAPTOP_FAN.ino` in Arduino IDE.
6. Board settings: select **ESP32 Dev Module** (4MB Flash, default partition scheme), and select the correct serial port.
7. Click Upload to compile and flash the firmware.

## Hardware Pinout (config.h)

| Definition | Pin | Function |
| --- | --- | --- |
| `PIN_FAN` | 5 | Fan PWM (LEDC, default 15 kHz) |
| `PIN_RELAY` | 19 | Relay |
| `PIN_BTN1` ~ `PIN_BTN5` | 12 / 13 / 14 / 15 / 16 | Menu / Auto / Manual / Speed+ / Speed- |
| OLED | I2C address `0x3C` | SSD1306 128×64 |

## Communication Protocol

One‑line ASCII text frames, terminated with `\r\n`:

- Host → Firmware: `PWM:<0-100>`, `TIME:yyyy/MM/dd HH:mm:ss`, `TEMP:<cpu>[,<gpu>]`;
- Firmware → Host: `READY` (connection handshake), `STATUS:...` (response to query).

## Host Software Integration

- **BLE mode**: device name `LAPTOP-FAN`; the host software searches and connects.
- **COM mode**: use USB serial or classic Bluetooth SPP virtual COM port (default baud rate 115200).
- The host software’s settings page can switch Bluetooth modes and perform automatic time synchronisation.

## Disclaimer

Fan control involves hardware cooling. Please adjust speed curves cautiously. The firmware is provided as‑is; use at your own risk.
## Disclaimer

Fan control affects hardware cooling; adjust curves carefully. Firmware is provided as-is; use at your own risk.
