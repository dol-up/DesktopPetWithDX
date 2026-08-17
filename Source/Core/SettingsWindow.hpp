#pragma once

#include <windows.h>

class SettingsWindow {
public:
    explicit SettingsWindow(HWND owner);
    ~SettingsWindow();

    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    void Show();

private:
    static LRESULT CALLBACK WindowProcSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WindowProcForward(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void CreateControls();

    HWND hWnd = nullptr;
    HINSTANCE hInstance = nullptr;
};
