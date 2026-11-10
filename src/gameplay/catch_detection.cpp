#include "catch_detection.hpp"
#include "forward_motion.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <optional>
#include <vector>

namespace lc {
CatchWindow predictCatch(double playerPosition, double playerSpeed,
                         double vehiclePosition, double vehicleSpeed,
                         double catchZoneLength, double duration, double maxRelativeSpeed) {
    if (!std::isfinite(playerPosition) || !std::isfinite(playerSpeed) ||
        !std::isfinite(vehiclePosition) || !std::isfinite(vehicleSpeed) ||
        !std::isfinite(catchZoneLength) || catchZoneLength <= 0.0 ||
        !std::isfinite(duration) || duration < 0.0 ||
        !std::isfinite(maxRelativeSpeed) || maxRelativeSpeed < 0.0) {
        throw std::invalid_argument("Invalid catch trajectory/window parameters.");
    }
    CatchWindow result;
    result.relativeSpeed = vehicleSpeed - playerSpeed;
    result.speedAllowed = std::abs(result.relativeSpeed) <= maxRelativeSpeed;
    const double relativePosition = playerPosition - vehiclePosition;
    const double relativeVelocity = playerSpeed - vehicleSpeed;
    const double halfZone = catchZoneLength / 2.0;
    if (relativeVelocity == 0.0) {
        result.intersects = std::abs(relativePosition) <= halfZone;
        result.exitTime = result.intersects ? duration : 0.0;
        return result;
    }
    const double first = (-halfZone - relativePosition) / relativeVelocity;
    const double second = (halfZone - relativePosition) / relativeVelocity;
    const double enter = std::max(0.0, std::min(first, second));
    const double exit = std::min(duration, std::max(first, second));
    result.intersects = enter <= exit;
    if (result.intersects) {
        result.enterTime = enter;
        result.exitTime = exit;
    }
    return result;
}

CatchWindow predictBoardingCatch(double playerPosition, double playerSpeed,
                                double vehiclePosition, const VehicleSpec& spec,
                                double duration, const GameplayConfig& config) {
    const double endSpeed = advanceForward(playerSpeed, duration, config).speed;
    const double greatestDifference = std::max(std::abs(spec.speed - playerSpeed), std::abs(spec.speed - endSpeed));
    auto geometryConfig = config;
    geometryConfig.maxTransferRelativeSpeed = greatestDifference + 1;
    auto result = predictRecoveringCatch(playerPosition, playerSpeed, vehiclePosition, spec.speed,
                                         spec.catchZoneLength, duration, geometryConfig);
    // Geometry spans the entire transfer. Only FIRST geometric contact gates
    // boarding; never wait inside an overlapping zone for a later speed pass.
    // The rider inherits vehicle speed immediately after this event.
    result.relativeSpeed = spec.speed - advanceForward(playerSpeed, result.enterTime, config).speed;
    result.speedAllowed = result.intersects &&
        std::abs(result.relativeSpeed) <= spec.maxBoardingRelativeSpeed + 1e-10;
    return result;
}

namespace {
using Interval = std::pair<double, double>;
void recordFirst(std::optional<Interval>& first, double begin, double end) {
    if (!first) first = Interval{begin, end};
    else if (begin <= first->second) first->second = std::max(first->second, end);
}
void addRoots(std::vector<double>& cuts, double a, double b, double c, double duration) {
    const auto add = [&](double t) { if (std::isfinite(t) && t > 0 && t < duration) cuts.push_back(t); };
    if (a == 0) { if (b != 0) add(-c / b); return; }
    const double discriminant = b * b - 4 * a * c;
    if (discriminant < 0) return;
    // Stable quadratic roots avoid subtracting nearly equal values.
    const double q = -.5 * (b + std::copysign(std::sqrt(discriminant), b));
    if (q == 0) add(-b / (2 * a));
    else { add(q / a); add(c / q); }
}
}

CatchWindow predictRecoveringCatch(double playerPosition, double playerSpeed,
                                  double vehiclePosition, double vehicleSpeed,
                                  double catchZoneLength, double duration, const GameplayConfig& config) {
    // Reuse the linear function's input validation and exact zero-acceleration path.
    const auto linear = predictCatch(playerPosition, playerSpeed, vehiclePosition, vehicleSpeed,
                                     catchZoneLength, duration, config.maxTransferRelativeSpeed);
    validateSpeedConfig(config);
    const double acceleration = forwardAcceleration(playerSpeed, config);
    if (acceleration == 0) return linear;
    const double settleTime = (config.normalRunSpeed - playerSpeed) / acceleration;
    std::vector<double> segments{0, duration};
    if (settleTime > 0 && settleTime < duration) segments.insert(segments.begin() + 1, settleTime);
    std::optional<Interval> geometric, eligible;
    const double half = catchZoneLength / 2;
    for (std::size_t piece = 0; piece + 1 < segments.size(); ++piece) {
        const double start = segments[piece];
        const double length = segments[piece + 1] - start;
        const auto motion = advanceForward(playerSpeed, start, config);
        const double a = start < settleTime ? acceleration : 0;
        const double r = (playerPosition - vehiclePosition) + motion.distance - vehicleSpeed * start;
        const double u = motion.speed - vehicleSpeed;
        std::vector<double> cuts{0, length};
        addRoots(cuts, .5 * a, u, r - half, length);
        addRoots(cuts, .5 * a, u, r + half, length);
        if (a != 0) {
            for (double boundary : {-config.maxTransferRelativeSpeed, config.maxTransferRelativeSpeed}) {
                const double t = (boundary - u) / a;
                if (t > 0 && t < length) cuts.push_back(t);
            }
        }
        std::sort(cuts.begin(), cuts.end());
        cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
        const auto inspect = [&](double sample, double begin, double end) {
            // Numerical tolerance only; no gameplay widening of the catch zone.
            constexpr double tolerance = 1e-10;
            const bool inside = std::abs(r + u * sample + .5 * a * sample * sample) <= half + tolerance;
            if (!inside) return;
            recordFirst(geometric, start + begin, start + end);
            if (std::abs(u + a * sample) <= config.maxTransferRelativeSpeed + tolerance)
                recordFirst(eligible, start + begin, start + end);
        };
        // Geometry and speed eligibility are constant in each open interval
        // between roots. Check endpoints separately to retain tangent contacts.
        for (std::size_t i = 0; i < cuts.size(); ++i) {
            inspect(cuts[i], cuts[i], cuts[i]);
            if (i + 1 < cuts.size()) inspect((cuts[i] + cuts[i + 1]) / 2, cuts[i], cuts[i + 1]);
        }
    }
    CatchWindow result;
    result.intersects = geometric.has_value();
    result.speedAllowed = eligible.has_value();
    const auto selected = eligible ? eligible : geometric;
    if (selected) { result.enterTime = selected->first; result.exitTime = selected->second; }
    result.relativeSpeed = vehicleSpeed - advanceForward(playerSpeed, result.enterTime, config).speed;
    return result;
}
}
