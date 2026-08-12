#include "WindowPhysics.hpp"

#include <algorithm>
#include <cmath>

void WindowPhysics::Update(
    HWND hWnd,
    float deltaTime,
    float modelBottomInClient,
    bool isSuspended) {

    if (!hWnd || deltaTime <= 0.0f) {
        return;
    }

    if (isSuspended) {
        ResetVelocity();
        wasSuspended = true;
        return;
    }

    // 드래그 또는 조작 모드가 끝난 프레임부터 항상 새 낙하로 시작한다.
    if (wasSuspended) {
        ResetVelocity();
        wasSuspended = false;
    }

    RECT windowRect = {};
    if (!GetWindowRect(hWnd, &windowRect)) {
        return;
    }

    HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfo(monitor, &monitorInfo)) {
        return;
    }

    RECT clientRect = {};
    if (!GetClientRect(hWnd, &clientRect)) {
        return;
    }

    POINT clientOrigin = { 0, 0 };
    if (!ClientToScreen(hWnd, &clientOrigin)) {
        return;
    }

    const float clientTopOffset = static_cast<float>(clientOrigin.y - windowRect.top);
    const float clientHeight = static_cast<float>(clientRect.bottom - clientRect.top);
    const float safeModelBottom = std::max(0.0f, std::min(modelBottomInClient, clientHeight));
    const float currentModelBottom =
        static_cast<float>(windowRect.top) + clientTopOffset + safeModelBottom;
    const float groundY = static_cast<float>(monitorInfo.rcWork.bottom);
    const float distanceToGround = groundY - currentModelBottom;

    if (distanceToGround <= 0.0f) {
        const int correctedWindowY = windowRect.top + static_cast<int>(std::floor(distanceToGround));
        SetWindowPos(
            hWnd,
            nullptr,
            windowRect.left,
            correctedWindowY,
            0,
            0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        ResetVelocity();
        return;
    }

    verticalVelocity = std::min(verticalVelocity + gravity * deltaTime, maxFallSpeed);
    float fallDistance = verticalVelocity * deltaTime + subpixelY;

    if (fallDistance >= distanceToGround) {
        fallDistance = distanceToGround;
        verticalVelocity = 0.0f;
        subpixelY = 0.0f;
    }

    const int pixelMovement = static_cast<int>(std::floor(fallDistance));
    subpixelY = fallDistance - static_cast<float>(pixelMovement);

    if (pixelMovement > 0) {
        SetWindowPos(
            hWnd,
            nullptr,
            windowRect.left,
            windowRect.top + pixelMovement,
            0,
            0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void WindowPhysics::ResetVelocity() {
    verticalVelocity = 0.0f;
    subpixelY = 0.0f;
}
