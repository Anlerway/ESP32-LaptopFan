#include "firmware.h"

#include <windows.h>

#include <bcrypt.h>

#include <algorithm>
#include <cstring>

#include "i18n.h"
#include "util.h"

#pragma comment(lib, "bcrypt.lib")

namespace fc {
namespace {

// 固件放在 <exe 目录>\firmware\ 下，跟主程序分开，换固件时只要替换 bin 文件。
bool LoadFileImage(const wchar_t* fileName, const wchar_t* label, uint32_t offset, FlashImage& image, std::wstring* error) {
    const std::wstring path = Join(Join(GetModuleDir(), L"firmware"), fileName);
    HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        if (error != nullptr) {
            *error = Format(T(Str::FwNotFound), path.c_str());
        }
        return false;
    }
    LARGE_INTEGER size{};
    if (!::GetFileSizeEx(file, &size) || size.QuadPart <= 0) {
        ::CloseHandle(file);
        if (error != nullptr) {
            *error = Format(T(Str::FwEmpty), path.c_str());
        }
        return false;
    }
    image.data.resize(static_cast<size_t>(size.QuadPart));
    DWORD got = 0;
    const bool ok = ::ReadFile(file, image.data.data(), static_cast<DWORD>(image.data.size()), &got, nullptr) != 0 &&
                    got == image.data.size();
    ::CloseHandle(file);
    if (!ok) {
        image.data.clear();
        if (error != nullptr) {
            *error = Format(T(Str::FwReadFail), path.c_str());
        }
        return false;
    }
    image.label = label;
    image.offset = offset;
    return true;
}

// 把 value 覆盖进 [pos, pos+slotSize) 这一段，剩余字节补 0（字符串以 0 结尾，变短也安全）。
bool OverwriteSlot(std::vector<uint8_t>& app, size_t pos, size_t slotSize, const std::string& value) {
    if (value.size() > slotSize) {
        return false;
    }
    if (pos + slotSize > app.size()) {
        return false;
    }
    std::memset(app.data() + pos, 0, slotSize);
    std::memcpy(app.data() + pos, value.data(), value.size());
    return true;
}

// 镜像末尾的 SHA-256（安全启动/完整性校验用），走系统 CNG，省得自带实现。
bool Sha256(const uint8_t* data, size_t length, uint8_t* out) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        return false;
    }
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) == 0;
    if (ok) {
        ok = BCryptHashData(hash, const_cast<PUCHAR>(data), static_cast<ULONG>(length), 0) == 0;
    }
    if (ok) {
        ok = BCryptFinishHash(hash, out, 32, 0) == 0;
    }
    if (hash != nullptr) {
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return ok;
}

uint32_t ReadU32(const std::vector<uint8_t>& image, size_t offset) {
    return static_cast<uint32_t>(image[offset]) | (static_cast<uint32_t>(image[offset + 1]) << 8) |
           (static_cast<uint32_t>(image[offset + 2]) << 16) | (static_cast<uint32_t>(image[offset + 3]) << 24);
}

}  // namespace

bool FixImageIntegrity(std::vector<uint8_t>& image, std::wstring* error) {
    if (image.size() < 64 || image[0] != 0xE9) {
        if (error != nullptr) {
            *error = T(Str::ImgNotApp);
        }
        return false;
    }
    const size_t segmentCount = image[1];
    const bool hashAppended = (image[23] & 0x01) != 0;

    size_t offset = 24;
    uint8_t checksum = 0xEF;
    for (size_t i = 0; i < segmentCount; ++i) {
        if (offset + 8 > image.size()) {
            if (error != nullptr) {
                *error = T(Str::ImgSegTableOut);
            }
            return false;
        }
        const uint32_t length = ReadU32(image, offset + 4);
        if (offset + 8 + length > image.size()) {
            if (error != nullptr) {
                *error = T(Str::ImgSegDataOut);
            }
            return false;
        }
        for (uint32_t k = 0; k < length; ++k) {
            checksum ^= image[offset + 8 + k];
        }
        offset += 8 + length;
    }

    // 段数据之后先补齐到 16 字节边界，最后一个字节是校验字节；再往后是可选 SHA-256。
    const size_t aligned = ((offset + 16) / 16) * 16;
    const size_t expected = aligned + (hashAppended ? 32 : 0);
    if (expected != image.size()) {
        if (error != nullptr) {
            *error = Format(T(Str::ImgSizeMismatch), expected, image.size());
        }
        return false;
    }

    image[aligned - 1] = checksum;
    if (hashAppended &&
        !Sha256(image.data(), image.size() - 32, image.data() + image.size() - 32)) {
        if (error != nullptr) {
            *error = T(Str::ImgShaFail);
        }
        return false;
    }
    return true;
}

bool LoadFirmware(FirmwareBundle& bundle, std::wstring* error) {
    if (!LoadFileImage(L"bootloader.bin", L"bootloader", 0x1000, bundle.bootloader, error)) {
        return false;
    }
    if (!LoadFileImage(L"partitions.bin", L"partitions", 0x8000, bundle.partitions, error)) {
        return false;
    }
    if (!LoadFileImage(L"app.bin", L"app", 0x10000, bundle.app, error)) {
        return false;
    }
    return true;
}

bool IsValidDeviceName(const std::wstring& name, std::wstring* error) {
    if (name.empty()) {
        if (error != nullptr) {
            *error = T(Str::NameEmpty);
        }
        return false;
    }
    if (name.size() > kMaxNameLength) {
        if (error != nullptr) {
            *error = Format(T(Str::NameTooLong), kMaxNameLength);
        }
        return false;
    }
    for (const wchar_t ch : name) {
        if (ch < 0x20 || ch > 0x7E) {
            if (error != nullptr) {
                *error = T(Str::NameBadChar);
            }
            return false;
        }
        if (ch == L' ' || ch == L'"' || ch == L'\\' || ch == L'/') {
            if (error != nullptr) {
                *error = T(Str::NameBadSpace);
            }
            return false;
        }
    }
    return true;
}

bool PatchDeviceName(std::vector<uint8_t>& app, const std::wstring& name, std::wstring* error) {
    if (!IsValidDeviceName(name, error)) {
        return false;
    }
    const std::string ascii = WideToUtf8(name);
    const std::string placeholder = WideToUtf8(kNamePlaceholder);
    const std::string placeholderCom = placeholder + "-COM";

    const auto find = [&app](const std::string& needle) -> size_t {
        const auto it = std::search(app.begin(), app.end(), needle.begin(), needle.end());
        return it == app.end() ? std::string::npos : static_cast<size_t>(it - app.begin());
    };

    const size_t blePos = find(placeholder);
    if (blePos == std::string::npos) {
        if (error != nullptr) {
            *error = T(Str::NameSlotMissing);
        }
        return false;
    }
    // -COM 槽位就紧跟在 BLE 槽位后面（同一个字符串常量池），单独再搜一次更稳妥。
    const size_t comPos = find(placeholderCom);

    if (!OverwriteSlot(app, blePos, placeholder.size(), ascii)) {
        if (error != nullptr) {
            *error = T(Str::NameWriteFail);
        }
        return false;
    }
    if (comPos != std::string::npos) {
        if (!OverwriteSlot(app, comPos, placeholderCom.size(), ascii + "-COM")) {
            if (error != nullptr) {
                *error = T(Str::NameComWriteFail);
            }
            return false;
        }
    }
    // 改过字节就要重算校验，否则 ESP32 启动时会判镜像损坏。
    return FixImageIntegrity(app, error);
}

}  // namespace fc
