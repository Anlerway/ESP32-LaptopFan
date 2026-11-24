#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace fc {

// 发布固件按 24 字符占位名编译，烧录时把用户输入的名字写进这两个固定长度的槽位。
constexpr wchar_t kNamePlaceholder[] = L"FAN-NAME-PLACEHOLDER-XXX";
constexpr size_t kMaxNameLength = 24;

struct FlashImage {
    std::wstring label;       // bootloader / partitions / app
    uint32_t offset = 0;
    std::vector<uint8_t> data;
};

struct FirmwareBundle {
    FlashImage bootloader;
    FlashImage partitions;
    FlashImage app;
};

// 从 <exe 目录>\firmware\ 读取三个烧录镜像（bootloader / partitions / app）。
bool LoadFirmware(FirmwareBundle& bundle, std::wstring* error);

// 把 name 写入 app 镜像里的两个占位槽（BLE 名与 -COM 名），长度不变，
// 并重算镜像尾部的校验字节与 SHA-256（ESP32 启动时会校验，改过就该重算）。
bool PatchDeviceName(std::vector<uint8_t>& app, const std::wstring& name, std::wstring* error);

// 重算 ESP32 应用镜像的结构校验：段数据 XOR 校验字节 + 末尾 32 字节 SHA-256。
bool FixImageIntegrity(std::vector<uint8_t>& image, std::wstring* error);

// 名字合法性：1..24 个可打印 ASCII，不能带空格和引号。
bool IsValidDeviceName(const std::wstring& name, std::wstring* error);

}  // namespace fc
