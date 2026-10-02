#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <cstdint>
#include "GravitySimulation.hpp"

struct WindowPhysicsFrame {
    PhysicsResult result;
    RECT windowRect{};
    RECT workArea{};
    POINT clientOffset{};
    HMONITOR monitor = nullptr;
    LONG clientHeight = 0;
    int verticalPixels = 0;
    bool suspended = false;
};

class WindowPhysics {
public:
    PhysicsResult Update(HWND hWnd, float deltaTime, float modelBottomInClient, bool isSuspended);
    // Prepare gravity without changing HWND position. Apply combines both axes.
    WindowPhysicsFrame Prepare(HWND hWnd, float deltaTime, float modelBottomInClient,
        bool isSuspended, HMONITOR preferredMonitor = nullptr);
    PhysicsResult Apply(HWND hWnd, const WindowPhysicsFrame& frame, std::int64_t horizontalPixels,
        float finalModelBottomInClient);
    void ResetVelocity();

private:
    GravitySimulation simulation;
};
