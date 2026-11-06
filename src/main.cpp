#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include "memory_manager.h"
#include "mascot.h"
#include "ui_renderer.h"
#include "resource.h"

#include <dwmapi.h>
#include <windowsx.h>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <string>
#include <thread>

#ifdef _MSC_VER
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "dwmapi.lib")
#endif

namespace {

constexpr UINT WM_APP_DEFLATE = WM_APP + 1;
constexpr uint64_t kTestChunk = 64ULL * 1024ULL * 1024ULL;

struct App {
    MemoryManager memory;
    UIRenderer ui;
    MascotRenderer mascot;
    ID2D1Factory* d2d = nullptr;
    IDWriteFactory* dwrite = nullptr;
    bool tracking = false;
};

MemoryManager* g_memory = nullptr;
HWND g_hwnd = nullptr;
std::atomic<bool> g_hotkeyStop{false};

void HotkeyThread() {
    bool latch = false;
    while (!g_hotkeyStop.load()) {
        const bool combo = (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                           (GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
                           (GetAsyncKeyState(VK_F12) & 0x8000);
        if (combo && !latch) {
            latch = true;
            if (g_memory) {
                g_memory->RequestEmergencyReclaim();
            }
            if (g_hwnd) {
                PostMessageW(g_hwnd, WM_APP_DEFLATE, 0, 0);
            }
        } else if (!combo) {
            latch = false;
        }
        Sleep(40);
    }
}

void EnableDarkTitle(HWND hwnd) {
    const BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark)))) {
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
        case WM_SIZE:
            if (app && wParam != SIZE_MINIMIZED) {
                app->ui.Resize(LOWORD(lParam), HIWORD(lParam));
            }
            return 0;
        case WM_DPICHANGED: {
            auto* suggested = reinterpret_cast<RECT*>(lParam);
            HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO monitorInfo{};
            monitorInfo.cbSize = sizeof(monitorInfo);
            RECT bounds = *suggested;
            if (GetMonitorInfoW(monitor, &monitorInfo)) {
                const int maxW = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
                const int maxH = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
                int w = bounds.right - bounds.left;
                int h = bounds.bottom - bounds.top;
                if (w > maxW) w = maxW;
                if (h > maxH) h = maxH;
                int x = bounds.left;
                int y = bounds.top;
                if (x + w > monitorInfo.rcWork.right) x = monitorInfo.rcWork.right - w;
                if (y + h > monitorInfo.rcWork.bottom) y = monitorInfo.rcWork.bottom - h;
                if (x < monitorInfo.rcWork.left) x = monitorInfo.rcWork.left;
                if (y < monitorInfo.rcWork.top) y = monitorInfo.rcWork.top;
                bounds = RECT{x, y, x + w, y + h};
            }
            SetWindowPos(hwnd, nullptr, bounds.left, bounds.top,
                         bounds.right - bounds.left, bounds.bottom - bounds.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            if (app) {
                RECT client{};
                GetClientRect(hwnd, &client);
                app->ui.Resize(static_cast<UINT>(client.right - client.left),
                               static_cast<UINT>(client.bottom - client.top));
            }
            return 0;
        }
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            UINT dpi = GetDpiForWindow(hwnd);
            if (dpi == 0) {
                dpi = 96;
            }
            info->ptMinTrackSize.x = MulDiv(440, static_cast<int>(dpi), 96);
            info->ptMinTrackSize.y = MulDiv(680, static_cast<int>(dpi), 96);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (app) {
                app->ui.OnMouseMove(static_cast<float>(GET_X_LPARAM(lParam)),
                                    static_cast<float>(GET_Y_LPARAM(lParam)));
                if (!app->tracking) {
                    TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, hwnd, 0};
                    TrackMouseEvent(&track);
                    app->tracking = true;
                }
            }
            return 0;
        case WM_MOUSELEAVE:
            if (app) {
                app->tracking = false;
                app->ui.OnMouseMove(-1.0f, -1.0f);
            }
            return 0;
        case WM_LBUTTONDOWN:
            if (app) {
                SetCapture(hwnd);
                app->ui.OnMouseDown(static_cast<float>(GET_X_LPARAM(lParam)),
                                    static_cast<float>(GET_Y_LPARAM(lParam)));
            }
            return 0;
        case WM_LBUTTONUP:
            if (app) {
                ReleaseCapture();
                app->ui.OnMouseUp(static_cast<float>(GET_X_LPARAM(lParam)),
                                  static_cast<float>(GET_Y_LPARAM(lParam)),
                                  app->memory, app->mascot, hwnd);
            }
            return 0;
        case WM_KEYDOWN:
            if (app && (wParam == VK_ESCAPE || (wParam == VK_F12 && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000)))) {
                app->memory.RequestEmergencyReclaim();
                app->mascot.TriggerDeflation();
            }
            return 0;
        case WM_APP_DEFLATE:
            if (app) {
                app->mascot.TriggerDeflation();
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int RunSelfTest() {
    std::ofstream out("self-test-result.txt", std::ios::trunc);
    auto finish = [&](int code, const std::string& text) {
        out << text << "\n";
        out.close();
        return code;
    };

    MemoryManager memory;
    memory.SetSpeed(AllocationSpeed::Fast);
    memory.SetSafetyMode(SafetyMode::Safe4GB);

    if (!memory.StartSacrifice()) {
        return finish(1, "FAIL start returned false");
    }
    if (memory.StartSacrifice()) {
        memory.ReclaimAll();
        return finish(1, "FAIL second start was allowed");
    }

    MemoryStats stats{};
    bool grew = false;
    for (int i = 0; i < 300; ++i) {
        stats = memory.GetStats();
        if (stats.appAllocatedBytes >= kTestChunk) {
            grew = true;
            break;
        }
        if (!stats.isSacrificing && stats.appAllocatedBytes == 0) {
            break;
        }
        Sleep(10);
    }

    if (!grew) {
        const std::string reason = stats.hadError ? stats.errorMessage : stats.statusMessage;
        memory.ReclaimAll();
        if (stats.hadError) {
            return finish(1, "FAIL allocation error: " + reason);
        }
        return finish(2, "SKIP machine is already inside the safety reserve (" + reason + ")");
    }

    PROCESS_MEMORY_COUNTERS_EX counters{};
    const bool infoOk = K32GetProcessMemoryInfo(
        GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters));
    if (!infoOk || counters.WorkingSetSize < 16ULL * 1024ULL * 1024ULL) {
        memory.ReclaimAll();
        return finish(1, "FAIL working set did not rise after touching pages");
    }

    memory.StopSacrifice();
    uint64_t held = 0;
    for (int i = 0; i < 40; ++i) {
        held = memory.GetStats().appAllocatedBytes;
        if (!memory.IsSacrificing() && held >= kTestChunk) {
            break;
        }
        Sleep(25);
    }
    if (held < kTestChunk) {
        memory.ReclaimAll();
        return finish(1, "FAIL stop released or dropped the allocation");
    }

    memory.ReclaimAll();
    if (memory.GetStats().appAllocatedBytes != 0 || memory.IsSacrificing()) {
        return finish(1, "FAIL reclaim did not free the allocation");
    }

    if (!memory.StartSacrifice()) {
        return finish(1, "FAIL could not start again after reclaim");
    }
    std::thread emergency([&memory]() {
        Sleep(120);
        memory.RequestEmergencyReclaim();
    });
    emergency.join();
    stats = memory.GetStats();
    if (stats.appAllocatedBytes != 0 || stats.isSacrificing) {
        memory.ReclaimAll();
        return finish(1, "FAIL emergency reclaim left memory behind");
    }

    return finish(0, "PASS allocated, committed, stopped without freeing, reclaimed, and emergency-released");
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int showCommand) {
    if (commandLine && wcsstr(commandLine, L"--self-test")) {
        return RunSelfTest();
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    App app;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &app.d2d)) || !app.d2d) {
        MessageBoxW(nullptr, L"Direct2D could not be created.", L"Altman.exe", MB_OK | MB_ICONERROR);
        if (SUCCEEDED(com)) CoUninitialize();
        return 1;
    }
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(&app.dwrite))) || !app.dwrite) {
        MessageBoxW(nullptr, L"DirectWrite could not be created.", L"Altman.exe", MB_OK | MB_ICONERROR);
        app.d2d->Release();
        if (SUCCEEDED(com)) CoUninitialize();
        return 1;
    }

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP_ICON));
    windowClass.hIconSm = windowClass.hIcon;
    windowClass.lpszClassName = L"AltmanExeWindow";
    if (!RegisterClassExW(&windowClass)) {
        MessageBoxW(nullptr, L"The window class could not be registered.", L"Altman.exe", MB_OK | MB_ICONERROR);
        app.dwrite->Release();
        app.d2d->Release();
        if (SUCCEEDED(com)) CoUninitialize();
        return 1;
    }

    UINT dpi = GetDpiForSystem();
    if (dpi == 0) {
        dpi = 96;
    }
    MONITORINFO primaryInfo{};
    primaryInfo.cbSize = sizeof(primaryInfo);
    HMONITOR primary = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    int width = MulDiv(560, static_cast<int>(dpi), 96);
    int height = MulDiv(860, static_cast<int>(dpi), 96);
    if (GetMonitorInfoW(primary, &primaryInfo)) {
        const int maxW = primaryInfo.rcWork.right - primaryInfo.rcWork.left - 24;
        const int maxH = primaryInfo.rcWork.bottom - primaryInfo.rcWork.top - 24;
        if (width > maxW) width = maxW;
        if (height > maxH) height = maxH;
    }

    HWND hwnd = CreateWindowExW(
        0, windowClass.lpszClassName, L"Altman.exe", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, instance, &app);
    if (!hwnd) {
        MessageBoxW(nullptr, L"The window could not be created.", L"Altman.exe", MB_OK | MB_ICONERROR);
        app.dwrite->Release();
        app.d2d->Release();
        if (SUCCEEDED(com)) CoUninitialize();
        return 1;
    }

    EnableDarkTitle(hwnd);
    if (!app.ui.Initialize(hwnd, app.d2d, app.dwrite)) {
        MessageBoxW(hwnd, L"The graphics device could not be created.", L"Altman.exe", MB_OK | MB_ICONERROR);
        DestroyWindow(hwnd);
        app.dwrite->Release();
        app.d2d->Release();
        if (SUCCEEDED(com)) CoUninitialize();
        return 1;
    }
    app.mascot.Initialize(app.ui.GetRenderTarget(), app.dwrite);

    g_hwnd = hwnd;
    g_memory = &app.memory;
    std::thread hotkey(HotkeyThread);

    ShowWindow(hwnd, showCommand);
    UpdateWindow(hwnd);

    LARGE_INTEGER frequency{};
    LARGE_INTEGER previous{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&previous);

    bool running = true;
    while (running) {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (!running) {
            break;
        }

        if (IsIconic(hwnd)) {
            QueryPerformanceCounter(&previous);
            Sleep(100);
            continue;
        }

        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        float delta = frequency.QuadPart > 0
            ? static_cast<float>(now.QuadPart - previous.QuadPart) / static_cast<float>(frequency.QuadPart)
            : 0.016f;
        previous = now;
        if (delta < 0.0f || delta > 0.1f) {
            delta = 0.016f;
        }

        const MemoryStats stats = app.memory.GetStats();
        app.mascot.Update(delta, stats.sacrificeProgress, stats.isSacrificing, stats.isAccepted, 0.0f, 0.0f);
        app.ui.Update(delta, stats);
        app.ui.Render(app.mascot, stats);

        LARGE_INTEGER after{};
        QueryPerformanceCounter(&after);
        const float spent = frequency.QuadPart > 0
            ? static_cast<float>(after.QuadPart - now.QuadPart) / static_cast<float>(frequency.QuadPart)
            : 0.0f;
        const float remain = 0.016f - spent;
        if (remain > 0.001f) {
            Sleep(static_cast<DWORD>(remain * 1000.0f));
        }
    }

    g_hotkeyStop.store(true);
    if (hotkey.joinable()) {
        hotkey.join();
    }
    g_memory = nullptr;
    g_hwnd = nullptr;

    app.memory.ReclaimAll();
    app.mascot.DiscardDeviceResources();
    app.ui.ReleaseGraphics();
    app.dwrite->Release();
    app.d2d->Release();
    app.dwrite = nullptr;
    app.d2d = nullptr;
    if (SUCCEEDED(com)) {
        CoUninitialize();
    }
    return 0;
}
