#include "util.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>

#include <devguid.h>
#include <setupapi.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "advapi32.lib")

namespace fc {

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int need = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(need), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), need,
                          nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int need = ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(need), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), need);
    return out;
}

std::wstring Format(const wchar_t* fmt, ...) {
    wchar_t buffer[1024];
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(buffer, _TRUNCATE, fmt, args);
    va_end(args);
    return buffer;
}

std::wstring Trim(const std::wstring& text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && (text[begin] == L' ' || text[begin] == L'\t' || text[begin] == L'\r' || text[begin] == L'\n')) {
        ++begin;
    }
    while (end > begin && (text[end - 1] == L' ' || text[end - 1] == L'\t' || text[end - 1] == L'\r' || text[end - 1] == L'\n')) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::wstring GetModulePath() {
    wchar_t path[MAX_PATH * 2] = {};
    const DWORD length = ::GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    return std::wstring(path, length);
}

std::wstring GetModuleDir() {
    std::wstring path = GetModulePath();
    const size_t cut = path.find_last_of(L"\\/");
    return cut == std::wstring::npos ? path : path.substr(0, cut);
}

std::wstring GetTempDir() {
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = ::GetTempPathW(static_cast<DWORD>(std::size(buffer)), buffer);
    return std::wstring(buffer, length);
}

std::wstring Join(const std::wstring& left, const std::wstring& right) {
    if (left.empty()) {
        return right;
    }
    if (right.empty()) {
        return left;
    }
    const wchar_t last = left.back();
    if (last == L'\\' || last == L'/') {
        return left + right;
    }
    return left + L"\\" + right;
}

bool FileExists(const std::wstring& path) {
    const DWORD attr = ::GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool DeleteFileIfExists(const std::wstring& path) {
    if (!FileExists(path)) {
        return true;
    }
    if (::DeleteFileW(path.c_str())) {
        return true;
    }
    ::SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
    return ::DeleteFileW(path.c_str()) != FALSE;
}

bool WriteFileBytes(const std::wstring& path, const std::vector<uint8_t>& data) {
    HANDLE file = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const bool ok = !data.empty() &&
                    ::WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) &&
                    written == data.size();
    if (data.empty()) {
        // 空文件也视为成功
    }
    ::CloseHandle(file);
    return data.empty() ? true : ok;
}

std::wstring ModuleVersion(const std::wstring& path) {
    DWORD ignored = 0;
    const DWORD size = ::GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (size == 0) {
        return L"";
    }
    std::vector<uint8_t> block(size);
    if (!::GetFileVersionInfoW(path.c_str(), 0, size, block.data())) {
        return L"";
    }
    VS_FIXEDFILEINFO* info = nullptr;
    UINT length = 0;
    if (!::VerQueryValueW(block.data(), L"\\", reinterpret_cast<void**>(&info), &length) || info == nullptr) {
        return L"";
    }
    return Format(L"%u.%u.%u", HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
                  HIWORD(info->dwFileVersionLS));
}

namespace {

std::wstring FriendlyName(HDEVINFO set, SP_DEVINFO_DATA* info) {
    wchar_t buffer[256] = {};
    DWORD type = 0;
    if (::SetupDiGetDeviceRegistryPropertyW(set, info, SPDRP_FRIENDLYNAME, &type,
                                            reinterpret_cast<PBYTE>(buffer), sizeof(buffer), nullptr)) {
        return Trim(buffer);
    }
    return L"";
}

std::wstring ExtractComPort(const std::wstring& text) {
    const std::wstring upper = [&] {
        std::wstring copy = text;
        std::transform(copy.begin(), copy.end(), copy.begin(), ::towupper);
        return copy;
    }();
    const size_t at = upper.find(L"(COM");
    if (at == std::wstring::npos) {
        return L"";
    }
    const size_t close = upper.find(L')', at);
    if (close == std::wstring::npos) {
        return L"";
    }
    return text.substr(at + 1, close - at - 1);
}

}  // namespace

std::vector<SerialPortInfo> EnumSerialPorts() {
    std::vector<SerialPortInfo> ports;
    HDEVINFO set = ::SetupDiGetClassDevsW(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
    if (set != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA info{};
        info.cbSize = sizeof(info);
        for (DWORD index = 0; ::SetupDiEnumDeviceInfo(set, index, &info); ++index) {
            const std::wstring friendly = FriendlyName(set, &info);
            if (friendly.empty()) {
                continue;
            }
            std::wstring device = ExtractComPort(friendly);
            if (device.empty()) {
                continue;
            }
            SerialPortInfo entry;
            entry.device = device;
            entry.name = friendly;
            ports.push_back(entry);
        }
        ::SetupDiDestroyDeviceInfoList(set);
    }

    std::sort(ports.begin(), ports.end(), [](const SerialPortInfo& a, const SerialPortInfo& b) {
        const int na = _wtoi(a.device.c_str() + 3);
        const int nb = _wtoi(b.device.c_str() + 3);
        return na < nb;
    });
    return ports;
}

}  // namespace fc
