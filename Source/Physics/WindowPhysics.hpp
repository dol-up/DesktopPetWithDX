#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

class WindowPhysics {
public:
    void Update(HWND hWnd, float deltaTime, float modelBottomInClient, bool isSuspended);
    void ResetVelocity();

private:
    float verticalVelocity = 0.0f;
    float subpixelY = 0.0f;
    bool wasSuspended = false;

    static constexpr float gravity = 1800.0f;
    static constexpr float maxFallSpeed = 2400.0f;
};
