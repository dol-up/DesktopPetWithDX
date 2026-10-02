#pragma once
#include <cstdint>
#include <random>

enum class WanderPhase { Waiting, Turning, Moving, Returning };
enum class WalkDirection { Left = -1, Right = 1 };

struct WanderSettings {
    double pixelsPerSecond = 60;
    double minimumWaitSeconds = 3;
    double maximumWaitSeconds = 8;
    double minimumMoveSeconds = 2;
    double maximumMoveSeconds = 5;
    double turnSeconds = 0.3;
    // Imported models can use different forward axes.
    double forwardYawRadians = 0;
};

bool operator==(const WanderSettings& a, const WanderSettings& b);

struct WanderInput {
    bool enabled = false;
    bool blocked = false;
    bool physicsValid = false;
    bool grounded = false;
    bool justLanded = false;
    double speedMultiplier = 1;
    std::int32_t windowX = 0;
    std::int32_t minimumX = 0;
    std::int32_t maximumX = 0;
};

struct WanderStep {
    std::int64_t horizontalPixels = 0;
    double facingYawRadians = 0;
    WanderPhase phase = WanderPhase::Waiting;
    WalkDirection direction = WalkDirection::Right;
};

// Owns only decisions and time; no HWND, renderer, animation or file access.
class WanderController {
public:
    explicit WanderController(std::uint32_t seed = std::random_device{}());
    WanderStep Update(double deltaSeconds, const WanderInput& input);
    void Reset(double currentYawRadians = 0);
    void SetSettings(const WanderSettings& value);
    static void ValidateSettings(const WanderSettings& value);
    const WanderSettings& GetSettings() const { return settings; }
    WanderPhase GetPhase() const { return phase; }
private:
    double RandomSeconds(double minimum, double maximum);
    void StartWaiting();
    void StartTurning(WalkDirection nextDirection);
    void StartRotation(double targetYaw, WanderPhase turnPhase);
    WanderStep Result(std::int64_t pixels = 0) const;
    std::mt19937 random;
    WanderSettings settings;
    WanderPhase phase = WanderPhase::Waiting;
    WalkDirection direction = WalkDirection::Right;
    bool active = false;
    double waitRemaining = 0;
    double moveRemaining = 0;
    double subpixel = 0;
    double yaw = 0;
    double turnStart = 0;
    double turnDelta = 0;
    double turnElapsed = 0;
};
