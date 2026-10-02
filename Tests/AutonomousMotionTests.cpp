#include "AutonomousMotion.hpp"
#include "Graphics.hpp"
#include <d3d11sdklayers.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

void Check(bool, const char*);

namespace {
    class HiddenMotionWindow {
    public:
        HiddenMotionWindow(int width, int height) {
            WNDCLASSW wc{};
            wc.lpfnWndProc = DefWindowProcW;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = Name();
            Check(RegisterClassW(&wc) != 0, "register hidden motion window");
            handle = CreateWindowExW(WS_EX_TOPMOST | WS_EX_NOACTIVATE, Name(), L"Hidden motion test", WS_POPUP,
                0, 0, width, height, nullptr, nullptr, wc.hInstance, nullptr);
            Check(handle != nullptr, "create hidden motion window");
        }
        ~HiddenMotionWindow() {
            DestroyWindow(handle);
            UnregisterClassW(Name(), GetModuleHandleW(nullptr));
        }
        HWND Get() const { return handle; }
    private:
        static const wchar_t* Name() { return L"DesktopPetHiddenMotionTest"; }
        HWND handle = nullptr;
    };
    RECT Position(HWND window) {
        RECT rect{};
        Check(GetWindowRect(window, &rect) != FALSE, "read test window position");
        return rect;
    }
    RECT WorkArea(HWND window) {
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        Check(GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &info) != FALSE, "read monitor work area");
        return info.rcWork;
    }
    void Place(HWND window, int x, int y) {
        Check(SetWindowPos(window, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
            "place hidden test window");
    }
}

void TestWindowPhysicsMotion() {
    HiddenMotionWindow window(100, 100);
    const auto work = WorkArea(window.Get());
    const int x = work.left + 100;
    const float initialBottom = 80.4f;
    Place(window.Get(), x, static_cast<int>(std::floor(work.bottom - initialBottom)));
    WindowPhysics physics;
    const auto before = Position(window.Get());
    auto frame = physics.Prepare(window.Get(), 1.0f / 60, initialBottom, false);
    Check(frame.result.valid && frame.result.grounded && !frame.result.justLanded, "prepare initial floor contact");
    Check(Position(window.Get()).top == before.top && Position(window.Get()).left == before.left,
        "preparing gravity does not move HWND");
    auto result = physics.Apply(window.Get(), frame, 7, 60.4f);
    auto after = Position(window.Get());
    Check(result.grounded && !result.justLanded && after.left == x + 7 && after.top == before.top + 20,
        "one commit combines horizontal travel and changed projected height");
    Check(!IsWindowVisible(window.Get()) && (GetWindowLongPtrW(window.Get(), GWL_EXSTYLE) & WS_EX_TOPMOST),
        "motion does not show or change topmost status");

    float bottom = 60.4f;
    for (int i = 0; i < 120; ++i) {
        frame = physics.Prepare(window.Get(), 1.0f / 60, bottom, false);
        Check(frame.result.grounded && !frame.result.justLanded, "turn height compensation does not create landing pulses");
        bottom = 60.4f + 20 * std::sin(i * 0.08f);
        result = physics.Apply(window.Get(), frame, 1, bottom);
        after = Position(window.Get());
        const double gap = work.bottom - (after.top + bottom);
        Check(result.valid && result.grounded && gap >= -0.001 && gap <= 1.001, "turning stays within one pixel of floor");
    }
    const auto suspendedPosition = Position(window.Get());
    frame = physics.Prepare(window.Get(), 1.0f / 60, bottom, true);
    result = physics.Apply(window.Get(), frame, 50, bottom);
    Check(result.valid && !result.grounded && Position(window.Get()).left == suspendedPosition.left,
        "suspended physics never applies horizontal motion");

    frame = physics.Prepare(window.Get(), 1.0f / 60, bottom, false);
    Place(window.Get(), frame.windowRect.left + 1, frame.windowRect.top);
    Check(!physics.Apply(window.Get(), frame, 9, bottom).valid, "stale position snapshot cannot overwrite an external move");
    frame = physics.Prepare(window.Get(), 1.0f / 60, bottom, false);
    Check(!physics.Apply(window.Get(), frame, std::numeric_limits<std::int64_t>::max(), bottom).valid,
        "huge horizontal movement is rejected without integer overflow");
    Check(!physics.Prepare(nullptr, 0.01f, bottom, false).result.valid, "invalid window is rejected");

    Place(window.Get(), x, work.bottom - 400);
    physics.ResetVelocity();
    frame = physics.Prepare(window.Get(), 0.05f, initialBottom, false);
    const auto airborne = Position(window.Get());
    result = physics.Apply(window.Get(), frame, 9, initialBottom);
    after = Position(window.Get());
    Check(result.valid && !result.grounded && after.left == airborne.left + 9 &&
        after.top == airborne.top + frame.verticalPixels, "gravity and horizontal motion preserve both coordinates");
}

void TestAutonomousMotion(const char* modelPath) {
    HiddenMotionWindow window(600, 600);
    Graphics graphics(window.Get(), 600, 600, modelPath, D3D_DRIVER_TYPE_WARP);
    const float originalBottom = graphics.GetModelBottomInClient();
    graphics.RotateModel(10, 3);
    const auto manualRotation = graphics.modelRotation;
    const auto bounds = graphics.GetWanderBoundsInClient();
    Check(bounds.valid && bounds.right > bounds.left, "compute stable all-yaw horizontal envelope");
    const auto work = WorkArea(window.Get());
    const int minimum = static_cast<int>(std::ceil(work.left - bounds.left));
    const int maximum = static_cast<int>(std::floor(work.right - bounds.right));
    Check(minimum < maximum, "test model fits monitor width");
    Place(window.Get(), minimum, static_cast<int>(std::floor(work.bottom - graphics.GetModelBottomInClient())));
    AutonomousMotion motion(123);
    auto settings = graphics.GetBehavior().GetSettings();
    settings.autonomousWalking = true;
    settings.wander.minimumWaitSeconds = settings.wander.maximumWaitSeconds = 0.05;
    settings.wander.minimumMoveSeconds = settings.wander.maximumMoveSeconds = 10;
    graphics.GetBehavior().SetSettings(settings);
    Microsoft::WRL::ComPtr<ID3D11InfoQueue> info;
    Check(SUCCEEDED(graphics.GetDevice()->QueryInterface(IID_PPV_ARGS(&info))), "motion debug info queue");
    info->ClearStoredMessages();
    const auto tick = [&](bool suspended = false, bool blocked = false, bool dragging = false) {
        const auto physics = motion.Update(window.Get(), 1.0f / 60, graphics, suspended, blocked);
        PetBehaviorInput input;
        input.dragging = dragging;
        input.physicsSuspended = suspended;
        input.physicsValid = physics.valid;
        input.grounded = physics.grounded;
        input.justLanded = physics.justLanded;
        input.walking = motion.IsWalking();
        graphics.UpdateBehavior(1.0 / 60, input);
        return physics;
    };
    if (!graphics.GetBehavior().HasWalkAnimation()) {
        for (int i = 0; i < 150; ++i) {
            const auto physics = tick();
            Check(physics.valid && physics.grounded && graphics.GetBehavior().GetState() == PetState::Idle,
                "missing walk clip preserves idle and normal gravity");
        }
        Check(Position(window.Get()).left == minimum && graphics.GetAutonomousFacing() == 0,
            "missing walk clip disables translation and automatic rotation");
        graphics.Render();
        std::cout << "Missing walk clip disables autonomy: " << modelPath << '\n';
        return;
    }
    int landings = 0;
    bool sawWalking = false, sawWaiting = false;
    for (int i = 0; i < 150; ++i) {
        const auto physics = tick();
        Check(physics.valid && physics.grounded, "loaded model stays grounded during autonomous travel");
        landings += physics.justLanded ? 1 : 0;
        sawWalking |= graphics.GetBehavior().GetState() == PetState::Walk;
        sawWaiting |= graphics.GetBehavior().GetState() == PetState::Idle;
        if (motion.IsWalking()) Check(graphics.GetAnimator().GetClipIndex() == graphics.GetBehavior().GetAssignedClip(PetState::Walk),
            "actual travel selects assigned walking animation");
        const auto rect = Position(window.Get());
        Check(rect.left >= minimum && rect.left <= maximum, "loaded model stays in its pinned monitor");
        const auto currentBounds = graphics.GetWanderBoundsInClient();
        Check(currentBounds.left == bounds.left && currentBounds.right == bounds.right,
            "horizontal envelope does not change with animated pose or autonomous yaw");
        const double gap = work.bottom - (rect.top + graphics.GetModelBottomInClient());
        Check(gap >= -0.001 && gap <= 1.001, "visible bind ground remains attached during turn");
        if (i % 10 == 0) {
            const auto wvp = graphics.GetModelMatrix() * graphics.camera->GetViewMatrix() * graphics.camera->GetProjectionMatrix();
            for (const auto& vertex : graphics.model->GetGroundingVertices()) {
                const auto projected = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&vertex), wvp);
                const float visibleX = std::clamp((DirectX::XMVectorGetX(projected) + 1) * 300, 0.0f, 600.0f);
                Check(visibleX >= bounds.left - 0.01f && visibleX <= bounds.right + 0.01f,
                    "projected geometry fits conservative turn envelope");
            }
        }
        if (i == 10 || i == 40 || i == 149) graphics.Render();
    }
    Check(Position(window.Get()).left > minimum + 40 && landings == 0 && sawWalking && sawWaiting,
        "loaded model travels after waiting without repeated landing animations");
    Check(graphics.modelRotation.x == manualRotation.x && graphics.modelRotation.y == manualRotation.y &&
        graphics.modelRotation.z == manualRotation.z && graphics.modelRotation.w == manualRotation.w,
        "automatic heading never overwrites manual quaternion");

    auto held = Position(window.Get());
    const auto heldYaw = graphics.GetAutonomousFacing();
    for (int i = 0; i < 30; ++i) tick(true, false, true);
    Check(Position(window.Get()).left == held.left && Position(window.Get()).top == held.top &&
        graphics.GetAutonomousFacing() == heldYaw, "drag and manual rotation freeze translation and facing");
    Check(graphics.GetBehavior().GetState() == PetState::Dragged, "drag animation overrides walking immediately");
    tick();
    Check(Position(window.Get()).left == held.left, "release resumes with a fresh wait");
    tick(false, true);
    Check(Position(window.Get()).left == held.left, "settings interaction stops wandering");
    const auto clip = graphics.GetBehavior().GetAssignedClip(PetState::Idle);
    if (clip != InvalidSkeletonNode) {
        graphics.GetBehavior().Preview(clip);
        tick(true);
        Check(Position(window.Get()).left == held.left, "animation preview blocks movement");
        graphics.GetBehavior().StopPreview();
    }
    settings.autonomousWalking = false;
    graphics.GetBehavior().SetSettings(settings);
    for (int i = 0; i < 60; ++i) tick();
    Check(Position(window.Get()).left == held.left, "autonomous mode defaults to a controllable stop");

    Place(window.Get(), maximum, static_cast<int>(std::floor(work.bottom - graphics.GetModelBottomInClient())));
    settings.autonomousWalking = true;
    graphics.GetBehavior().SetSettings(settings);
    for (int i = 0; i < 150; ++i) tick();
    Check(Position(window.Get()).left < maximum - 40 && motion.GetLastStep().direction == WalkDirection::Left,
        "right boundary selects inward travel and left-facing turn");

    settings.wander.minimumWaitSeconds = settings.wander.maximumWaitSeconds = 0.25;
    settings.wander.minimumMoveSeconds = settings.wander.maximumMoveSeconds = 0.5;
    settings.wander.forwardYawRadians = 0.37;
    graphics.GetBehavior().SetSettings(settings);
    Place(window.Get(), minimum, static_cast<int>(std::floor(work.bottom - graphics.GetModelBottomInClient())));
    bool sawReturning = false;
    auto previousPhase = WanderPhase::Waiting;
    auto previousX = minimum;
    for (int frame = 0; frame < 90; ++frame) {
        const auto physics = tick();
        const auto current = Position(window.Get());
        Check(physics.valid && physics.grounded && !physics.justLanded,
            "return to front preserves grounded contact without a landing event");
        if (previousPhase == WanderPhase::Returning)
            Check(current.left == previousX, "front return changes facing without sliding the HWND");
        if (motion.GetLastStep().phase == WanderPhase::Returning) {
            sawReturning = true;
            Check(!motion.IsWalking() && graphics.GetBehavior().GetState() == PetState::Idle,
                "front return plays idle rather than walking in place");
        }
        const double gap = work.bottom - (current.top + graphics.GetModelBottomInClient());
        Check(gap >= -0.001 && gap <= 1.001, "front-facing turn keeps bind ground at the monitor floor");
        previousX = current.left;
        previousPhase = motion.GetLastStep().phase;
    }
    Check(sawReturning && motion.GetLastStep().phase == WanderPhase::Waiting &&
        std::abs(graphics.GetAutonomousFacing() - settings.wander.forwardYawRadians) < 1e-6 &&
        graphics.GetBehavior().GetState() == PetState::Idle,
        "real model completes its walk and rests facing the calibrated front");
    Check(graphics.modelRotation.x == manualRotation.x && graphics.modelRotation.y == manualRotation.y &&
        graphics.modelRotation.z == manualRotation.z && graphics.modelRotation.w == manualRotation.w,
        "front return preserves user quaternion rotation");
    graphics.Render();

    settings.wander.minimumWaitSeconds = settings.wander.maximumWaitSeconds = 0;
    settings.wander.turnSeconds = 0;
    settings.wander.minimumMoveSeconds = settings.wander.maximumMoveSeconds = 10;
    settings.states[WalkStateIndex].speed = 2;
    for (int size : { 600, 300 }) {
        Check(SetWindowPos(window.Get(), nullptr, 0, 0, size, size, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
            "resize motion test window");
        graphics.GetBehavior().SetSettings(settings);
        motion.Reset(graphics.GetAutonomousFacing());
        const auto sizeBounds = graphics.GetWanderBoundsInClient();
        const int left = static_cast<int>(std::ceil(work.left - sizeBounds.left));
        Place(window.Get(), left, static_cast<int>(std::floor(work.bottom - graphics.GetModelBottomInClient())));
        for (int frame = 0; frame < 30; ++frame) tick();
        Check(Position(window.Get()).left - left == size / 10 && graphics.GetAnimator().GetSpeed() == 2,
            "movement speed follows walking playback multiplier and model window size");
    }
    SetWindowPos(window.Get(), nullptr, 0, 0, 600, 600, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    graphics.ResetModelRotation();
    motion.Reset(graphics.GetAutonomousFacing());
    Check(graphics.GetAutonomousFacing() == 0 && std::abs(graphics.GetModelBottomInClient() - originalBottom) < 0.001f,
        "rotation reset restores original user and autonomous orientation");
    for (UINT64 i = 0; i < info->GetNumStoredMessages(); ++i) {
        SIZE_T length = 0;
        info->GetMessage(i, nullptr, &length);
        std::vector<unsigned char> data(length);
        auto* message = reinterpret_cast<D3D11_MESSAGE*>(data.data());
        info->GetMessage(i, message, &length);
        if (message->Severity <= D3D11_MESSAGE_SEVERITY_ERROR) {
            std::cerr << message->pDescription << '\n';
            Check(false, "autonomous rendering has no D3D11 errors");
        }
    }
    Check(!IsWindowVisible(window.Get()), "motion testing never displays its test window");
    std::cout << "Autonomous movement, facing and stable floor passed: " << modelPath << '\n';
}
