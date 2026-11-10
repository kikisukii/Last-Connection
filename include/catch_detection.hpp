#pragma once
#include "gameplay_config.hpp"

namespace lc {
struct CatchWindow {
    bool intersects = false;
    bool speedAllowed = false;
    double enterTime = 0.0; // seconds after the start of the supplied window
    double exitTime = 0.0;
    double relativeSpeed = 0.0; // vehicle minus player
    bool canCatch() const { return intersects && speedAllowed; }
};

// Continuous 1D intersection: point player against a moving centered catch zone.
// It deliberately does not use the full body or only the transfer's end frame.
CatchWindow predictCatch(double playerPosition, double playerSpeed,
                         double vehiclePosition, double vehicleSpeed,
                         double catchZoneLength, double duration, double maxRelativeSpeed);
// Phase 5: acceleration/deceleration toward equilibrium, speed limit evaluated
// at the candidate intersection, including a limit crossed during overlap.
CatchWindow predictRecoveringCatch(double playerPosition, double playerSpeed,
                                  double vehiclePosition, double vehicleSpeed,
                                  double catchZoneLength, double duration, const GameplayConfig& config);
// Live V0.3: first geometric intersection only. Speed is evaluated before
// attachment at that instant; a later speed pass inside the zone cannot rescue it.
CatchWindow predictBoardingCatch(double playerPosition, double playerSpeed,
                                double vehiclePosition, const VehicleSpec& spec,
                                double duration, const GameplayConfig& config);
}
