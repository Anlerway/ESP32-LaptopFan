#pragma once
#include <functional>
#include <string>

namespace fc {

struct FlashRequest {
    std::wstring port;         // COM5
    std::wstring name;         // 用户填的蓝牙名称
    std::wstring esptoolPath;  // 空 = 自动查找
    int baud = 460800;
};

struct FlashEvents {
    std::function<void(const std::wstring& step)> onStep;
    std::function<void(int percent)> onPercent;
    std::function<void(const std::wstring& line)> onLog;
    std::function<bool()> cancelled;
};

// 找到 esptool.exe：先找自身目录，再找 Arduino15 里的板包工具。
std::wstring FindEsptool(std::wstring* error);

// 打补丁 → 写临时镜像 → 调 esptool 烧录 → 清理临时文件。
bool FlashFirmware(const FlashRequest& request, const FlashEvents& events, std::wstring* error);

}  // namespace fc
