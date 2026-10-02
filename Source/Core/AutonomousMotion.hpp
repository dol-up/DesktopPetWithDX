#pragma once
#include "WanderController.hpp"
#include "Physics/WindowPhysics.hpp"

class Graphics;

// Coordinates the pure planner, projection and one Win32 position commit.
class AutonomousMotion {
public:
    explicit AutonomousMotion(std::uint32_t seed = std::random_device{}()) : wander(seed) {}
    void Reset(double currentYawRadians);
    PhysicsResult Update(HWND window, float deltaSeconds, Graphics& graphics,
        bool physicsSuspended, bool interactionBlocked = false);
    bool IsWalking() const { return lastStep.phase == WanderPhase::Moving; }
    const WanderStep& GetLastStep() const { return lastStep; }
private:
    bool enabled = false;
    std::size_t walkingClip = static_cast<std::size_t>(-1);
    HMONITOR walkingMonitor = nullptr;
    WindowPhysics physics;
    WanderController wander;
    WanderStep lastStep;
};
