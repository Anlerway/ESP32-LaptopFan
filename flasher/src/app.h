#pragma once
// 烧录器主界面：单页卡片布局，风格与主程序 FanControl 一致。
#include <windows.h>

#include <d2d1.h>
#include <dwrite.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "theme.h"
#include "ui.h"
#include "drivers.h"
#include "util.h"

namespace fc {

class App {
public:
    static constexpr float kUiScale = 1.0f;
    static constexpr float kDesignWidth = 700.0f;
    static constexpr float kDesignHeight = 578.0f;
    static constexpr int kProgressMessage = WM_APP + 1;

    bool Create(HWND hwnd);
    void Destroy();
    LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp);

private:
    enum class HitId {
        None, PortPill, Refresh, PortItem, NameField, InstallDriver, Primary, LangZh, LangEn, SourceLink
    };
    struct Hotspot {
        D2D1_RECT_F rect;
        HitId id = HitId::None;
        int index = -1;
    };

    void Paint();
    void PaintCard(float y, float h);
    void PaintHeader();
    void PaintPorts(float y, float h);
    void PaintName(float y, float h);
    void PaintProgress(float y, float h);
    void PaintFooter(float y);
    void PaintDropdown();

    void Label(const std::wstring& text, const D2D1_RECT_F& box, IDWriteTextFormat* format,
               const D2D1_COLOR_F& color,
               DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING);
    void Button(const D2D1_RECT_F& r, const std::wstring& text, HitId id, bool primary, bool enabled);

    void RefreshPorts();
    void InstallDriver();
    void StartFlash();
    void CancelFlash();
    void ApplyWorkerState();
    void SetStatus(const std::wstring& text, bool bad, bool good);
    void FocusName(bool on);
    void InsertName(const std::wstring& text);
    int  CaretFromX(float x);
    void Invalidate();

    float width() const { return gfx_.width(); }

    HWND hwnd_ = nullptr;
    Gfx gfx_;
    ID2D1Factory* d2d_ = nullptr;
    IDWriteFactory* dw_ = nullptr;
    IDWriteTextFormat* mono_ = nullptr;
    std::vector<Hotspot> hotspots_;

    std::vector<SerialPortInfo> ports_;
    std::vector<ChipProbe> chips_;
    int portIndex_ = 0;
    bool dropdownOpen_ = false;
    bool sourceHover_ = false;

    std::wstring name_ = L"LAPTOP-FAN";
    size_t caret_ = 0;
    bool nameFocused_ = false;
    bool caretOn_ = true;

    bool busy_ = false;
    int percent_ = 0;
    std::wstring step_;
    std::wstring status_;
    bool statusBad_ = false;
    bool statusGood_ = false;
    std::vector<std::wstring> log_;

    std::thread worker_;
    std::atomic<bool> cancel_{false};
    std::mutex workerMutex_;
    std::wstring workerStep_;
    int workerPercent_ = 0;
    bool workerFinished_ = false;
    bool workerOk_ = false;
    std::wstring workerError_;
    std::vector<std::wstring> workerLogs_;
};

}  // namespace fc
