#include "forward_motion.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lc {
void validateSpeedConfig(const GameplayConfig& config) {
    if (!std::isfinite(config.normalRunSpeed) || config.normalRunSpeed <= 0 ||
        !std::isfinite(config.normalDeceleration) || config.normalDeceleration < 0 ||
        !std::isfinite(config.recoveryAcceleration) || config.recoveryAcceleration < 0 ||
        !std::isfinite(config.missPenalty) || config.missPenalty < 0 ||
        !std::isfinite(config.minimumForwardSpeed) || config.minimumForwardSpeed <= 0 ||
        config.minimumForwardSpeed > config.normalRunSpeed)
        throw std::invalid_argument("Invalid speed/recovery configuration.");
}
double forwardAcceleration(double speed, const GameplayConfig& config) {
    if (speed > config.normalRunSpeed) return -config.normalDeceleration;
    if (speed < config.normalRunSpeed) return config.recoveryAcceleration;
    return 0;
}
ForwardStep advanceForward(double speed, double seconds, const GameplayConfig& config) {
    validateSpeedConfig(config);
    if (!std::isfinite(speed) || speed <= 0 || !std::isfinite(seconds) || seconds < 0)
        throw std::invalid_argument("Invalid forward motion sample.");
    const double acceleration = forwardAcceleration(speed, config);
    if (acceleration == 0) return {speed * seconds, speed};
    const double settleTime = (config.normalRunSpeed - speed) / acceleration;
    const double changingTime = std::min(seconds, settleTime);
    const double distance = speed * changingTime + .5 * acceleration * changingTime * changingTime +
                            config.normalRunSpeed * (seconds - changingTime);
    return {distance, seconds >= settleTime ? config.normalRunSpeed : speed + acceleration * seconds};
}
}
