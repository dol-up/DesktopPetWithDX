#pragma once

struct PhysicsResult {
    bool valid = false;
    bool grounded = false;
    bool justLanded = false;
    float verticalVelocity = 0;
};

struct GravityStep {
    PhysicsResult result;
    int pixelMovement = 0;
};

// Pure gravity/contact calculation, independent of HWND and monitor APIs.
class GravitySimulation {
public:
    GravityStep Step(float distanceToGround, float deltaSeconds, bool suspended);
    void Reset();
private:
    float velocity = 0;
    float subpixel = 0;
    bool wasAirborne = false;
};
