#pragma once
#include "gameplay_config.hpp"

namespace lc {
struct ForwardStep { double distance; double speed; };
void validateSpeedConfig(const GameplayConfig& config);
double forwardAcceleration(double speed, const GameplayConfig& config);
// Exact integral of constant acceleration until equilibrium, then constant speed.
// Shared by actual movement and predictive catch detection.
ForwardStep advanceForward(double speed, double seconds, const GameplayConfig& config);
}
