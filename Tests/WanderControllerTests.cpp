#include "WanderController.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

void Check(bool, const char*);

namespace {
    constexpr double Pi = 3.14159265358979323846;
    void Near(double actual, double expected, const char* message) {
        Check(std::abs(actual - expected) < 1e-7, message);
    }
    WanderSettings Fixed() {
        WanderSettings settings;
        settings.minimumWaitSeconds = settings.maximumWaitSeconds = 0.25;
        settings.minimumMoveSeconds = settings.maximumMoveSeconds = 2;
        return settings;
    }
    WanderInput Ground() {
        WanderInput input;
        input.enabled = input.physicsValid = input.grounded = true;
        input.minimumX = 0;
        input.maximumX = 10000;
        return input;
    }
}

void TestWanderController() {
    WanderController controller(123);
    controller.SetSettings(Fixed());
    auto input = Ground();
    auto step = controller.Update(0.2, input);
    Check(step.horizontalPixels == 0 && step.phase == WanderPhase::Waiting, "wait before autonomous movement");
    step = controller.Update(0.05, input);
    Check(step.phase == WanderPhase::Turning && step.horizontalPixels == 0, "choose inward direction at left edge");
    step = controller.Update(0.15, input);
    Near(step.facingYawRadians, -Pi / 4, "smooth midpoint faces toward screen right");
    Check(step.horizontalPixels == 0, "stop horizontally during turn");
    step = controller.Update(0.15, input);
    Near(step.facingYawRadians, -Pi / 2, "right-facing turn endpoint");
    Check(step.phase == WanderPhase::Moving, "turn completion starts movement");
    step = controller.Update(0.1, input);
    Check(step.horizontalPixels == 6, "speed is pixels per second");
    input.windowX += static_cast<std::int32_t>(step.horizontalPixels);

    const auto cancel = [&](WanderInput interrupted, const char* message) {
        const double oldYaw = step.facingYawRadians;
        step = controller.Update(0.1, interrupted);
        Check(step.horizontalPixels == 0 && step.phase == WanderPhase::Waiting, message);
        Near(step.facingYawRadians, oldYaw, "interruption preserves current visible facing");
        step = controller.Update(0.2, input);
        Check(step.horizontalPixels == 0 && step.phase == WanderPhase::Waiting, "resuming starts a fresh wait");
    };
    auto interrupted = input; interrupted.blocked = true;
    cancel(interrupted, "drag, rotation, manual input and preview can block movement");
    interrupted = input; interrupted.grounded = false;
    cancel(interrupted, "airborne pet never moves autonomously");
    interrupted = input; interrupted.physicsValid = false;
    cancel(interrupted, "failed physics query stops autonomous movement");
    interrupted = input; interrupted.justLanded = true;
    cancel(interrupted, "landing contact cannot start a walk in the same frame");
    interrupted = input; interrupted.enabled = false;
    cancel(interrupted, "disabled autonomous mode never changes position");
    interrupted = input; interrupted.maximumX = interrupted.minimumX;
    cancel(interrupted, "a pet wider than its range cannot oscillate at the boundary");

    // A completed walk returns to the calibrated front before the next wait.
    for (int fps : { 30, 60, 144 }) {
        for (bool startAtRight : { false, true }) {
            WanderController front(4);
            auto settings = Fixed();
            settings.minimumMoveSeconds = settings.maximumMoveSeconds = 0.5;
            settings.forwardYawRadians = 0.65;
            front.SetSettings(settings);
            auto position = Ground();
            position.windowX = startAtRight ? position.maximumX : position.minimumX;
            const auto startX = position.windowX;
            bool sawReturn = false;
            auto previousPhase = WanderPhase::Waiting;
            WanderStep returned;
            for (int frame = 0; frame < fps * 3 / 2; ++frame) {
                returned = front.Update(1.0 / fps, position);
                if (previousPhase == WanderPhase::Returning)
                    Check(returned.horizontalPixels == 0, "returning to front never adds horizontal travel");
                sawReturn |= returned.phase == WanderPhase::Returning;
                position.windowX += static_cast<std::int32_t>(returned.horizontalPixels);
                previousPhase = returned.phase;
            }
            Check(sawReturn && returned.phase == WanderPhase::Waiting, "both walking directions return before waiting at every frame rate");
            Near(returned.facingYawRadians, settings.forwardYawRadians, "resting yaw uses model forward-axis correction");
            Check(position.windowX - startX == (startAtRight ? -30 : 30), "front return preserves completed walk distance");
            returned = front.Update(0.05, position);
            Near(returned.facingYawRadians, settings.forwardYawRadians, "resting holds front until the next walking turn");
        }
    }

    WanderController returning(5);
    auto returnSettings = Fixed();
    returnSettings.minimumMoveSeconds = returnSettings.maximumMoveSeconds = 0.1;
    returning.SetSettings(returnSettings);
    auto returnInput = Ground();
    const auto advanceReturn = [&](double seconds) {
        const auto result = returning.Update(seconds, returnInput);
        returnInput.windowX += static_cast<std::int32_t>(result.horizontalPixels);
        return result;
    };
    advanceReturn(0.25);
    advanceReturn(0.3);
    step = advanceReturn(0.1);
    Check(step.phase == WanderPhase::Returning, "finished walk starts return rather than a new walk");
    step = advanceReturn(0.15);
    Near(step.facingYawRadians, -Pi / 4, "return smoothly passes through the half-angle");
    const double interruptedReturnYaw = step.facingYawRadians;
    returnInput.blocked = true;
    step = advanceReturn(0.2);
    Check(step.phase == WanderPhase::Waiting && step.horizontalPixels == 0, "interaction cancels the front return");
    Near(step.facingYawRadians, interruptedReturnYaw, "cancelled return does not snap the user's model");
    returnInput.blocked = false;
    step = advanceReturn(0.2);
    Check(step.phase == WanderPhase::Waiting, "interrupted return resumes with a fresh wait");

    returnSettings.turnSeconds = 0;
    returning.SetSettings(returnSettings);
    returnInput.windowX = 0;
    advanceReturn(0.25);
    step = advanceReturn(0.1);
    Check(step.phase == WanderPhase::Waiting, "zero turn duration returns directly to waiting");
    Near(step.facingYawRadians, returnSettings.forwardYawRadians, "zero turn duration applies front immediately");

    // Compare exact distance at common frame rates, including subpixel speeds.
    for (double speed : { 1.0, 60.0 }) {
        for (int fps : { 30, 60, 144 }) {
            WanderController timed(9);
            auto settings = Fixed();
            settings.pixelsPerSecond = speed;
            settings.minimumWaitSeconds = settings.maximumWaitSeconds = settings.turnSeconds = 0;
            settings.minimumMoveSeconds = settings.maximumMoveSeconds = 10;
            timed.SetSettings(settings);
            auto position = Ground();
            for (int frame = 0; frame < fps * 2; ++frame) {
                const auto movement = timed.Update(1.0 / fps, position);
                position.windowX += static_cast<std::int32_t>(movement.horizontalPixels);
            }
            Check(position.windowX == static_cast<std::int32_t>(speed * 2), "movement distance is independent of frame rate");
        }
    }

    WanderController edge(10);
    auto edgeSettings = Fixed();
    edgeSettings.minimumWaitSeconds = edgeSettings.maximumWaitSeconds = 0;
    edge.SetSettings(edgeSettings);
    auto edgeInput = Ground();
    edgeInput.maximumX = 10;
    int reversals = 0;
    auto previousDirection = WalkDirection::Right;
    for (int frame = 0; frame < 180; ++frame) {
        const auto movement = edge.Update(1.0 / 60, edgeInput);
        edgeInput.windowX += static_cast<std::int32_t>(movement.horizontalPixels);
        Check(edgeInput.windowX >= 0 && edgeInput.windowX <= 10, "movement never crosses screen edge");
        if (movement.direction != previousDirection) ++reversals;
        previousDirection = movement.direction;
    }
    Check(reversals > 1 && reversals < 12, "edge turns occur without per-frame reversal");
    edgeInput.windowX = -40;
    step = edge.Update(0.01, edgeInput);
    Check(step.horizontalPixels == 40 && step.phase == WanderPhase::Waiting, "recover outside range then wait");
    edgeInput.windowX = 40;
    step = edge.Update(0.01, edgeInput);
    Check(step.horizontalPixels == -30, "recover from right side of range");

    WanderController first(42), second(42);
    auto firstInput = Ground(), secondInput = Ground();
    firstInput.windowX = secondInput.windowX = -1400;
    firstInput.minimumX = secondInput.minimumX = -1800;
    firstInput.maximumX = secondInput.maximumX = -1000;
    bool sawMove = false, sawWait = false;
    for (int i = 0; i < 2400; ++i) {
        const auto a = first.Update(1.0 / 60, firstInput), b = second.Update(1.0 / 60, secondInput);
        Check(a.horizontalPixels == b.horizontalPixels && a.phase == b.phase && a.direction == b.direction &&
            a.facingYawRadians == b.facingYawRadians, "seeded random schedule is reproducible");
        firstInput.windowX += static_cast<std::int32_t>(a.horizontalPixels);
        secondInput.windowX += static_cast<std::int32_t>(b.horizontalPixels);
        Check(firstInput.windowX >= -1800 && firstInput.windowX <= -1000, "negative monitor coordinates remain valid");
        sawMove |= a.phase == WanderPhase::Moving;
        sawWait |= a.phase == WanderPhase::Waiting;
    }
    Check(sawMove && sawWait, "autonomy alternates moving and resting");

    auto multiplied = Fixed();
    multiplied.minimumWaitSeconds = multiplied.maximumWaitSeconds = multiplied.turnSeconds = 0;
    controller.SetSettings(multiplied);
    auto speedInput = Ground();
    speedInput.speedMultiplier = 2;
    step = controller.Update(0.1, speedInput);
    Check(step.horizontalPixels == 12, "dynamic playback and size multiplier scales movement");
    speedInput.windowX = 12;
    speedInput.speedMultiplier = 0.5;
    step = controller.Update(0.1, speedInput);
    Check(step.horizontalPixels == 3, "speed multiplier change preserves current move without a new wait");

    const auto oldSettings = controller.GetSettings();
    const auto oldPhase = controller.GetPhase();
    auto invalid = oldSettings;
    invalid.maximumWaitSeconds = -1;
    bool rejected = false;
    try { controller.SetSettings(invalid); } catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected && controller.GetSettings().maximumWaitSeconds == oldSettings.maximumWaitSeconds &&
        controller.GetPhase() == oldPhase, "invalid settings cannot partially mutate controller");
    rejected = false;
    try { controller.Update(std::numeric_limits<double>::quiet_NaN(), input); }
    catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected && controller.GetPhase() == oldPhase, "invalid time cannot mutate the schedule");

    auto facingSettings = Fixed();
    facingSettings.forwardYawRadians = Pi / 2;
    controller.SetSettings(facingSettings);
    controller.Reset(0);
    step = controller.Update(0.26, Ground());
    Near(step.facingYawRadians, 0, "forward-axis correction is applied to target facing");
    Check(step.phase == WanderPhase::Moving, "already facing the target does not require a turn");

    controller.Reset(Pi - 0.01);
    auto shortTurn = Fixed();
    shortTurn.forwardYawRadians = -Pi / 2 + 0.01;
    controller.SetSettings(shortTurn);
    controller.Update(0.25, Ground());
    step = controller.Update(0.15, Ground());
    Check(std::abs(std::remainder(step.facingYawRadians - (Pi - 0.01), 2 * Pi)) < 0.02,
        "facing interpolation crosses angle wrap by the shortest arc");
}
