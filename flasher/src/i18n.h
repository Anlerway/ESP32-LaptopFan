#pragma once
#include <windows.h>

#include <string>

namespace fc {

// 界面文案表：一行一条（id, 中文, English）。新增文案只改这张表。
#define FC_STRINGS(X)                                                                                       \
    X(AppTitle, L"FanControl 固件烧录器", L"FanControl Firmware Flasher")                                    \
    X(AppSubtitle, L"版本 %s  ·  为 ESP32 小板写入固件与自定义蓝牙名称",                                     \
      L"Version %s  ·  Flash firmware and set the Bluetooth name of the ESP32 board")                        \
    X(SectionDevice, L"设备", L"Device")                                                                     \
    X(NoPort, L"没有找到串口设备", L"No serial port found")                                                  \
    X(Refresh, L"刷新", L"Refresh")                                                                          \
    X(PortHint, L"把小板用数据线插到电脑上，再选择对应的串口。",                                             \
      L"Connect the board with a USB data cable, then choose its port.")                                     \
    X(SectionName, L"蓝牙名称", L"Bluetooth name")                                                           \
    X(NameHint, L"1–24 个字符，只能使用英文字母、数字和 - _ 等符号。",                                        \
      L"1–24 characters: letters, digits and symbols such as - _ only.")                                     \
    X(SectionFlash, L"烧录", L"Flashing")                                                                    \
    X(WaitStart, L"等待开始", L"Ready")                                                                      \
    X(StartFlash, L"开始烧录", L"Start flashing")                                                            \
    X(CancelFlash, L"取消烧录", L"Cancel")                                                                   \
    X(StatusNoChip, L"没有检测到需要安装驱动的设备。", L"No device needs a driver.")                         \
    X(StatusNoBundled, L"这个版本没有内置 %s 的驱动，请到厂商页面下载安装后再点刷新。",                      \
      L"This build has no bundled driver for %s — install the vendor driver, then press Refresh.")           \
    X(StatusNeedAdmin, L"安装驱动需要管理员权限，已取消。",                                                   \
      L"Installing the driver needs administrator rights — cancelled.")                                      \
    X(StatusInstalling, L"正在安装驱动……", L"Installing the driver…")                                       \
    X(StatusInstalled, L"驱动安装结束，请选择串口。", L"Driver install finished — choose the port.")          \
    X(ChipMissing, L"检测到 %s，但没有串口：缺少驱动。",                                                      \
      L"Found %s, but it has no serial port: the driver is missing.")                                        \
    X(ChipMissingHint, L"装完驱动后点上面的「刷新」。", L"Install the driver, then press Refresh above.")     \
    X(InstallDriver, L"安装串口驱动", L"Install serial driver")                                               \
    X(FlashDone, L"烧录完成，可以拔掉数据线了。", L"Flashing finished — you can unplug the cable now.")       \
    X(FlashOk, L"烧录成功。", L"Flashing succeeded.")                                                        \
    X(FlashFail, L"烧录失败。", L"Flashing failed.")                                                         \
    X(FlashFailTitle, L"烧录失败", L"Flashing failed")                                                       \
    X(NoPortStatus, L"没有找到串口设备，请先插上小板。", L"No serial port found — plug in the board first.")  \
    X(Preparing, L"准备中…", L"Preparing…")                                                                  \
    X(Cancelling, L"正在取消…", L"Cancelling…")                                                              \
    X(ErrPipe, L"创建管道失败。", L"Could not create the pipe.")                                             \
    X(ErrSpawn, L"启动 esptool 失败（错误 %lu）。", L"Could not start esptool (error %lu).")                  \
    X(ErrNoEsptool, L"找不到 esptool.exe，请确认 tools 文件夹与烧录器放在一起。",                             \
      L"esptool.exe not found — keep the tools folder next to the flasher.")                                 \
    X(StepReadFw, L"读取固件…", L"Reading firmware…")                                                        \
    X(LogAppSize, L"固件应用大小 %zu KB，蓝牙名称 %s", L"Application image %zu KB, Bluetooth name %s")         \
    X(ErrNoTemp, L"无法创建临时文件夹（已尝试 %s）。请检查磁盘空间和文件夹权限。",                            \
      L"Could not create a temp folder (tried %s). Check free space and folder permissions.")                \
    X(LogTempDir, L"临时目录 %s", L"Temp folder %s")                                                         \
    X(ErrWriteFw, L"写入临时固件失败：%s（错误码 %lu）。请检查磁盘空间和文件夹权限。",                        \
      L"Could not write the temp firmware: %s (error %lu). Check free space and folder permissions.")        \
    X(StepConnect, L"连接设备并烧录…", L"Connecting and flashing…")                                          \
    X(ErrCancelled, L"已取消烧录。", L"Flashing cancelled.")                                                 \
    X(ErrExitCode, L"esptool 退出码 %lu。", L"esptool exited with code %lu.")                                 \
    X(FwNotFound, L"找不到固件文件：%s。请确认 firmware 文件夹与烧录器放在一起。",                            \
      L"Firmware file not found: %s — keep the firmware folder next to the flasher.")                        \
    X(FwEmpty, L"固件文件为空：%s", L"Firmware file is empty: %s")                                           \
    X(FwReadFail, L"读取固件文件失败：%s", L"Could not read the firmware file: %s")                          \
    X(ImgNotApp, L"内置镜像不是 ESP32 应用镜像。", L"The bundled image is not an ESP32 application image.")   \
    X(ImgSegTableOut, L"镜像段表越界。", L"The image segment table is out of range.")                         \
    X(ImgSegDataOut, L"镜像段数据越界。", L"The image segment data is out of range.")                         \
    X(ImgSizeMismatch, L"镜像长度不符（算出 %zu，实际 %zu）。", L"Image length mismatch (expected %zu, got %zu).") \
    X(ImgShaFail, L"重算镜像 SHA-256 失败。", L"Could not recompute the image SHA-256.")                      \
    X(NameEmpty, L"请填写蓝牙名称。", L"Enter a Bluetooth name.")                                            \
    X(NameTooLong, L"名称最多 %zu 个字符。", L"The name is limited to %zu characters.")                       \
    X(NameBadChar, L"名称只能用英文字母、数字和常见符号。", L"Use letters, digits and common symbols only.")  \
    X(NameBadSpace, L"名称不能包含空格、引号或斜杠。", L"The name cannot contain spaces, quotes or slashes.") \
    X(NameSlotMissing, L"固件里找不到占位名称，请用 build-firmware.bat 重新生成固件。",                       \
      L"Placeholder name not found in the firmware — rebuild it with build-firmware.bat.")                    \
    X(NameWriteFail, L"写入蓝牙名称失败。", L"Could not write the Bluetooth name.")                            \
    X(NameComWriteFail, L"写入串口名称失败。", L"Could not write the serial-port name.")                      \
    X(DrvNoInf, L"驱动文件夹里没有 INF 文件：%s", L"No INF file in the driver folder: %s")                    \
    X(DrvPnpExit, L"pnputil 退出码 %lu（%s）", L"pnputil exited with code %lu (%s)")                          \
    X(DrvInstallFail, L"驱动安装失败：%s", L"Driver installation failed: %s")                                 \
    X(DrvFolderMissing, L"drivers\\%s 文件夹里没有驱动文件，请确认驱动文件夹与烧录器放在一起。",            \
      L"No driver files in drivers\\%s — keep the drivers folder next to the flasher.")                     \
    X(DrvInstalled, L"串口驱动已安装，请回到烧录器点「刷新」。",                                             \
      L"The serial driver is installed — go back to the flasher and press Refresh.")                          \
    X(ErrUiInit, L"界面初始化失败，请确认系统支持 Direct2D。",                                                \
      L"UI initialization failed — check that Direct2D is available.")                                         \
    X(SourceLink, L"开源地址", L"Source code")

enum class Str {
#define FC_ENUM_ROW(name, zh, en) name,
    FC_STRINGS(FC_ENUM_ROW)
#undef FC_ENUM_ROW
    Count
};

const wchar_t* T(Str id);

void SetUiLanguage(bool chinese);
bool IsChineseUi();
// 从注册表读上次的选择，没有就按系统界面语言判断。
void InitLanguage();
void SaveLanguage(bool chinese);

}  // namespace fc
