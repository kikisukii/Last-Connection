#pragma once
#include "gameplay_config.hpp"

namespace lc {
class Movement {
public:
    explicit Movement(GameplayConfig config = {});
    // One edge-triggered input requests exactly one adjacent lane.
    bool requestTransfer(int direction);
    void returnToCenter(); // service end: retarget from actual lateral position
    void update(double seconds, bool relaxSpeed = false);
    void setForwardSpeed(double speed);

    const GameplayConfig& config() const { return config_; }
    double forwardPosition() const { return forwardPosition_; }
    double forwardSpeed() const { return forwardSpeed_; }
    double elapsedSeconds() const { return elapsedSeconds_; }
    Lane lane() const { return lane_; }
    Lane targetLane() const { return targetLane_; }
    bool transferring() const { return transferring_; }
    double transferRemainingSeconds() const { return transferring_ ? config_.lateralTransferDuration - transferElapsed_ : 0.0; }
    double transferProgress() const;
    double lateralPosition() const;
    double lateralVelocity() const;
    double lateralAcceleration() const;

private:
    GameplayConfig config_;
    double forwardSpeed_;
    double forwardPosition_ = 0.0;
    double elapsedSeconds_ = 0.0;
    double transferElapsed_ = 0.0;
    double transferStartX_ = 0.0;
    Lane lane_ = Lane::Center;
    Lane targetLane_ = Lane::Center;
    bool transferring_ = false;
};
const char* laneName(Lane lane);
}
