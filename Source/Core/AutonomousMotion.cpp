#include "AutonomousMotion.hpp"
#include "Graphics.hpp"
#include <cmath>
#include <limits>
#include <algorithm>

void AutonomousMotion::Reset(double currentYawRadians) {
    physics.ResetVelocity();
    wander.Reset(currentYawRadians);
    walkingMonitor = nullptr;
    lastStep = { 0, currentYawRadians };
}

PhysicsResult AutonomousMotion::Update(HWND window, float dt, Graphics& graphics,
    bool physicsSuspended, bool interactionBlocked) {
    const double oldYaw = graphics.GetAutonomousFacing();
    const auto& behavior = graphics.GetBehavior();
    const auto& settings = behavior.GetSettings();
    const auto clip = behavior.GetAssignedClip(PetState::Walk);
    const bool canWalk = settings.autonomousWalking && behavior.HasWalkAnimation();
    if (!(settings.wander == wander.GetSettings())) {
        wander.SetSettings(settings.wander);
        Reset(oldYaw);
    }
    if (enabled != canWalk || walkingClip != clip) {
        enabled = canWalk;
        walkingClip = clip;
        wander.Reset(oldYaw);
        walkingMonitor = nullptr;
    }
    const bool blocked = physicsSuspended || interactionBlocked || graphics.GetBehavior().IsPreviewing() ||
        graphics.GetBehavior().GetState() == PetState::Landing;
    if (!enabled || blocked) {
        walkingMonitor = nullptr;
        wander.Reset(oldYaw);
    }
    auto frame = physics.Prepare(window, dt, graphics.GetModelBottomInClient(), physicsSuspended, walkingMonitor);
    if (!frame.result.valid && walkingMonitor) {
        // A display can be disconnected during a walk. Resolve a fresh monitor.
        walkingMonitor = nullptr;
        frame = physics.Prepare(window, dt, graphics.GetModelBottomInClient(), physicsSuspended);
    }
    WanderInput input;
    input.enabled = enabled;
    input.blocked = blocked;
    input.physicsValid = frame.result.valid;
    input.grounded = frame.result.grounded;
    input.justLanded = frame.result.justLanded;
    input.speedMultiplier = std::clamp(settings.states[WalkStateIndex].speed * frame.clientHeight / 600.0, 0.001, 64.0);
    input.windowX = frame.windowRect.left;
    if (enabled && !blocked && frame.result.valid) {
        const auto bounds = graphics.GetWanderBoundsInClient();
        const double minimum = std::ceil(static_cast<double>(frame.workArea.left) - frame.clientOffset.x - bounds.left);
        const double maximum = std::floor(static_cast<double>(frame.workArea.right) - frame.clientOffset.x - bounds.right);
        if (bounds.valid && std::isfinite(minimum) && std::isfinite(maximum) &&
            minimum >= std::numeric_limits<std::int32_t>::min() && maximum <= std::numeric_limits<std::int32_t>::max() &&
            minimum < maximum) {
            input.minimumX = static_cast<std::int32_t>(minimum);
            input.maximumX = static_cast<std::int32_t>(maximum);
            if (frame.result.grounded) walkingMonitor = frame.monitor;
        } else input.blocked = true;
    }
    lastStep = wander.Update(dt, input);
    graphics.SetAutonomousFacing(lastStep.facingYawRadians);
    const auto result = physics.Apply(window, frame, lastStep.horizontalPixels, graphics.GetModelBottomInClient());
    if (!result.valid) {
        graphics.SetAutonomousFacing(oldYaw);
        Reset(oldYaw);
    }
    return result;
}
