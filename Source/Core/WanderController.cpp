#include "WanderController.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
    constexpr double Pi = 3.14159265358979323846;
    constexpr double Epsilon = 1e-9;
    double Wrapped(double angle) { return std::remainder(angle, 2 * Pi); }
    bool InRange(double value, double minimum, double maximum) {
        return std::isfinite(value) && value >= minimum && value <= maximum;
    }
}

bool operator==(const WanderSettings& a, const WanderSettings& b) {
    return a.pixelsPerSecond == b.pixelsPerSecond && a.minimumWaitSeconds == b.minimumWaitSeconds &&
        a.maximumWaitSeconds == b.maximumWaitSeconds && a.minimumMoveSeconds == b.minimumMoveSeconds &&
        a.maximumMoveSeconds == b.maximumMoveSeconds && a.turnSeconds == b.turnSeconds && a.forwardYawRadians == b.forwardYawRadians;
}

WanderController::WanderController(std::uint32_t seed) : random(seed) {}

void WanderController::Reset(double currentYawRadians) {
    if (!std::isfinite(currentYawRadians)) throw std::invalid_argument("Invalid facing angle");
    yaw = Wrapped(currentYawRadians);
    phase = WanderPhase::Waiting;
    active = false;
    waitRemaining = moveRemaining = subpixel = turnElapsed = 0;
}

void WanderController::ValidateSettings(const WanderSettings& value) {
    if (!InRange(value.pixelsPerSecond, 1, 2000) ||
        !InRange(value.minimumWaitSeconds, 0, 3600) ||
        !InRange(value.maximumWaitSeconds, value.minimumWaitSeconds, 3600) ||
        !InRange(value.minimumMoveSeconds, 0.1, 60) ||
        !InRange(value.maximumMoveSeconds, value.minimumMoveSeconds, 60) ||
        !InRange(value.turnSeconds, 0, 2) || !InRange(value.forwardYawRadians, -Pi, Pi))
        throw std::invalid_argument("Invalid wander settings");
}

void WanderController::SetSettings(const WanderSettings& value) {
    ValidateSettings(value);
    settings = value;
    Reset(yaw);
}

double WanderController::RandomSeconds(double minimum, double maximum) {
    return minimum == maximum ? minimum : std::uniform_real_distribution<double>(minimum, maximum)(random);
}

void WanderController::StartWaiting() {
    phase = WanderPhase::Waiting;
    waitRemaining = RandomSeconds(settings.minimumWaitSeconds, settings.maximumWaitSeconds);
    subpixel = 0;
}

void WanderController::StartTurning(WalkDirection nextDirection) {
    direction = nextDirection;
    // With the camera on -Z, a model facing -Z turns toward +X with negative yaw.
    const double target = settings.forwardYawRadians - static_cast<int>(direction) * Pi / 2;
    StartRotation(target, WanderPhase::Turning);
}

void WanderController::StartRotation(double targetYaw, WanderPhase turnPhase) {
    turnStart = yaw;
    turnDelta = Wrapped(targetYaw - yaw);
    turnElapsed = 0;
    phase = turnPhase;
    subpixel = 0;
    if (settings.turnSeconds == 0 || std::abs(turnDelta) < Epsilon) {
        yaw = Wrapped(targetYaw);
        if (phase == WanderPhase::Returning) StartWaiting();
        else phase = WanderPhase::Moving;
    }
}

WanderStep WanderController::Result(std::int64_t pixels) const {
    return { pixels, yaw, phase, direction };
}

WanderStep WanderController::Update(double dt, const WanderInput& input) {
    if (!std::isfinite(dt) || dt < 0) throw std::invalid_argument("Invalid wander frame time");
    if (!InRange(input.speedMultiplier, 0.001, 64)) throw std::invalid_argument("Invalid movement multiplier");
    if (!input.enabled || input.blocked || !input.physicsValid || !input.grounded || input.justLanded ||
        input.minimumX >= input.maximumX) {
        Reset(yaw);
        return Result();
    }

    // An external move, resize or work-area change can leave the pet outside its
    // allowed range. Recover once, then start a fresh wait instead of oscillating.
    const auto initialX = static_cast<std::int64_t>(input.windowX);
    const auto safeX = std::clamp(initialX, static_cast<std::int64_t>(input.minimumX),
        static_cast<std::int64_t>(input.maximumX));
    if (initialX != safeX) {
        Reset(yaw);
        return Result(safeX - initialX);
    }
    if (!active) { active = true; StartWaiting(); }
    auto x = initialX;
    double remaining = dt;
    // Bound work for a pathological frame time; normal frames consume all dt.
    for (int transitions = 0; remaining > Epsilon && transitions < 32; ++transitions) {
        if (phase == WanderPhase::Waiting) {
            const double elapsed = std::min(remaining, waitRemaining);
            waitRemaining -= elapsed;
            remaining -= elapsed;
            if (waitRemaining > Epsilon) break;
            moveRemaining = RandomSeconds(settings.minimumMoveSeconds, settings.maximumMoveSeconds);
            const auto chosen = x <= input.minimumX ? WalkDirection::Right : x >= input.maximumX ? WalkDirection::Left :
                (std::uniform_int_distribution<int>(0, 1)(random) ? WalkDirection::Right : WalkDirection::Left);
            StartTurning(chosen);
        } else if (phase == WanderPhase::Turning || phase == WanderPhase::Returning) {
            const double elapsed = std::min(remaining, settings.turnSeconds - turnElapsed);
            turnElapsed += elapsed;
            remaining -= elapsed;
            const double t = std::clamp(turnElapsed / settings.turnSeconds, 0.0, 1.0);
            const double smooth = t * t * (3 - 2 * t);
            yaw = Wrapped(turnStart + turnDelta * smooth);
            if (t < 1 - Epsilon) break;
            yaw = Wrapped(turnStart + turnDelta);
            if (phase == WanderPhase::Returning) StartWaiting();
            else phase = WanderPhase::Moving;
        } else {
            const double speed = settings.pixelsPerSecond * input.speedMultiplier;
            const int sign = static_cast<int>(direction);
            const double distance = sign > 0 ? static_cast<double>(input.maximumX - x) - subpixel :
                static_cast<double>(x - input.minimumX) + subpixel;
            const double toEdge = std::max(0.0, distance) / speed;
            const double elapsed = std::min({ remaining, moveRemaining, toEdge });
            const double movement = sign * speed * elapsed + subpixel;
            const double rounded = std::round(movement);
            const auto pixels = static_cast<std::int64_t>(std::abs(movement - rounded) < Epsilon ? rounded : std::trunc(movement));
            x = std::clamp(x + pixels, static_cast<std::int64_t>(input.minimumX), static_cast<std::int64_t>(input.maximumX));
            subpixel = movement - static_cast<double>(pixels);
            if (std::abs(subpixel) < Epsilon) subpixel = 0;
            moveRemaining -= elapsed;
            remaining -= elapsed;
            if (moveRemaining <= Epsilon) StartRotation(settings.forwardYawRadians, WanderPhase::Returning);
            else if (toEdge <= elapsed + Epsilon) {
                x = sign > 0 ? input.maximumX : input.minimumX;
                StartTurning(sign > 0 ? WalkDirection::Left : WalkDirection::Right);
            } else break;
        }
    }
    return Result(x - initialX);
}
