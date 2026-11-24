#include "i18n.h"

namespace fc {
namespace {

struct Entry {
    Str          id;
    const wchar_t* zh;
    const wchar_t* en;
};

const Entry kTable[] = {
#define FC_TABLE_ROW(name, zh, en) { Str::name, zh, en },
    FC_STRINGS(FC_TABLE_ROW)
#undef FC_TABLE_ROW
};

constexpr wchar_t kRegKey[] = L"Software\\FanControl\\Flasher";
constexpr wchar_t kRegValue[] = L"Language";

bool g_chinese = false;

}  // namespace

const wchar_t* T(Str id) {
    for (const Entry& entry : kTable) {
        if (entry.id == id) {
            return g_chinese ? entry.zh : entry.en;
        }
    }
    return L"";
}

void SetUiLanguage(bool chinese) {
    g_chinese = chinese;
}

bool IsChineseUi() {
    return g_chinese;
}

void InitLanguage() {
    wchar_t buffer[32] = {};
    DWORD size = sizeof(buffer);
    DWORD type = 0;
    const LSTATUS status = ::RegGetValueW(HKEY_CURRENT_USER, kRegKey, kRegValue, RRF_RT_REG_SZ, &type, buffer, &size);
    if (status == ERROR_SUCCESS && buffer[0] != 0) {
        g_chinese = (_wcsicmp(buffer, L"zh-CN") == 0);
        return;
    }
    const LANGID lang = ::GetUserDefaultUILanguage();
    g_chinese = (PRIMARYLANGID(lang) == LANG_CHINESE);
}

void SaveLanguage(bool chinese) {
    HKEY key = nullptr;
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS) {
        return;
    }
    const wchar_t* value = chinese ? L"zh-CN" : L"en-US";
    ::RegSetValueExW(key, kRegValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(value),
                     static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));
    ::RegCloseKey(key);
}

}  // namespace fc
