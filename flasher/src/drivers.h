#ifndef FCFLASH_DRIVERS_H
#define FCFLASH_DRIVERS_H

#include <string>
#include <vector>

namespace fc {

// 常见 USB 转串口芯片（ESP32 开发板的下载芯片）。
enum class SerialChip {
    Ch34x,   // CH340 / CH341  1A86:7523
    Ch9102,  // CH9102 / CH343 1A86:55D3 / 1A86:55D4
    Cp210x,  // CP2102 / CP2109 10C4:EA60 / EA70
    Ftdi,    // FT232 / FT231  0403:6001 / 6015
    Pl2303,  // PL2303         067B:2303
};

struct ChipProbe {
    SerialChip chip = SerialChip::Ch34x;
    std::wstring instanceId;   // USB\VID_xxxx&PID_yyyy\...
    std::wstring port;         // 已装驱动时是 COM 口名（COM5），否则为空
    std::wstring description;  // 设备描述，便于界面显示
    bool driverInstalled = false;
};

// 芯片的中英文显示名（不含厂商名）。
const wchar_t* ChipName(SerialChip chip);

// drivers\ 下的子文件夹名：ch34x / ch9102 / cp210x / ftdi / pl2303。
const wchar_t* ChipFolderName(SerialChip chip);

// 该芯片的驱动文件夹（<exe 目录>\drivers\<芯片>）。
std::wstring DriverFolder(SerialChip chip);

// 枚举当前插着的 USB 转串口芯片，装没装驱动都能看出来。
std::vector<ChipProbe> ProbeUsbSerialChips();

// 把 dir 里所有 INF 用 pnputil 装进驱动仓库（需要管理员）。
bool InstallInfFolder(const std::wstring& dir, std::wstring* error);

// 用 <exe 目录>\drivers\<芯片> 下的 INF 安装驱动（需要管理员）。
bool InstallBundledDriver(SerialChip chip, std::wstring* error);

// 该芯片的驱动文件夹里是否有 INF。
bool HasBundledDriver(SerialChip chip);

}  // namespace fc

#endif
