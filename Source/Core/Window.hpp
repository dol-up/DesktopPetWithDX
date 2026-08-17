#pragma once

#define NOMINMAX

#include <windows.h>
#include <functional>

class Window {
public:
    Window(int width, int height, const char* name);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool ProcessMessages();
    HWND GetHWND() const { return hWnd; }
    bool IsDragging() const { return isDragging; }
    bool IsRotating() const { return isRotating; }
    void SetModelHitTest(std::function<bool(int, int)> hitTest) { modelHitTest = hitTest; }
    void SetModelRotate(std::function<void(float, float)> rotate) { modelRotate = rotate; }
    void SetSettingsRequested(std::function<void()> callback) { settingsRequested = callback; }




private:
    static LRESULT CALLBACK WindowProcSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WindowProcForward(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hWnd;
    HINSTANCE hInstance;

    bool isDragging = false;
    bool isRotating = false;
    POINT lastMousePos = { 0, 0 };
    POINT dragStartPos = { 0, 0 };
    POINT lastRotationMousePos = { 0, 0 };
    std::function<bool(int, int)> modelHitTest;
    std::function<void(float, float)> modelRotate;
    std::function<void()> settingsRequested;
};
