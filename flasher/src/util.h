#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

namespace fc {

std::string WideToUtf8(const std::wstring& text);
std::wstring Utf8ToWide(const std::string& text);
std::wstring Format(const wchar_t* fmt, ...);
std::wstring Trim(const std::wstring& text);

std::wstring GetModulePath();
std::wstring GetModuleDir();
std::wstring GetTempDir();
std::wstring Join(const std::wstring& left, const std::wstring& right);
bool FileExists(const std::wstring& path);
bool DeleteFileIfExists(const std::wstring& path);
bool WriteFileBytes(const std::wstring& path, const std::vector<uint8_t>& data);
std::wstring ModuleVersion(const std::wstring& path);

struct SerialPortInfo {
    std::wstring device;   // COM5
    std::wstring name;     // USB-SERIAL CH340 (COM5) 里的友好名
};

std::vector<SerialPortInfo> EnumSerialPorts();

}  // namespace fc
