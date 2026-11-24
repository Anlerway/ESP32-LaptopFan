#include "drivers.h"

#include <windows.h>

#include <setupapi.h>
#include <devguid.h>

#include <algorithm>
#include <cstring>

#include "i18n.h"
#include "util.h"

#pragma comment(lib, "setupapi.lib")

namespace fc {
namespace {

struct VipEntry {
    unsigned vid;
    unsigned pidLo;
    unsigned pidHi;
    SerialChip chip;
};

// 常见下载芯片的 VID/PID 区间，与 drivers\<芯片> 里 INF 的硬件 ID 一一对应。
// 顺序有意义：CH340/CH341 与 CH343 都有 0x7523，先匹配谁就装谁的驱动，CH340 归 ch34x。
// ESP32-S3/C3 的原生 USB 走系统自带 usbser，不需要驱动，故不在此表。
constexpr VipEntry kVips[] = {
    {0x1A86, 0x5523, 0x5523, SerialChip::Ch34x},  // CH341A
    {0x1A86, 0x7522, 0x7523, SerialChip::Ch34x},  // CH340K / CH340 / CH341
    {0x1A86, 0xE523, 0xE523, SerialChip::Ch34x},  // CH341 复合设备
    {0x4348, 0x5523, 0x5523, SerialChip::Ch34x},  // CH341 旧 VID

    {0x10C4, 0xEA60, 0xEA60, SerialChip::Cp210x}, // CP2102 / CP2109
    {0x10C4, 0xEA63, 0xEA63, SerialChip::Cp210x}, // CP2108
    {0x10C4, 0xEA70, 0xEA71, SerialChip::Cp210x}, // CP2105
    {0x10C4, 0xEA7A, 0xEA7B, SerialChip::Cp210x}, // CP210x 变体

    {0x0403, 0x0000, 0x0000, SerialChip::Ftdi},   // FTDI 复合设备
    {0x0403, 0x0FEC, 0x0FEC, SerialChip::Ftdi},   // FT-X
    {0x0403, 0x6001, 0x6001, SerialChip::Ftdi},   // FT232R
    {0x0403, 0x6010, 0x6011, SerialChip::Ftdi},   // FT2232 / FT4232
    {0x0403, 0x6014, 0x6015, SerialChip::Ftdi},   // FT232H / FT231X
    {0x0403, 0x601B, 0x601C, SerialChip::Ftdi},   // FT230X 等
    {0x0403, 0x6031, 0x604F, SerialChip::Ftdi},   // FT 新系列

    {0x067B, 0x2303, 0x2304, SerialChip::Pl2303}, // PL2303 HXA / XA
    {0x067B, 0x2320, 0x2323, SerialChip::Pl2303}, // PL2303GC 等
    {0x067B, 0x23A3, 0x23F3, SerialChip::Pl2303}, // PL2303 新系列
    {0x067B, 0xFFFF, 0xFFFF, SerialChip::Pl2303}, // 未烧写 EEPROM

    {0x1A86, 0x55D2, 0x55D8, SerialChip::Ch9102}, // CH342 / CH343 / CH9102
    {0x1A86, 0x55DA, 0x55DB, SerialChip::Ch9102},
    {0x1A86, 0x55DD, 0x55DF, SerialChip::Ch9102},
    {0x1A86, 0x55E7, 0x55EC, SerialChip::Ch9102}, // CH9101 等
    {0x1A86, 0x55EF, 0x55EF, SerialChip::Ch9102},
    {0x1A86, 0x5610, 0x5610, SerialChip::Ch9102},
    {0x1A86, 0x7523, 0x7523, SerialChip::Ch9102}, // CH343P 复用 0x7523
};

constexpr const wchar_t* kChipFolders[] = {L"ch34x", L"ch9102", L"cp210x", L"ftdi", L"pl2303"};

bool RunAndWait(const std::wstring& command, DWORD timeoutMs, DWORD* exitCode) {
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION info{};
    std::wstring mutableCommand = command;
    if (!::CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                          nullptr, &startup, &info)) {
        return false;
    }
    const DWORD waited = ::WaitForSingleObject(info.hProcess, timeoutMs);
    if (waited == WAIT_TIMEOUT) {
        ::TerminateProcess(info.hProcess, 1);
        ::WaitForSingleObject(info.hProcess, 5000);
    }
    DWORD code = 1;
    ::GetExitCodeProcess(info.hProcess, &code);
    ::CloseHandle(info.hThread);
    ::CloseHandle(info.hProcess);
    if (exitCode != nullptr) {
        *exitCode = code;
    }
    return waited != WAIT_TIMEOUT;
}

bool ParseVidPid(const wchar_t* hardwareId, unsigned* vid, unsigned* pid) {
    if (hardwareId == nullptr) {
        return false;
    }
    const wchar_t* v = wcsstr(hardwareId, L"VID_");
    const wchar_t* p = wcsstr(hardwareId, L"PID_");
    if (v == nullptr || p == nullptr) {
        return false;
    }
    *vid = static_cast<unsigned>(wcstoul(v + 4, nullptr, 16));
    *pid = static_cast<unsigned>(wcstoul(p + 4, nullptr, 16));
    return true;
}

bool ChipFromVidPid(unsigned vid, unsigned pid, SerialChip* chip) {
    for (const VipEntry& entry : kVips) {
        if (entry.vid == vid && pid >= entry.pidLo && pid <= entry.pidHi) {
            *chip = entry.chip;
            return true;
        }
    }
    return false;
}

std::wstring ReadProperty(HDEVINFO set, SP_DEVINFO_DATA& data, DWORD property) {
    wchar_t buffer[512] = {};
    DWORD type = 0;
    DWORD needed = 0;
    if (!::SetupDiGetDeviceRegistryPropertyW(set, &data, property, &type,
                                             reinterpret_cast<PBYTE>(buffer), sizeof(buffer), &needed)) {
        return std::wstring();
    }
    return std::wstring(buffer);
}

// 端口类设备（已装驱动）→ 实例 id 与友好名（含 COMx）。
void CollectComPorts(std::vector<std::pair<std::wstring, std::wstring>>* ports) {
    HDEVINFO set = ::SetupDiGetClassDevsW(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE) {
        return;
    }
    SP_DEVINFO_DATA data{};
    data.cbSize = sizeof(data);
    for (DWORD index = 0; ::SetupDiEnumDeviceInfo(set, index, &data); ++index) {
        wchar_t instance[512] = {};
        if (!::SetupDiGetDeviceInstanceIdW(set, &data, instance, 512, nullptr)) {
            continue;
        }
        ports->emplace_back(instance, ReadProperty(set, data, SPDRP_FRIENDLYNAME));
    }
    ::SetupDiDestroyDeviceInfoList(set);
}

}  // namespace

const wchar_t* ChipName(SerialChip chip) {
    switch (chip) {
        case SerialChip::Ch34x:
            return L"CH340/CH341";
        case SerialChip::Ch9102:
            return L"CH9102/CH343";
        case SerialChip::Cp210x:
            return L"CP210x";
        case SerialChip::Ftdi:
            return L"FTDI";
        case SerialChip::Pl2303:
            return L"PL2303";
    }
    return L"Serial";
}

const wchar_t* ChipFolderName(SerialChip chip) {
    const int index = static_cast<int>(chip);
    return (index >= 0 && index < 5) ? kChipFolders[index] : kChipFolders[0];
}

std::wstring DriverFolder(SerialChip chip) {
    return Join(Join(GetModuleDir(), L"drivers"), ChipFolderName(chip));
}

std::vector<ChipProbe> ProbeUsbSerialChips() {
    std::vector<std::pair<std::wstring, std::wstring>> ports;
    CollectComPorts(&ports);

    std::vector<ChipProbe> probes;
    HDEVINFO set = ::SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE) {
        return probes;
    }
    SP_DEVINFO_DATA data{};
    data.cbSize = sizeof(data);
    for (DWORD index = 0; ::SetupDiEnumDeviceInfo(set, index, &data); ++index) {
        const std::wstring hardwareIds = ReadProperty(set, data, SPDRP_HARDWAREID);
        if (hardwareIds.empty()) {
            continue;
        }
        // SPDRP_HARDWAREID 是 REG_MULTI_SZ，第一段就够判断。
        unsigned vid = 0;
        unsigned pid = 0;
        if (!ParseVidPid(hardwareIds.c_str(), &vid, &pid)) {
            continue;
        }
        SerialChip chip = SerialChip::Ch34x;
        if (!ChipFromVidPid(vid, pid, &chip)) {
            continue;
        }
        wchar_t instance[512] = {};
        if (!::SetupDiGetDeviceInstanceIdW(set, &data, instance, 512, nullptr)) {
            continue;
        }
        ChipProbe probe;
        probe.chip = chip;
        probe.instanceId = instance;
        probe.description = ReadProperty(set, data, SPDRP_DEVICEDESC);
        for (const auto& port : ports) {
            if (_wcsicmp(port.first.c_str(), instance) == 0) {
                probe.driverInstalled = true;
                // 友好名形如 "USB-SERIAL CH340 (COM5)"
                const size_t open = port.second.rfind(L"(COM");
                if (open != std::wstring::npos) {
                    const size_t close = port.second.find(L')', open);
                    if (close != std::wstring::npos) {
                        probe.port = port.second.substr(open + 1, close - open - 1);
                    }
                }
                break;
            }
        }
        probes.push_back(probe);
    }
    ::SetupDiDestroyDeviceInfoList(set);
    return probes;
}

bool InstallInfFolder(const std::wstring& dir, std::wstring* error) {
    WIN32_FIND_DATAW found{};
    const std::wstring pattern = Join(dir, L"*.inf");
    HANDLE search = ::FindFirstFileW(pattern.c_str(), &found);
    if (search == INVALID_HANDLE_VALUE) {
        if (error != nullptr) {
            *error = Format(T(Str::DrvNoInf), dir.c_str());
        }
        return false;
    }
    bool any = false;
    bool ok = false;
    std::wstring lastError;
    do {
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            continue;
        }
        any = true;
        const std::wstring inf = Join(dir, found.cFileName);
        DWORD code = 1;
        const std::wstring command = L"pnputil.exe /add-driver \"" + inf + L"\" /install";
        if (RunAndWait(command, 180000, &code) && code == 0) {
            ok = true;
        } else {
            lastError = Format(T(Str::DrvPnpExit), code, found.cFileName);
        }
    } while (::FindNextFileW(search, &found));
    ::FindClose(search);
    if (!any) {
        if (error != nullptr) {
            *error = Format(T(Str::DrvNoInf), dir.c_str());
        }
        return false;
    }
    if (!ok && error != nullptr) {
        *error = Format(T(Str::DrvInstallFail), lastError.c_str());
    }
    return ok;
}

bool HasBundledDriver(SerialChip chip) {
    const std::wstring dir = DriverFolder(chip);
    WIN32_FIND_DATAW found{};
    HANDLE search = ::FindFirstFileW(Join(dir, L"*.inf").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE) {
        return false;
    }
    ::FindClose(search);
    return true;
}

bool InstallBundledDriver(SerialChip chip, std::wstring* error) {
    const std::wstring dir = DriverFolder(chip);
    if (!HasBundledDriver(chip)) {
        if (error != nullptr) {
            *error = Format(T(Str::DrvFolderMissing), ChipFolderName(chip));
        }
        return false;
    }
    return InstallInfFolder(dir, error);
}

}  // namespace fc
