#include "espclt.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <vector>

#include "firmware.h"
#include "i18n.h"
#include "util.h"

namespace fc {
namespace {

std::wstring QuoteArg(const std::wstring& value) {
    if (value.find_first_of(L" \t\"") == std::wstring::npos) {
        return value;
    }
    std::wstring out = L"\"";
    for (const wchar_t ch : value) {
        if (ch == L'"') {
            out += L"\\\"";
        } else {
            out += ch;
        }
    }
    out += L"\"";
    return out;
}

// 从一行 esptool 输出里抠出 "(NN %)" 的百分比。
bool ParsePercent(const std::wstring& line, int* percent) {
    const size_t paren = line.rfind(L"(");
    if (paren == std::wstring::npos) {
        return false;
    }
    const size_t mark = line.find(L'%', paren);
    if (mark == std::wstring::npos) {
        return false;
    }
    const std::wstring digits = Trim(line.substr(paren + 1, mark - paren - 1));
    if (digits.empty()) {
        return false;
    }
    *percent = _wtoi(digits.c_str());
    return *percent >= 0 && *percent <= 100;
}

struct ProcessPipe {
    HANDLE read = nullptr;
    HANDLE write = nullptr;
    PROCESS_INFORMATION info{};

    ~ProcessPipe() {
        if (read != nullptr) {
            ::CloseHandle(read);
        }
        if (write != nullptr) {
            ::CloseHandle(write);
        }
        if (info.hProcess != nullptr) {
            ::CloseHandle(info.hProcess);
        }
        if (info.hThread != nullptr) {
            ::CloseHandle(info.hThread);
        }
    }
};

bool StartHidden(const std::wstring& commandLine, const std::wstring& workDir, ProcessPipe& pipe,
                 std::wstring* error) {
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    if (!::CreatePipe(&pipe.read, &pipe.write, &security, 0)) {
        if (error != nullptr) {
            *error = T(Str::ErrPipe);
        }
        return false;
    }
    ::SetHandleInformation(pipe.read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdOutput = pipe.write;
    startup.hStdError = pipe.write;
    startup.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);

    std::wstring mutableCommand = commandLine;
    const BOOL started = ::CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE,
                                          CREATE_NO_WINDOW, nullptr,
                                          workDir.empty() ? nullptr : workDir.c_str(), &startup, &pipe.info);
    if (!started) {
        if (error != nullptr) {
            *error = Format(T(Str::ErrSpawn), ::GetLastError());
        }
        return false;
    }
    ::CloseHandle(pipe.write);
    pipe.write = nullptr;
    return true;
}

bool EnsureDirTree(const std::wstring& dir) {
    if (dir.empty()) {
        return false;
    }
    std::wstring current;
    for (const wchar_t ch : dir) {
        current += ch;
        if (ch == L'\\' && current.size() > 3) {
            ::CreateDirectoryW(current.c_str(), nullptr);
        }
    }
    ::CreateDirectoryW(dir.c_str(), nullptr);
    const DWORD attr = ::GetFileAttributesW(dir.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// 候选临时目录：系统 TEMP → 用户 LOCALAPPDATA\Temp → 程序目录（TEMP 被策略/权限挡住时兜底）。
std::vector<std::wstring> WorkDirCandidates() {
    const std::wstring tag = Format(L"FanControl-Flasher-%lu", ::GetCurrentProcessId());
    std::vector<std::wstring> list;
    wchar_t temp[MAX_PATH] = {};
    const DWORD tempLength = ::GetTempPathW(MAX_PATH, temp);
    if (tempLength > 0 && tempLength < MAX_PATH) {
        list.push_back(Join(std::wstring(temp, tempLength), tag));
    }
    wchar_t local[MAX_PATH] = {};
    if (::GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH) > 0) {
        list.push_back(Join(Join(local, L"Temp"), tag));
    }
    list.push_back(Join(GetModuleDir(), tag));
    return list;
}

// 真正建目录并写一个探针文件，确认这个目录可写。
std::wstring PickWorkDir(std::wstring* tried) {
    const std::vector<uint8_t> probeData = {0x46, 0x43};
    for (const std::wstring& candidate : WorkDirCandidates()) {
        if (EnsureDirTree(candidate)) {
            const std::wstring probe = Join(candidate, L"probe.tmp");
            if (WriteFileBytes(probe, probeData)) {
                DeleteFileIfExists(probe);
                return candidate;
            }
        }
        if (tried != nullptr) {
            *tried += (tried->empty() ? L"" : L"；") + candidate;
        }
    }
    return std::wstring();
}

}  // namespace

std::wstring FindEsptool(std::wstring* error) {
    // 优先 exe 旁边的 tools\esptool.exe，其次同目录（兼容旧布局）。
    const std::wstring nearby[] = {Join(Join(GetModuleDir(), L"tools"), L"esptool.exe"),
                                   Join(GetModuleDir(), L"esptool.exe")};
    for (const std::wstring& candidate : nearby) {
        if (FileExists(candidate)) {
            return candidate;
        }
    }

    // 开发机：Arduino15 里板包自带的 esptool
    wchar_t localAppData[MAX_PATH] = {};
    const DWORD length = ::GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, static_cast<DWORD>(std::size(localAppData)));
    if (length > 0) {
        const std::wstring root = Join(std::wstring(localAppData, length), L"Arduino15\\packages\\esp32\\tools\\esptool_py");
        WIN32_FIND_DATAW found{};
        HANDLE search = ::FindFirstFileW(Join(root, L"*").c_str(), &found);
        if (search != INVALID_HANDLE_VALUE) {
            do {
                if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                    continue;
                }
                continue;
            } while (::FindNextFileW(search, &found));
            ::FindClose(search);
        }
        // 版本目录 → esptool.exe
        search = ::FindFirstFileW(Join(root, L"*").c_str(), &found);
        if (search != INVALID_HANDLE_VALUE) {
            do {
                if (found.cFileName[0] == L'.') {
                    continue;
                }
                const std::wstring candidate = Join(Join(root, found.cFileName), L"esptool.exe");
                if (FileExists(candidate)) {
                    ::FindClose(search);
                    return candidate;
                }
            } while (::FindNextFileW(search, &found));
            ::FindClose(search);
        }
    }

    if (error != nullptr) {
        *error = T(Str::ErrNoEsptool);
    }
    return L"";
}

bool FlashFirmware(const FlashRequest& request, const FlashEvents& events, std::wstring* error) {
    const auto log = [&events](const std::wstring& line) {
        if (events.onLog) {
            events.onLog(line);
        }
    };
    const auto step = [&events](const std::wstring& text) {
        if (events.onStep) {
            events.onStep(text);
        }
    };

    step(T(Str::StepReadFw));
    FirmwareBundle bundle;
    if (!LoadFirmware(bundle, error)) {
        return false;
    }
    if (!PatchDeviceName(bundle.app.data, request.name, error)) {
        return false;
    }
    log(Format(T(Str::LogAppSize), bundle.app.data.size() / 1024, request.name.c_str()));

    std::wstring esptool = request.esptoolPath.empty() ? FindEsptool(error) : request.esptoolPath;
    if (esptool.empty()) {
        return false;
    }

    std::wstring triedDirs;
    const std::wstring workDir = PickWorkDir(&triedDirs);
    if (workDir.empty()) {
        if (error != nullptr) {
            *error = Format(T(Str::ErrNoTemp), triedDirs.c_str());
        }
        return false;
    }
    log(Format(T(Str::LogTempDir), workDir.c_str()));
    const std::wstring bootPath = Join(workDir, L"bootloader.bin");
    const std::wstring partPath = Join(workDir, L"partitions.bin");
    const std::wstring appPath = Join(workDir, L"app.bin");
    const auto cleanup = [&] {
        DeleteFileIfExists(bootPath);
        DeleteFileIfExists(partPath);
        DeleteFileIfExists(appPath);
        ::RemoveDirectoryW(workDir.c_str());
    };
    bool written = false;
    for (int attempt = 0; attempt < 3 && !written; ++attempt) {
        if (attempt > 0) {
            ::Sleep(150);  // 杀毒软件刚扫过的新文件偶尔会短暂占用
        }
        written = WriteFileBytes(bootPath, bundle.bootloader.data) &&
                  WriteFileBytes(partPath, bundle.partitions.data) && WriteFileBytes(appPath, bundle.app.data);
    }
    if (!written) {
        const DWORD lastError = ::GetLastError();
        if (error != nullptr) {
            *error = Format(T(Str::ErrWriteFw), workDir.c_str(),
                            lastError);
        }
        cleanup();
        return false;
    }

    const std::wstring command =
        QuoteArg(esptool) +
        Format(L" --chip esp32 --port %s --baud %d --before default_reset --after hard_reset", request.port.c_str(),
               request.baud) +
        L" write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect" +
        Format(L" 0x1000 %s 0x8000 %s 0x10000 %s", QuoteArg(bootPath).c_str(), QuoteArg(partPath).c_str(),
               QuoteArg(appPath).c_str());
    log(command);

    ProcessPipe pipe;
    if (!StartHidden(command, workDir, pipe, error)) {
        cleanup();
        return false;
    }

    step(T(Str::StepConnect));
    std::wstring pending;
    std::vector<std::wstring> tail;
    int lastPercent = -1;
    bool cancelled = false;
    bool finished = false;

    while (!finished) {
        if (events.cancelled && events.cancelled()) {
            cancelled = true;
            ::TerminateProcess(pipe.info.hProcess, 1);
            break;
        }
        DWORD available = 0;
        if (::PeekNamedPipe(pipe.read, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
            char buffer[4096];
            DWORD got = 0;
            const DWORD want = available < sizeof(buffer) ? available : static_cast<DWORD>(sizeof(buffer));
            if (::ReadFile(pipe.read, buffer, want, &got, nullptr) && got > 0) {
                pending.append(Utf8ToWide(std::string(buffer, got)));
            }
        } else {
            if (::WaitForSingleObject(pipe.info.hProcess, 60) == WAIT_OBJECT_0) {
                finished = true;
            }
            continue;
        }

        size_t cut = 0;
        while (true) {
            const size_t at = pending.find_first_of(L"\r\n", cut);
            if (at == std::wstring::npos) {
                break;
            }
            const std::wstring line = Trim(pending.substr(0, at));
            pending.erase(0, at + 1);
            cut = 0;
            if (line.empty()) {
                continue;
            }
            log(line);
            tail.push_back(line);
            if (tail.size() > 40) {
                tail.erase(tail.begin());
            }
            int percent = 0;
            if (ParsePercent(line, &percent) && percent != lastPercent) {
                lastPercent = percent;
                if (events.onPercent) {
                    events.onPercent(percent);
                }
            }
        }
    }

    DWORD exitCode = 0;
    ::GetExitCodeProcess(pipe.info.hProcess, &exitCode);

    if (cancelled) {
        if (error != nullptr) {
            *error = T(Str::ErrCancelled);
        }
        cleanup();
        return false;
    }
    if (exitCode != 0) {
        std::wstring detail;
        for (size_t i = tail.size() > 6 ? tail.size() - 6 : 0; i < tail.size(); ++i) {
            detail += (detail.empty() ? L"" : L"\n") + tail[i];
        }
        if (error != nullptr) {
            *error = detail.empty() ? Format(T(Str::ErrExitCode), exitCode) : detail;
        }
        cleanup();
        return false;
    }

    cleanup();
    if (events.onPercent) {
        events.onPercent(100);
    }
    return true;
}

}  // namespace fc
