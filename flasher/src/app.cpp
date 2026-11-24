#include "app.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cwchar>
#include <cwctype>

#include "espclt.h"
#include "firmware.h"
#include "i18n.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shell32.lib")

namespace fc {
namespace {

constexpr int kIconResourceId = 101;
constexpr float kMargin = 24.0f;
constexpr float kHeaderH = 84.0f;
constexpr float kGap = 12.0f;

// 开源地址：右上角的「开源地址 / Source code」点开就在浏览器里打开这里。
constexpr wchar_t kSourceUrl[] = L"https://github.com/Anlerway/ESP32-LaptopFan";

std::wstring g_version = L"2.0.0";

constexpr float CardY1 = kHeaderH;                 // 设备
constexpr float CardH1 = 128.0f;
constexpr float CardY2 = CardY1 + CardH1 + kGap;   // 蓝牙名称
constexpr float CardH2 = 116.0f;
constexpr float CardY3 = CardY2 + CardH2 + kGap;   // 烧录
constexpr float CardH3 = 150.0f;
constexpr float FooterY = CardY3 + CardH3 + 16.0f;

std::wstring ClipboardText() {
    if (!::OpenClipboard(nullptr)) {
        return L"";
    }
    std::wstring text;
    HANDLE data = ::GetClipboardData(CF_UNICODETEXT);
    if (data != nullptr) {
        const auto* raw = static_cast<const wchar_t*>(::GlobalLock(data));
        if (raw != nullptr) {
            text.assign(raw);
            ::GlobalUnlock(data);
        }
    }
    ::CloseClipboard();
    return text;
}

void ApplyFrameEffects(HWND hwnd, bool dark) {
    const BOOL darkFlag = dark ? TRUE : FALSE;
    ::DwmSetWindowAttribute(hwnd, 20, &darkFlag, sizeof(darkFlag));
    const int round = 2;
    ::DwmSetWindowAttribute(hwnd, 33, &round, sizeof(round));
    COLORREF caption = dark ? RGB(0x20, 0x20, 0x20) : RGB(0xF3, 0xF3, 0xF3);
    COLORREF border = dark ? RGB(0x3A, 0x3A, 0x3A) : RGB(0xE9, 0xE9, 0xE9);
    COLORREF textColor = dark ? RGB(0xFF, 0xFF, 0xFF) : RGB(0x1B, 0x1B, 0x1B);
    ::DwmSetWindowAttribute(hwnd, 35, &caption, sizeof(caption));
    ::DwmSetWindowAttribute(hwnd, 34, &border, sizeof(border));
    ::DwmSetWindowAttribute(hwnd, 36, &textColor, sizeof(textColor));
}

}  // namespace

bool App::Create(HWND hwnd) {
    InitLanguage();
    hwnd_ = hwnd;
    const bool dark = !SystemUsesLightTheme();

    if (FAILED(::D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d_))) {
        return false;
    }
    if (FAILED(::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                     reinterpret_cast<IUnknown**>(&dw_)))) {
        return false;
    }
    gfx_.SetTheme(MakeTheme(dark));
    gfx_.SetUiScale(kUiScale);
    if (!gfx_.Init(hwnd, d2d_, dw_)) {
        return false;
    }
    HICON icon = static_cast<HICON>(::LoadImageW(::GetModuleHandleW(nullptr), MAKEINTRESOURCEW(kIconResourceId),
                                                 IMAGE_ICON, 128, 128, LR_DEFAULTCOLOR));
    if (icon != nullptr) {
        gfx_.LoadIcon(icon);
    }
    if (SUCCEEDED(dw_->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                        DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"zh-CN", &mono_))) {
        mono_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }

    ApplyFrameEffects(hwnd_, dark);
    const std::wstring version = ModuleVersion(GetModulePath());
    if (!version.empty()) {
        g_version = version;
    }

    RefreshPorts();
    caret_ = name_.size();
    ::SetTimer(hwnd_, 1, 530, nullptr);
    return true;
}

void App::Destroy() {
    cancel_ = true;
    if (worker_.joinable()) {
        worker_.join();
    }
    ::KillTimer(hwnd_, 1);
    if (mono_ != nullptr) {
        mono_->Release();
        mono_ = nullptr;
    }
    gfx_.Shutdown();
    if (dw_ != nullptr) {
        dw_->Release();
        dw_ = nullptr;
    }
    if (d2d_ != nullptr) {
        d2d_->Release();
        d2d_ = nullptr;
    }
}

void App::Invalidate() {
    if (hwnd_ != nullptr) {
        ::InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void App::RefreshPorts() {
    ports_ = EnumSerialPorts();
    // 顺便看有没有插着但缺驱动的下载芯片（CH340/CP210x/FTDI…）。
    chips_ = ProbeUsbSerialChips();
    if (portIndex_ >= static_cast<int>(ports_.size())) {
        portIndex_ = 0;
    }
    dropdownOpen_ = false;
    Invalidate();
}

void App::InstallDriver() {
    const ChipProbe* target = nullptr;
    for (const ChipProbe& probe : chips_) {
        if (!probe.driverInstalled) {
            target = &probe;
            break;
        }
    }
    if (target == nullptr) {
        SetStatus(T(Str::StatusNoChip), true, false);
        RefreshPorts();
        return;
    }
    if (!HasBundledDriver(target->chip)) {
        SetStatus(Format(T(Str::StatusNoBundled), ChipName(target->chip)), true, false);
        return;
    }

    const std::wstring exe = GetModulePath();
    const std::wstring parameters = Format(L"--install-driver %d", static_cast<int>(target->chip));
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = exe.c_str();
    info.lpParameters = parameters.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (!::ShellExecuteExW(&info)) {
        SetStatus(T(Str::StatusNeedAdmin), true, false);
        return;
    }
    SetStatus(T(Str::StatusInstalling), false, false);
    ::WaitForSingleObject(info.hProcess, 240000);
    ::CloseHandle(info.hProcess);
    RefreshPorts();
    SetStatus(T(Str::StatusInstalled), false, true);
}

void App::SetStatus(const std::wstring& text, bool bad, bool good) {
    status_ = text;
    statusBad_ = bad;
    statusGood_ = good;
}

void App::FocusName(bool on) {
    if (nameFocused_ == on) {
        return;
    }
    nameFocused_ = on;
    caretOn_ = true;
    if (on) {
        caret_ = std::min(caret_, name_.size());
    }
    Invalidate();
}

int App::CaretFromX(float x) {
    // 与绘制时的内边距保持一致（12）。
    const float target = x - 12.0f;
    float best = 0.0f;
    size_t index = 0;
    for (size_t i = 1; i <= name_.size(); ++i) {
        const float w = gfx_.Measure(name_.substr(0, i), gfx_.fonts().body14);
        if (w > target) {
            return static_cast<int>((w - target) < (target - best) ? i : index);
        }
        best = w;
        index = i;
    }
    (void)index;
    return static_cast<int>(name_.size());
}

void App::InsertName(const std::wstring& text) {
    if (text.empty() || busy_) {
        return;
    }
    size_t room = kMaxNameLength - std::min(name_.size(), kMaxNameLength);
    if (room == 0) {
        return;
    }
    const std::wstring clipped = text.substr(0, room);
    name_.insert(caret_, clipped);
    caret_ += clipped.size();
    Invalidate();
}

void App::PaintCard(float y, float h) {
    const Theme& th = gfx_.theme();
    const D2D1_RECT_F card = Rect(kMargin, y, width() - 2 * kMargin, h);
    gfx_.FillRounded(card, 10.0f, th.cardBg);
    gfx_.StrokeRounded(card, 10.0f, th.cardBorder, 1.0f);
}

void App::Label(const std::wstring& text, const D2D1_RECT_F& box, IDWriteTextFormat* format,
                const D2D1_COLOR_F& color, DWRITE_TEXT_ALIGNMENT align) {
    gfx_.Text(text, box, format, color, align);
}

void App::Button(const D2D1_RECT_F& r, const std::wstring& text, HitId id, bool primary, bool enabled) {
    const Theme& th = gfx_.theme();
    if (primary) {
        gfx_.FillRounded(r, 8.0f, enabled ? th.accent : th.controlBg);
    } else {
        gfx_.FillRounded(r, 8.0f, th.controlBg);
        gfx_.StrokeRounded(r, 8.0f, th.controlBorder, 1.0f);
    }
    const float h = r.bottom - r.top;
    gfx_.Text(text, D2D1::RectF(r.left, r.top + (h - 20.0f) / 2.0f, r.right, r.top + (h + 20.0f) / 2.0f),
              gfx_.fonts().body14, primary ? th.accentText : th.textPrimary, DWRITE_TEXT_ALIGNMENT_CENTER);
    if (enabled) {
        hotspots_.push_back({r, id, -1});
    }
}

void App::PaintHeader() {
    const Theme& th = gfx_.theme();
    if (gfx_.hasIcon()) {
        gfx_.DrawIcon(Rect(kMargin, 20.0f, 44.0f, 44.0f));
    }
    const float textX = kMargin + 58.0f;
    gfx_.Text(T(Str::AppTitle), D2D1::RectF(textX, 20.0f, width() - kMargin - 140.0f, 44.0f),
              gfx_.fonts().title20, th.textPrimary);
    gfx_.Text(Format(T(Str::AppSubtitle), g_version.c_str()),
              D2D1::RectF(textX, 46.0f, width() - kMargin - 140.0f, 64.0f), gfx_.fonts().small12, th.textTertiary);

    // 右上角语言切换：中文 / English
    const bool chinese = IsChineseUi();
    const float switchW = 124.0f;
    const float switchH = 26.0f;
    const D2D1_RECT_F langBox = D2D1::RectF(width() - kMargin - switchW, 24.0f, width() - kMargin, 24.0f + switchH);
    gfx_.FillRounded(langBox, 6.0f, th.controlBg);
    gfx_.StrokeRounded(langBox, 6.0f, th.controlBorder, 1.0f);
    const float halfW = switchW / 2.0f;
    const D2D1_RECT_F zhBox =
        D2D1::RectF(langBox.left + 2.0f, langBox.top + 2.0f, langBox.left + halfW, langBox.bottom - 2.0f);
    const D2D1_RECT_F enBox =
        D2D1::RectF(langBox.left + halfW, langBox.top + 2.0f, langBox.right - 2.0f, langBox.bottom - 2.0f);
    if (chinese) {
        gfx_.FillRounded(zhBox, 4.0f, th.accent);
    } else {
        gfx_.FillRounded(enBox, 4.0f, th.accent);
    }
    gfx_.Text(L"中文", zhBox, gfx_.fonts().small12, chinese ? th.accentText : th.textSecondary,
              DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    gfx_.Text(L"English", enBox, gfx_.fonts().small12, chinese ? th.textSecondary : th.accentText,
              DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    hotspots_.push_back({zhBox, HitId::LangZh, -1});
    hotspots_.push_back({enBox, HitId::LangEn, -1});

    // 语言开关下面是开源地址，鼠标移上去变成手型并可点击打开仓库主页。
    const D2D1_RECT_F linkBox = D2D1::RectF(width() - kMargin - 220.0f, 62.0f, width() - kMargin, 80.0f);
    gfx_.Text(std::wstring(T(Str::SourceLink)) + L" \u2197", linkBox, gfx_.fonts().small12,
              sourceHover_ ? th.accentHover : th.accent, DWRITE_TEXT_ALIGNMENT_TRAILING);
    hotspots_.push_back({linkBox, HitId::SourceLink, -1});
}

void App::PaintPorts(float y, float h) {
    const Theme& th = gfx_.theme();
    PaintCard(y, h);
    gfx_.Text(T(Str::SectionDevice), D2D1::RectF(kMargin + 18.0f, y + 14.0f, width() - kMargin - 18.0f, y + 36.0f),
              gfx_.fonts().h16, th.textPrimary);

    const float rowY = y + 46.0f;
    const float rowH = 38.0f;
    const float refreshW = 78.0f;
    const D2D1_RECT_F pill =
        D2D1::RectF(kMargin + 18.0f, rowY, width() - kMargin - 18.0f - refreshW - 10.0f, rowY + rowH);
    const bool hasPort = !ports_.empty() && portIndex_ < static_cast<int>(ports_.size());

    gfx_.FillRounded(pill, 8.0f, th.controlBg);
    gfx_.StrokeRounded(pill, 8.0f, dropdownOpen_ ? th.accent : th.controlBorder, dropdownOpen_ ? 1.6f : 1.0f);
    const std::wstring portText =
        hasPort ? (ports_[portIndex_].device + L"   " + ports_[portIndex_].name) : std::wstring(T(Str::NoPort));
    gfx_.PushClip(D2D1::RectF(pill.left + 12.0f, pill.top, pill.right - 30.0f, pill.bottom));
    gfx_.Text(portText, D2D1::RectF(pill.left + 12.0f, pill.top + 9.0f, pill.right, pill.top + 30.0f),
              gfx_.fonts().body14, hasPort ? th.textPrimary : th.textTertiary);
    gfx_.PopClip();
    // 右侧下拉箭头
    gfx_.DrawLine(pill.right - 22.0f, pill.top + 17.0f, pill.right - 17.0f, pill.top + 22.0f, th.textSecondary, 1.4f);
    gfx_.DrawLine(pill.right - 17.0f, pill.top + 22.0f, pill.right - 12.0f, pill.top + 17.0f, th.textSecondary, 1.4f);
    if (!busy_) {
        hotspots_.push_back({pill, HitId::PortPill, -1});
    }

    Button(D2D1::RectF(width() - kMargin - 18.0f - refreshW, rowY, width() - kMargin - 18.0f, rowY + rowH),
           T(Str::Refresh), HitId::Refresh, false, !busy_);

    Label(T(Str::PortHint),
          D2D1::RectF(kMargin + 18.0f, y + 92.0f, width() - kMargin - 18.0f, y + 110.0f), gfx_.fonts().small12,
          th.textTertiary);
}

void App::PaintName(float y, float h) {
    const Theme& th = gfx_.theme();
    PaintCard(y, h);
    gfx_.Text(T(Str::SectionName), D2D1::RectF(kMargin + 18.0f, y + 14.0f, width() - kMargin - 18.0f, y + 36.0f),
              gfx_.fonts().h16, th.textPrimary);

    const D2D1_RECT_F field =
        D2D1::RectF(kMargin + 18.0f, y + 44.0f, width() - kMargin - 18.0f, y + 84.0f);
    gfx_.FillRounded(field, 8.0f, th.controlBg);
    gfx_.StrokeRounded(field, 8.0f, nameFocused_ ? th.accent : th.controlBorder, nameFocused_ ? 1.6f : 1.0f);

    const float padX = 12.0f;
    const float avail = (field.right - field.left) - padX * 2.0f;
    const float caretX = gfx_.Measure(name_.substr(0, std::min(caret_, name_.size())), gfx_.fonts().body14);
    const float offset = caretX > avail - 6.0f ? caretX - (avail - 6.0f) : 0.0f;
    gfx_.PushClip(D2D1::RectF(field.left + padX, field.top + 2.0f, field.right - padX, field.bottom - 2.0f));
    gfx_.Text(name_, D2D1::RectF(field.left + padX - offset, field.top + 9.0f, field.left + padX - offset + 4000.0f,
                                 field.top + 30.0f),
              gfx_.fonts().body14, th.textPrimary);
    if (nameFocused_ && caretOn_) {
        gfx_.DrawLine(field.left + padX + caretX - offset, field.top + 11.0f, field.left + padX + caretX - offset,
                      field.top + 29.0f, th.accent, 1.4f);
    }
    gfx_.PopClip();
    hotspots_.push_back({field, HitId::NameField, -1});

    Label(T(Str::NameHint),
          D2D1::RectF(kMargin + 18.0f, y + 88.0f, width() - kMargin - 90.0f, y + 106.0f), gfx_.fonts().small12,
          th.textTertiary);
    Label(Format(L"%zu / %zu", name_.size(), kMaxNameLength),
          D2D1::RectF(width() - kMargin - 90.0f, y + 88.0f, width() - kMargin - 18.0f, y + 106.0f),
          gfx_.fonts().small12, th.textTertiary, DWRITE_TEXT_ALIGNMENT_TRAILING);
}

void App::PaintProgress(float y, float h) {
    const Theme& th = gfx_.theme();
    PaintCard(y, h);
    gfx_.Text(T(Str::SectionFlash), D2D1::RectF(kMargin + 18.0f, y + 14.0f, width() - kMargin - 18.0f, y + 36.0f),
              gfx_.fonts().h16, th.textPrimary);
    Label(Format(L"%d%%", percent_),
          D2D1::RectF(width() - kMargin - 100.0f, y + 14.0f, width() - kMargin - 18.0f, y + 36.0f),
          gfx_.fonts().body14, percent_ > 0 ? th.textPrimary : th.textTertiary, DWRITE_TEXT_ALIGNMENT_TRAILING);

    const std::wstring step = step_.empty() ? T(Str::WaitStart) : step_;
    Label(step, D2D1::RectF(kMargin + 18.0f, y + 38.0f, width() - kMargin - 18.0f, y + 56.0f), gfx_.fonts().small12,
          th.textSecondary);

    const D2D1_RECT_F track = D2D1::RectF(kMargin + 18.0f, y + 60.0f, width() - kMargin - 18.0f, y + 68.0f);
    gfx_.FillRounded(track, 4.0f, th.track);
    if (percent_ > 0) {
        const float full = track.right - track.left;
        const float part = full * std::min(std::max(percent_, 0), 100) / 100.0f;
        if (part > 0.0f) {
            gfx_.FillRounded(D2D1::RectF(track.left, track.top, track.left + part, track.bottom), 4.0f,
                             statusBad_ ? th.danger : th.accent);
        }
    }

    // 没插上串口但发现了缺驱动的下载芯片：直接把"安装驱动"摆出来。
    if (!busy_) {
        std::wstring missing;
        for (const ChipProbe& probe : chips_) {
            if (probe.driverInstalled) {
                continue;
            }
            if (!missing.empty()) {
                missing += L" / ";
            }
            missing += ChipName(probe.chip);
        }
        if (!missing.empty()) {
            Label(Format(T(Str::ChipMissing), missing.c_str()),
                  D2D1::RectF(kMargin + 18.0f, y + 76.0f, width() - kMargin - 170.0f, y + 94.0f), gfx_.fonts().small12,
                  th.danger);
            Label(T(Str::ChipMissingHint),
                  D2D1::RectF(kMargin + 18.0f, y + 96.0f, width() - kMargin - 170.0f, y + 114.0f), gfx_.fonts().small12,
                  th.textTertiary);
            Button(D2D1::RectF(width() - kMargin - 158.0f, y + 90.0f, width() - kMargin - 18.0f, y + 122.0f),
                   T(Str::InstallDriver), HitId::InstallDriver, false, true);
            return;
        }
    }

    // 最近 4 行 esptool 输出
    float lineY = y + 76.0f;
    const size_t count = log_.size();
    const size_t from = count > 4 ? count - 4 : 0;
    for (size_t i = from; i < count; ++i) {
        std::wstring text = log_[i];
        if (text.size() > 96) {
            text = text.substr(0, 96) + L"…";
        }
        gfx_.Text(text, D2D1::RectF(kMargin + 18.0f, lineY, width() - kMargin - 18.0f, lineY + 16.0f),
                  mono_ != nullptr ? mono_ : gfx_.fonts().small12, th.textTertiary);
        lineY += 15.0f;
    }
}

void App::PaintFooter(float y) {
    const Theme& th = gfx_.theme();
    if (!status_.empty()) {
        const D2D1_COLOR_F color = statusBad_ ? th.danger : (statusGood_ ? th.ok : th.textSecondary);
        gfx_.Text(status_, D2D1::RectF(kMargin, y + 10.0f, width() - kMargin - 150.0f, y + 30.0f),
                  gfx_.fonts().small12, color);
    }
    const D2D1_RECT_F button =
        D2D1::RectF(width() - kMargin - 132.0f, y, width() - kMargin, y + 38.0f);
    Button(button, busy_ ? T(Str::CancelFlash) : T(Str::StartFlash), HitId::Primary, true, true);
}

void App::PaintDropdown() {
    if (!dropdownOpen_ || ports_.empty()) {
        return;
    }
    const Theme& th = gfx_.theme();
    const float panelW = width() - 2 * kMargin - 36.0f - 88.0f;
    const float itemH = 36.0f;
    const float panelH = static_cast<float>(ports_.size()) * itemH + 8.0f;
    const float left = kMargin + 18.0f;
    const float top = CardY1 + 46.0f + 38.0f + 2.0f;
    const D2D1_RECT_F panel = D2D1::RectF(left, top, left + panelW, top + panelH);
    gfx_.FillRounded(panel, 8.0f, th.cardBg);
    gfx_.StrokeRounded(panel, 8.0f, th.controlBorder, 1.0f);

    float itemY = top + 4.0f;
    for (size_t i = 0; i < ports_.size(); ++i) {
        const D2D1_RECT_F item = D2D1::RectF(left + 4.0f, itemY, left + panelW - 4.0f, itemY + itemH);
        if (static_cast<int>(i) == portIndex_) {
            gfx_.FillRounded(item, 6.0f, th.controlBg);
        }
        gfx_.Text(ports_[i].device + L"   " + ports_[i].name,
                  D2D1::RectF(item.left + 10.0f, itemY + 8.0f, item.right, itemY + 28.0f), gfx_.fonts().body14,
                  th.textPrimary);
        hotspots_.push_back({item, HitId::PortItem, static_cast<int>(i)});
        itemY += itemH;
    }
}

void App::Paint() {
    if (!gfx_.Begin(gfx_.theme().windowBg)) {
        return;
    }
    hotspots_.clear();
    PaintHeader();
    PaintPorts(CardY1, CardH1);
    PaintName(CardY2, CardH2);
    PaintProgress(CardY3, CardH3);
    PaintFooter(FooterY);
    PaintDropdown();
    gfx_.End();
}

void App::ApplyWorkerState() {
    bool finished = false;
    std::wstring failure;
    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        step_ = workerStep_;
        percent_ = workerPercent_;
        log_ = workerLogs_;
        if (workerFinished_) {
            finished = true;
            workerFinished_ = false;
            busy_ = false;
            if (workerOk_) {
                percent_ = 100;
                step_ = T(Str::FlashDone);
                SetStatus(T(Str::FlashOk), false, true);
            } else {
                SetStatus(workerError_.empty() ? T(Str::FlashFail) : workerError_, true, false);
                failure = workerError_;
            }
        }
    }
    if (finished && worker_.joinable()) {
        worker_.join();
    }
    if (!failure.empty()) {
        // esptool 的报错有多行，状态栏放不下，单独弹一个框。
        ::MessageBoxW(hwnd_, failure.c_str(), T(Str::FlashFailTitle), MB_ICONERROR | MB_OK);
    }
    Invalidate();
}

void App::StartFlash() {
    if (busy_) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    std::wstring error;
    if (!IsValidDeviceName(name_, &error)) {
        SetStatus(error, true, false);
        FocusName(true);
        return;
    }
    if (ports_.empty() || portIndex_ >= static_cast<int>(ports_.size())) {
        SetStatus(T(Str::NoPortStatus), true, false);
        return;
    }

    cancel_ = false;
    busy_ = true;
    percent_ = 0;
    step_ = T(Str::Preparing);
    log_.clear();
    SetStatus(L"", false, false);
    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        workerStep_ = step_;
        workerPercent_ = 0;
        workerFinished_ = false;
        workerOk_ = false;
        workerError_.clear();
        workerLogs_.clear();
    }

    FlashRequest request;
    request.port = ports_[portIndex_].device;
    request.name = name_;
    worker_ = std::thread([this, request]() {
        const HWND hwnd = hwnd_;
        FlashEvents events;
        events.onStep = [this, hwnd](const std::wstring& text) {
            {
                std::lock_guard<std::mutex> lock(workerMutex_);
                workerStep_ = text;
            }
            ::PostMessageW(hwnd, kProgressMessage, 0, 0);
        };
        events.onPercent = [this, hwnd](int value) {
            {
                std::lock_guard<std::mutex> lock(workerMutex_);
                workerPercent_ = value;
            }
            ::PostMessageW(hwnd, kProgressMessage, 0, 0);
        };
        events.onLog = [this, hwnd](const std::wstring& line) {
            {
                std::lock_guard<std::mutex> lock(workerMutex_);
                workerLogs_.push_back(line);
                if (workerLogs_.size() > 200) {
                    workerLogs_.erase(workerLogs_.begin());
                }
            }
            ::PostMessageW(hwnd, kProgressMessage, 0, 0);
        };
        events.cancelled = [this]() { return cancel_.load(); };

        std::wstring flashError;
        const bool ok = FlashFirmware(request, events, &flashError);
        {
            std::lock_guard<std::mutex> lock(workerMutex_);
            workerOk_ = ok;
            workerFinished_ = true;
            workerError_ = flashError;
        }
        ::PostMessageW(hwnd, kProgressMessage, 0, 0);
    });
    Invalidate();
}

void App::CancelFlash() {
    if (!busy_) {
        return;
    }
    cancel_ = true;
    step_ = T(Str::Cancelling);
    SetStatus(T(Str::Cancelling), false, false);
    Invalidate();
}

LRESULT App::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            ::BeginPaint(hwnd_, &ps);
            Paint();
            ::EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            gfx_.Resize(LOWORD(lp), HIWORD(lp));
            Invalidate();
            return 0;
        case WM_TIMER:
            if (wp == 1) {
                caretOn_ = !caretOn_;
                if (nameFocused_) {
                    Invalidate();
                }
            }
            return 0;
        case kProgressMessage:
            ApplyWorkerState();
            return 0;
        case WM_LBUTTONDOWN: {
            const float sx = static_cast<float>(GET_X_LPARAM(lp)) / gfx_.scale();
            const float sy = static_cast<float>(GET_Y_LPARAM(lp)) / gfx_.scale();

            bool consumed = false;
            if (dropdownOpen_) {
                for (const Hotspot& spot : hotspots_) {
                    if (spot.id == HitId::PortItem && Contains(spot.rect, sx, sy)) {
                        portIndex_ = spot.index;
                        dropdownOpen_ = false;
                        consumed = true;
                        break;
                    }
                }
                if (!consumed) {
                    dropdownOpen_ = false;
                }
            }

            if (!consumed) {
                for (const Hotspot& spot : hotspots_) {
                    if (spot.id == HitId::PortItem || !Contains(spot.rect, sx, sy)) {
                        continue;
                    }
                    consumed = true;
                    switch (spot.id) {
                        case HitId::PortPill:
                            dropdownOpen_ = true;
                            Invalidate();
                            break;
                        case HitId::Refresh:
                            RefreshPorts();
                            break;
                        case HitId::InstallDriver:
                            InstallDriver();
                            break;
                        case HitId::NameField:
                            FocusName(true);
                            caret_ = static_cast<size_t>(CaretFromX(sx - spot.rect.left));
                            Invalidate();
                            break;
                        case HitId::LangZh:
                            if (!IsChineseUi()) {
                                SetUiLanguage(true);
                                SaveLanguage(true);
                                Invalidate();
                            }
                            break;
                        case HitId::LangEn:
                            if (IsChineseUi()) {
                                SetUiLanguage(false);
                                SaveLanguage(false);
                                Invalidate();
                            }
                            break;
                        case HitId::SourceLink:
                            ::ShellExecuteW(nullptr, L"open", kSourceUrl, nullptr, nullptr, SW_SHOWNORMAL);
                            break;
                        case HitId::Primary:
                            if (busy_) {
                                CancelFlash();
                            } else {
                                StartFlash();
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                }
            }
            if (!consumed && nameFocused_) {
                FocusName(false);
            }
            Invalidate();
            return 0;
        }
        case WM_CHAR: {
            const wchar_t ch = static_cast<wchar_t>(wp);
            if (nameFocused_ && !busy_ && ch >= 0x20 && ch < 0x7F) {
                InsertName(std::wstring(1, ch));
            }
            return 0;
        }
        case WM_KEYDOWN: {
            const bool ctrl = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (!nameFocused_ || busy_) {
                if (wp == VK_RETURN && !busy_) {
                    StartFlash();
                }
                return 0;
            }
            switch (wp) {
                case VK_LEFT:
                    if (caret_ > 0) {
                        --caret_;
                    }
                    break;
                case VK_RIGHT:
                    if (caret_ < name_.size()) {
                        ++caret_;
                    }
                    break;
                case VK_HOME:
                    caret_ = 0;
                    break;
                case VK_END:
                    caret_ = name_.size();
                    break;
                case VK_BACK:
                    if (caret_ > 0) {
                        name_.erase(caret_ - 1, 1);
                        --caret_;
                    }
                    break;
                case VK_DELETE:
                    if (caret_ < name_.size()) {
                        name_.erase(caret_, 1);
                    }
                    break;
                case 'V':
                    if (ctrl) {
                        std::wstring text;
                        for (const wchar_t ch : ClipboardText()) {
                            if (ch >= 0x20 && ch < 0x7F) {
                                text += ch;
                            }
                        }
                        InsertName(text);
                    }
                    break;
                default:
                    break;
            }
            caretOn_ = true;
            Invalidate();
            return 0;
        }
        case WM_SETCURSOR: {
            POINT pt{};
            ::GetCursorPos(&pt);
            ::ScreenToClient(hwnd_, &pt);
            const float sx = static_cast<float>(pt.x) / gfx_.scale();
            const float sy = static_cast<float>(pt.y) / gfx_.scale();
            bool overLink = false;
            for (const Hotspot& spot : hotspots_) {
                if (spot.id == HitId::NameField && Contains(spot.rect, sx, sy)) {
                    ::SetCursor(::LoadCursorW(nullptr, IDC_IBEAM));
                    return TRUE;
                }
                if (spot.id == HitId::SourceLink && Contains(spot.rect, sx, sy)) {
                    overLink = true;
                }
            }
            if (overLink != sourceHover_) {
                sourceHover_ = overLink;
                Invalidate();
            }
            if (overLink) {
                ::SetCursor(::LoadCursorW(nullptr, IDC_HAND));
                return TRUE;
            }
            break;
        }
        default:
            break;
    }
    return ::DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace fc
