# ESP32-LaptopFan 固件

[English](README.en.md) | **中文**

> 适配 UP 主[垃圾研究社](https://www.bilibili.com/video/BV1Lr421M7u2/)开源硬件的固件，配合上位机 [anlerway/Bluetooth-FanControl](https://github.com/anlerway/Bluetooth-FanControl) 使用，组成完整的笔记本风扇控制方案。

![ESP32](https://img.shields.io/badge/ESP32-SSD1306-orange) ![Arduino](https://img.shields.io/badge/Arduino-IDE-blue) ![Flasher](https://img.shields.io/badge/Flasher-2.0.0-brightgreen)

## 仓库结构

| 路径 | 说明 |
| --- | --- |
| `LAPTOP_FAN/` | 固件源码（Arduino sketch，用 Arduino IDE 打开 `LAPTOP_FAN.ino`） |
| `flasher/` | 配套的 Windows 固件烧录器：不用装 Arduino IDE，可自定义蓝牙名称，内置常用串口驱动 |
| `images/` | 文档用的实物图 |

## 实物图

![固件实物 1](images/firmware-1.jpg)

![固件实物 2](images/firmware-2.jpg)

## 功能

- **重制 OLED 主界面**：状态栏（日期时间 + 蓝牙状态）+ 2×2 四宫格图标控制台；
- **时间显示**：支持上位机 `TIME` 指令校时；
- **双通信**： 经典蓝牙 SPP（COM 模式）/ BLE（Nordic UART 服务）；
- **隐藏蓝牙**：可切换隐藏/可见，防止蓝牙被他人乱连；
- **自定义蓝牙名**：`config.h` 的 `BT_DEVICE_NAME` 可在编译期覆盖，配合烧录器无需改源码即可改名；
- **按键交互**：菜单 / 自动 / 手动 / 转速 ±，长按设置休眠；
- 接收上位机 PWM / 温度 / 校时指令，响应 `STATUS?` 查询。

## 烧录指南

### 方式一：用配套烧录器（推荐）

不用装 Arduino IDE、不需要工具链，插上数据线点一下就好，还能顺手改蓝牙名称。

1. 下载 `FanControl-Flasher` 发布包（见 [Releases](<https://github.com/anlerway/ESP32-LaptopFan/releases>)），
   解压后运行 `FanControl-Flasher.exe`（需要管理员权限，会弹一次 UAC）；
2. 用数据线把小板插到电脑，点界面上的「刷新」，在串口下拉里选中它；
3. 需要改名就填「蓝牙名称」，不改就用默认；
4. 按下单片机「BOOT」点「开始烧录」，等进度条走完即可。

若界面提示"检测到 xxx 但没有串口：缺少驱动"，直接点「安装串口驱动」，内置 CH340/CH341、
CH343/CH9102、CP210x、FTDI、PL2303 五种常见芯片的官方签名驱动。

![烧录器界面](flasher/docs/screenshot-zh.png)

烧录器源码、实现细节（占位名替换 + 镜像完整性重算）和构建方法见 [flasher/README.md](flasher/README.md)。

### 方式二：Flash Download Tool（乐鑫官方工具）

1. 编译产物下载：[GitHub Releases](<https://github.com/anlerway/ESP32-LaptopFan/releases>) 并解压。
2. 下载乐鑫官方烧录工具 [Flash Download Tool](https://docs.espressif.com/projects/esp-test-tools/zh_CN/latest/esp32/production_stage/tools/flash_download_tool.html)。
3. 将 ESP32 开发板通过 USB 连接到电脑，确认设备管理器中识别到串口。
4. 打开烧录工具，选择芯片型号 **ESP32**，加载解压出的 `.bin` 固件文件，配置烧录参数后点击开始烧录。

| 烧录地址 | 烧录固件 |
| --- | --- |
| 0x1000 | LAPTOP_FAN.ino.bootloader.bin |
| 0x8000 | LAPTOP_FAN.ino.partitions.bin |
| 0x10000 | LAPTOP_FAN.ino.bin |

### 方式三：源码编译烧录（Arduino IDE）

1. 源码下载：`git clone https://github.com/anlerway/ESP32-LaptopFan`。
2. 安装 **Arduino IDE 2.3.10**。
3. 安装 ESP32 板包：在开发板管理器中搜索并安装 `esp32 by Espressif`（版本 **2.0.4**）。
4. 安装依赖库：`Adafruit SSD1306`、`Adafruit GFX`。
5. 用 Arduino IDE 打开 `LAPTOP_FAN/LAPTOP_FAN.ino`。
6. 开发板设置：选择 **ESP32 Dev Module**（4MB Flash，默认分区），端口选择对应串口。
7. 点击上传即可编译并烧录。

## 硬件操作逻辑

设备共有 **5 个按钮**（从左到右），分主页和菜单两种模式：

### 主页模式（默认）

| 按钮 | 功能 |
| --- | --- |
| 按钮1 | 转速减（支持长按连续调节） |
| 按钮2 | 转速加（支持长按连续调节） |
| 按钮3 | 切换至 **手动模式** |
| 按钮4 | 切换至 **自动模式** |
| 按钮5 | 长按 3 秒进入 **休眠**；休眠状态下再次点击 **唤醒** |

### 菜单模式

长按按钮5进入菜单模式后：

| 按钮 | 功能 |
| --- | --- |
| 按钮1 | 调节减（支持长按连续调节） |
| 按钮2 | 调节加（支持长按连续调节） |
| 按钮3 | 上选择 |
| 按钮4 | 下选择 |
| 按钮5 | 退出菜单，返回主页 |

## 菜单功能介绍

| 菜单项 | 说明 |
| --- | --- |
| **BT Visible**（蓝牙可见性） | 配对成功后建议开启 **隐藏蓝牙**，防止其他用户配对连接，不影响已有连接和自动重连 |
| **BT Mode**（连接模式） | 建议选择 **BLE** 模式，低功耗且无需手动选择串口 |
| **Brightness**（屏幕亮度） | 调节 OLED 屏幕显示亮度 |
| **Fan Freq**（PWM 频率） | 保持默认值即可；如遇风扇不转，可尝试调节此参数 |

## 硬件接线（config.h）

| 定义 | 引脚 | 功能 |
| --- | --- | --- |
| `PIN_FAN` | 5 | 风扇 PWM（LEDC，默认 15kHz） |
| `PIN_RELAY` | 19 | 继电器 |
| `PIN_BTN1` ~ `PIN_BTN5` | 12 / 13 / 14 / 15 / 16 | 菜单 / 自动 / 手动 / 转速+ / 转速- |
| OLED | I2C 地址 `0x3C` | SSD1306 128×64 |

## 通信协议

一行 ASCII 文本帧，`\r\n` 结尾：

- 上位机 → 固件：`PWM:<0-100>`、`TIME:yyyy/MM/dd HH:mm:ss`、`TEMP:<cpu>[,<gpu>]`；
- 固件 → 上位机：`READY`（连接握手）、`STATUS:...`（查询应答）。

BLE 使用 Nordic UART 服务（`6e400001-...`，写 `6e400002`、通知 `6e400003`）。

## 上位机联动

- **BLE 模式**：设备名默认 `LAPTOP-FAN`（可用烧录器改），上位机搜索并连接；
- **COM 模式**：使用 USB 串口或经典蓝牙 SPP 虚拟串口连接（默认波特率 115200），设备名为 `<名字>-COM`；
- 上位机设置页可切换蓝牙模式、固件校时自动进行。

## 免责声明

软件烧录涉及管理员权限，可能会被杀软报毒，自行加入白名单
