#include "GravitySimulation.hpp"
#include <algorithm>
#include <cmath>

void GravitySimulation::Reset() {
    velocity = subpixel = 0;
    wasAirborne = false;
}

GravityStep GravitySimulation::Step(float distance, float dt, bool suspended) {
    GravityStep step;
    if (!std::isfinite(dt) || dt <= 0 || !std::isfinite(distance)) return step;
    step.result.valid = true;
    if (suspended) { Reset(); return step; }

    // Window coordinates are integral; the projected model bottom is fractional.
    // A gap smaller than one pixel is contact, not a new fall on every frame.
    if (distance <= 1.0f) {
        step.pixelMovement = distance < 0 ? static_cast<int>(std::floor(distance)) : 0;
        step.result.grounded = true;
        step.result.justLanded = wasAirborne;
        Reset();
        return step;
    }

    wasAirborne = true;
    velocity = std::min(velocity + 1800.0f * dt, 2400.0f);
    const float fallDistance = std::min(velocity * dt + subpixel, distance);
    step.pixelMovement = static_cast<int>(std::floor(fallDistance));
    subpixel = fallDistance - step.pixelMovement;
    // Report contact from the applied integer movement, not the unrounded intent.
    if (distance - step.pixelMovement <= 1.0f) {
        step.result.grounded = true;
        step.result.justLanded = true;
        Reset();
    }
    step.result.verticalVelocity = velocity;
    return step;
}
