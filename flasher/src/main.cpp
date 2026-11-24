// 烧录器入口：建窗口、装 D2D，然后交给 App。
#include <windows.h>

#include <objbase.h>
#include <shellapi.h>

#include <string>

#include "app.h"
#include "drivers.h"
#include "i18n.h"
#include "util.h"

namespace {

constexpr int kIconResourceId = 101;
constexpr wchar_t kWindowClass[] = L"FanControlFlasherWindow";

fc::App g_app;
bool    g_appReady = false;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROY) {
        ::PostQuitMessage(0);
    }
    if (g_appReady) {
        return g_app.HandleMessage(msg, wp, lp);
    }
    return ::DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hinst, HINSTANCE, PWSTR, int) {
    using namespace fc;

    InitLanguage();
    // 提权后的同进程副本负责装驱动（pnputil 需要管理员）。
    int argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (argv != nullptr && argc >= 3 && _wcsicmp(argv[1], L"--install-driver") == 0) {
        const int index = _wtoi(argv[2]);
        ::LocalFree(argv);
        std::wstring error;
        const bool ok = fc::InstallBundledDriver(static_cast<fc::SerialChip>(index), &error);
        ::MessageBoxW(nullptr, ok ? T(Str::DrvInstalled) : error.c_str(), T(Str::AppTitle),
                      (ok ? MB_ICONINFORMATION : MB_ICONERROR) | MB_OK);
        return ok ? 0 : 1;
    }
    if (argv != nullptr) {
        ::LocalFree(argv);
    }

    if (FAILED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
        return 1;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hinst;
    wc.hIcon = static_cast<HICON>(::LoadImageW(hinst, MAKEINTRESOURCEW(kIconResourceId), IMAGE_ICON,
                                               0, 0, LR_DEFAULTSIZE | LR_SHARED));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    if (!::RegisterClassExW(&wc)) {
        ::CoUninitialize();
        return 1;
    }

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    const float scale = static_cast<float>(::GetDpiForSystem()) / 96.0f * fc::App::kUiScale;
    RECT frame{0, 0, static_cast<LONG>(fc::App::kDesignWidth * scale + 0.5f),
               static_cast<LONG>(fc::App::kDesignHeight * scale + 0.5f)};
    ::AdjustWindowRectEx(&frame, style, FALSE, 0);
    const int width = frame.right - frame.left;
    const int height = frame.bottom - frame.top;
    RECT work{};
    ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int x = work.left + ((work.right - work.left) - width) / 2;
    const int y = work.top + ((work.bottom - work.top) - height) / 2;

    HWND hwnd = ::CreateWindowExW(0, kWindowClass, T(Str::AppTitle), style, x, y, width, height, nullptr, nullptr,
                                  hinst, nullptr);
    if (hwnd == nullptr) {
        ::CoUninitialize();
        return 1;
    }
    if (!g_app.Create(hwnd)) {
        ::MessageBoxW(nullptr, T(Str::ErrUiInit), T(Str::AppTitle), MB_ICONERROR | MB_OK);
        ::DestroyWindow(hwnd);
        ::CoUninitialize();
        return 1;
    }
    g_appReady = true;
    ::ShowWindow(hwnd, SW_SHOW);
    ::UpdateWindow(hwnd);

    MSG msg{};
    while (::GetMessageW(&msg, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }

    g_appReady = false;
    g_app.Destroy();
    ::CoUninitialize();
    return 0;
}
