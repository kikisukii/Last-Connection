#include "movement.hpp"
#include "forward_motion.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace lc {
namespace {
bool positiveFinite(double value) { return std::isfinite(value) && value > 0.0; }
}
Movement::Movement(GameplayConfig config) : config_(config), forwardSpeed_(config.normalRunSpeed) {
    validateSpeedConfig(config);
    if (!positiveFinite(config.normalRunSpeed) || !positiveFinite(config.laneSpacing) ||
        !positiveFinite(config.lateralTransferDuration) || !positiveFinite(config.fixedStepSeconds) ||
        !positiveFinite(config.maxFrameSeconds) || config.maxFrameSeconds < config.fixedStepSeconds) {
        throw std::invalid_argument("Movement configuration must contain positive finite values; maxFrame >= fixedStep.");
    }
}

void Movement::setForwardSpeed(double speed) {
    if (!positiveFinite(speed)) throw std::invalid_argument("Forward speed must be positive and finite.");
    forwardSpeed_ = speed;
}

bool Movement::requestTransfer(int direction) {
    if (transferring_ || (direction != -1 && direction != 1)) return false;
    const int next = static_cast<int>(lane_) + direction;
    if (next < static_cast<int>(Lane::Left) || next > static_cast<int>(Lane::Right)) return false;
    targetLane_ = static_cast<Lane>(next);
    transferStartX_ = lateralPosition();
    transferElapsed_ = 0.0;
    transferring_ = true;
    return true;
}

void Movement::returnToCenter() {
    transferStartX_ = lateralPosition();
    targetLane_ = Lane::Center;
    transferElapsed_ = 0;
    transferring_ = transferStartX_ != 0;
    if (!transferring_) lane_ = Lane::Center;
}

void Movement::update(double seconds, bool relaxSpeed) {
    if (!std::isfinite(seconds) || seconds < 0.0) throw std::invalid_argument("Invalid simulation delta.");
    elapsedSeconds_ += seconds;
    const auto motion = relaxSpeed ? advanceForward(forwardSpeed_, seconds, config_)
                                  : ForwardStep{forwardSpeed_ * seconds, forwardSpeed_};
    forwardPosition_ += motion.distance;
    forwardSpeed_ = motion.speed;
    if (!transferring_) return;
    transferElapsed_ = std::min(config_.lateralTransferDuration, transferElapsed_ + seconds);
    // Repeated 1/120 additions can land a few ULPs below 0.25. Do not make
    // a transfer last an extra physics tick solely due to binary rounding.
    const double timeTolerance = 8.0 * std::numeric_limits<double>::epsilon() *
                                 std::max(1.0, config_.lateralTransferDuration);
    if (config_.lateralTransferDuration - transferElapsed_ <= timeTolerance) {
        transferElapsed_ = config_.lateralTransferDuration;
        lane_ = targetLane_;
        transferring_ = false;
    }
}

double Movement::transferProgress() const {
    return std::clamp(transferElapsed_ / config_.lateralTransferDuration, 0.0, 1.0);
}

double Movement::lateralPosition() const {
    if (!transferring_) return static_cast<int>(lane_) * config_.laneSpacing;
    const double start = transferStartX_;
    const double u = transferProgress();
    const double distance = static_cast<int>(targetLane_) * config_.laneSpacing - transferStartX_;
    // An analytic smoothstep gives zero start/end velocity and a known duration.
    // Unlike teleportation or frame-based lerp, timing does not depend on frame rate.
    return start + distance * u * u * (3.0 - 2.0 * u);
}

double Movement::lateralVelocity() const {
    if (!transferring_) return 0.0;
    const double u = transferProgress();
    const double distance = static_cast<int>(targetLane_) * config_.laneSpacing - transferStartX_;
    return distance * 6.0 * u * (1.0 - u) / config_.lateralTransferDuration;
}

double Movement::lateralAcceleration() const {
    if (!transferring_) return 0.0;
    const double distance = static_cast<int>(targetLane_) * config_.laneSpacing - transferStartX_;
    return distance * (6.0 - 12.0 * transferProgress()) /
           (config_.lateralTransferDuration * config_.lateralTransferDuration);
}

const char* laneName(Lane lane) {
    switch (lane) {
    case Lane::Left: return "LEFT";
    case Lane::Center: return "CENTER";
    case Lane::Right: return "RIGHT";
    }
    return "UNKNOWN";
}
}
