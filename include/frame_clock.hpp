#pragma once
#include "gameplay_config.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lc {
// All moving objects use the same accepted frame time. Dropped stall time must
// never be applied to a vehicle later as a separate catch-up teleport.
class FrameClock {
public:
    explicit FrameClock(const GameplayConfig& config)
        : step_(config.fixedStepSeconds), maximumFrame_(config.maxFrameSeconds) {
        if (!std::isfinite(step_) || step_ <= 0.0 || !std::isfinite(maximumFrame_) || maximumFrame_ < step_) {
            throw std::invalid_argument("Invalid frame clock configuration.");
        }
    }
    int advance(double frameSeconds) {
        if (!std::isfinite(frameSeconds) || frameSeconds < 0.0) throw std::invalid_argument("Invalid frame time.");
        accumulator_ += std::min(frameSeconds, maximumFrame_);
        const int steps = static_cast<int>(accumulator_ / step_);
        accumulator_ -= steps * step_;
        return steps;
    }
private:
    double step_;
    double maximumFrame_;
    double accumulator_ = 0.0;
};
}
