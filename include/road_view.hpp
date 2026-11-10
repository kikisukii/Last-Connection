#pragma once
#include "vehicles.hpp"

namespace lc {
struct VehicleScreenBounds {
    float top;
    float bottom;
};

// One projection for renderer and entry regression tests. A vehicle's world
// position is its center; its front bumper must cross the road's bottom first.
struct RoadView {
    static constexpr float top = 112.0f;
    static constexpr float bottom = 470.0f;
    float pixelsPerUnit;
    float playerY;

    explicit RoadView(const GameplayConfig& config)
        : pixelsPerUnit((bottom - top) / static_cast<float>(config.visibleAheadDistance + config.visibleBehindDistance)),
          playerY(top + static_cast<float>(config.visibleAheadDistance) * pixelsPerUnit) {}

    VehicleScreenBounds bounds(const Vehicle& vehicle, double playerPosition) const {
        const double relative = vehicle.forwardPosition - playerPosition;
        return {playerY - static_cast<float>(relative + vehicle.length / 2.0) * pixelsPerUnit,
                playerY - static_cast<float>(relative - vehicle.length / 2.0) * pixelsPerUnit};
    }
};
}
