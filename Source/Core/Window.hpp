#pragma once

#define NOMINMAX

#include <windows.h>

class Window {
public:
    Window(int width, int height, const char* name);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool ProcessMessages();
    HWND GetHWND() const { return hWnd; }

private:
    static LRESULT CALLBACK WindowProcSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WindowProcForward(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hWnd;
    HINSTANCE hInstance;

    bool isDragging = false;
    POINT lastMousePos = { 0, 0 };
    POINT dragStartPos = { 0, 0 };
};