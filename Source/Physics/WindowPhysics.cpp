#include "WindowPhysics.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

PhysicsResult WindowPhysics::Update(HWND hWnd, float deltaTime, float modelBottomInClient, bool isSuspended) {
    const auto frame = Prepare(hWnd, deltaTime, modelBottomInClient, isSuspended);
    return Apply(hWnd, frame, 0, modelBottomInClient);
}

WindowPhysicsFrame WindowPhysics::Prepare(HWND hWnd, float deltaTime, float modelBottomInClient,
    bool isSuspended, HMONITOR preferredMonitor) {
    WindowPhysicsFrame frame;
    if (!hWnd || !std::isfinite(deltaTime) || deltaTime <= 0.0f || !std::isfinite(modelBottomInClient)) return frame;

    RECT windowRect{};
    if (!GetWindowRect(hWnd, &windowRect)) return frame;
    const HMONITOR monitor = preferredMonitor ? preferredMonitor : MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (!GetMonitorInfoW(monitor, &monitorInfo)) return frame;
    RECT clientRect{};
    if (!GetClientRect(hWnd, &clientRect)) return frame;
    POINT clientOrigin{};
    if (!ClientToScreen(hWnd, &clientOrigin)) return frame;

    const float clientTopOffset = static_cast<float>(clientOrigin.y - windowRect.top);
    const float clientHeight = static_cast<float>(clientRect.bottom - clientRect.top);
    if (clientHeight <= 0) return frame;
    const float safeModelBottom = std::max(0.0f, std::min(modelBottomInClient, clientHeight));
    const float currentModelBottom = static_cast<float>(windowRect.top) + clientTopOffset + safeModelBottom;
    const float distanceToGround = static_cast<float>(monitorInfo.rcWork.bottom) - currentModelBottom;
    const auto step = simulation.Step(distanceToGround, deltaTime, isSuspended);
    frame.result = step.result;
    frame.windowRect = windowRect;
    frame.workArea = monitorInfo.rcWork;
    frame.clientOffset = { clientOrigin.x - windowRect.left, clientOrigin.y - windowRect.top };
    frame.monitor = monitor;
    frame.clientHeight = clientRect.bottom - clientRect.top;
    frame.verticalPixels = step.pixelMovement;
    frame.suspended = isSuspended;
    return frame;
}

PhysicsResult WindowPhysics::Apply(HWND hWnd, const WindowPhysicsFrame& frame, std::int64_t horizontalPixels,
    float finalModelBottomInClient) {
    if (!hWnd || !frame.result.valid || !std::isfinite(finalModelBottomInClient)) {
        simulation.Reset();
        return {};
    }
    RECT current{};
    if (!GetWindowRect(hWnd, &current) || current.left != frame.windowRect.left || current.top != frame.windowRect.top ||
        current.right != frame.windowRect.right || current.bottom != frame.windowRect.bottom) {
        simulation.Reset();
        return {};
    }
    if (frame.suspended) return frame.result;
    const auto originX = static_cast<std::int64_t>(frame.windowRect.left);
    if (horizontalPixels < std::numeric_limits<LONG>::min() - originX ||
        horizontalPixels > std::numeric_limits<LONG>::max() - originX) {
        simulation.Reset();
        return {};
    }
    const auto x = originX + horizontalPixels;
    auto y = static_cast<std::int64_t>(frame.windowRect.top) + frame.verticalPixels;
    if (frame.result.grounded) {
        // A yaw change alters perspective-projected height. Keep an existing
        // contact attached to the same floor without generating a new fall/land.
        const double bottom = std::clamp(static_cast<double>(finalModelBottomInClient), 0.0,
            static_cast<double>(frame.clientHeight));
        y = static_cast<std::int64_t>(std::floor(static_cast<double>(frame.workArea.bottom) - frame.clientOffset.y - bottom));
    }
    if (x < std::numeric_limits<LONG>::min() || x > std::numeric_limits<LONG>::max() ||
        y < std::numeric_limits<LONG>::min() || y > std::numeric_limits<LONG>::max()) {
        simulation.Reset();
        return {};
    }
    if ((x != frame.windowRect.left || y != frame.windowRect.top) &&
        !SetWindowPos(hWnd, nullptr, static_cast<int>(x), static_cast<int>(y), 0, 0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)) {
        simulation.Reset();
        return {};
    }
    return frame.result;
}

void WindowPhysics::ResetVelocity() {
    simulation.Reset();
}
