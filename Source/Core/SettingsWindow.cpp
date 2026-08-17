#include "SettingsWindow.hpp"

#include <algorithm>

namespace {
    constexpr wchar_t SettingsClassName[] = L"DesktopPetSettingsWindow";
    constexpr int SettingsClientWidth = 360;
    constexpr int SettingsClientHeight = 130;

    int ClampPosition(int value, int minimum, int maximum) {
        return std::max(minimum, std::min(value, maximum));
    }

    void SetControlFont(HWND control, HFONT font) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
}

SettingsWindow::SettingsWindow(HWND owner)
    : hInstance(GetModuleHandleW(nullptr)) {

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProcSetup;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = SettingsClassName;
    RegisterClassExW(&windowClass);

    RECT windowRect = { 0, 0, SettingsClientWidth, SettingsClientHeight };
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    const DWORD extendedStyle = WS_EX_TOOLWINDOW;
    AdjustWindowRectEx(&windowRect, style, FALSE, extendedStyle);

    CreateWindowExW(
        extendedStyle,
        SettingsClassName,
        L"데스크톱 펫 설정",
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        owner,
        nullptr,
        hInstance,
        this);
}

SettingsWindow::~SettingsWindow() {
    if (hWnd) {
        DestroyWindow(hWnd);
    }
    UnregisterClassW(SettingsClassName, hInstance);
}

void SettingsWindow::Show() {
    if (!hWnd) {
        return;
    }

    POINT cursor = {};
    GetCursorPos(&cursor);

    RECT windowRect = {};
    GetWindowRect(hWnd, &windowRect);
    const int width = windowRect.right - windowRect.left;
    const int height = windowRect.bottom - windowRect.top;

    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfoW(monitor, &monitorInfo);

    const int x = ClampPosition(
        cursor.x + 12,
        monitorInfo.rcWork.left,
        monitorInfo.rcWork.right - width);
    const int y = ClampPosition(
        cursor.y + 12,
        monitorInfo.rcWork.top,
        monitorInfo.rcWork.bottom - height);

    SetWindowPos(hWnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(hWnd);
}

LRESULT CALLBACK SettingsWindow::WindowProcSetup(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam) {

    if (msg == WM_NCCREATE) {
        const CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SettingsWindow* window = static_cast<SettingsWindow*>(create->lpCreateParams);
        window->hWnd = hWnd;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        SetWindowLongPtrW(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WindowProcForward));
        return window->HandleMessage(hWnd, msg, wParam, lParam);
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

LRESULT CALLBACK SettingsWindow::WindowProcForward(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam) {

    SettingsWindow* window = reinterpret_cast<SettingsWindow*>(
        GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (!window) {
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return window->HandleMessage(hWnd, msg, wParam, lParam);
}

LRESULT SettingsWindow::HandleMessage(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam) {

    switch (msg) {
    case WM_CREATE:
        CreateControls();
        return 0;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void SettingsWindow::CreateControls() {
    HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    HWND group = CreateWindowExW(
        0, L"BUTTON", L"설정", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        12, 10, 336, 105, hWnd, nullptr, hInstance, nullptr);
    SetControlFont(group, font);

    HWND message = CreateWindowExW(
        0, L"STATIC", L"설정 항목은 추후 추가됩니다.", WS_CHILD | WS_VISIBLE | SS_CENTER,
        30, 53, 300, 24, hWnd, nullptr, hInstance, nullptr);
    SetControlFont(message, font);
}
