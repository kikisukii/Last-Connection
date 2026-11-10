#include "time_attack.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace lc {
void validateRaceConfig(const GameplayConfig& config) {
    if (!std::isfinite(config.finishDistance) || config.finishDistance <= 0 ||
        !std::isfinite(config.deadline) || config.deadline <= 0)
        throw std::invalid_argument("Finish distance and deadline must be positive and finite.");
    double previous = 0;
    for (double limit : config.rankTimeLimits) {
        if (!std::isfinite(limit) || limit <= previous || limit >= config.deadline)
            throw std::invalid_argument("Rank times must increase strictly before deadline.");
        previous = limit;
    }
}
char rankForTime(double seconds, const GameplayConfig& config) {
    validateRaceConfig(config);
    if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("Invalid finish time.");
    constexpr char ranks[] = {'S', 'A', 'B', 'C'};
    for (std::size_t i = 0; i < config.rankTimeLimits.size(); ++i)
        if (seconds <= config.rankTimeLimits[i] + 1e-9) return ranks[i];
    return seconds <= config.deadline + 1e-9 ? 'D' : 'F';
}
std::string formatRaceTime(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 1e9) return "--:--.--";
    const auto centiseconds = static_cast<long long>(std::llround(seconds * 100));
    std::ostringstream out;
    out << std::setfill('0') << std::setw(2) << centiseconds / 6000 << ':'
        << std::setw(2) << (centiseconds / 100) % 60 << '.' << std::setw(2) << centiseconds % 100;
    return out.str();
}
std::string scoreRulesKey(const GameplayConfig& c) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17) << "LC_SIM_TIME_INTERSECTION_V03";
    // Excludes the historical sequence and each random run's individual seed.
    for (double value : {c.normalRunSpeed, c.normalDeceleration, c.recoveryAcceleration,
         c.missPenalty, c.minimumForwardSpeed, c.laneSpacing, c.lateralTransferDuration,
         c.fixedStepSeconds, c.maxFrameSeconds, c.visibleBehindDistance, c.visibleAheadDistance,
         c.vehicleMinimumGap, c.maxTransferRelativeSpeed, c.trafficEntryMargin,
         c.leftLaneProbability, c.firstVehicleEntryTime, c.minimumSpawnInterval,
         c.maximumSpawnInterval, c.finishDistance, c.deadline}) out << '|' << value;
    for (const auto& spec : c.vehicleSpecs) out << '|' << spec.speed << '|' << spec.length << '|' << spec.catchZoneLength
        << '|' << spec.maxBoardingRelativeSpeed << '|' << spec.rideServiceDistance();
    for (double value : c.laneFirstSpawnTimes) out << '|' << value;
    for (double value : c.laneMinimumSpawnIntervals) out << '|' << value;
    for (double value : c.laneMaximumSpawnIntervals) out << '|' << value;
    out << '|' << c.minimumSpawnTTC << '|' << c.encounterTimeMin << '|' << c.encounterTimeMax
        << '|' << c.encounterReviewInterval << '|' << c.serviceContinuationLeadTime
        << '|' << c.encounterVisibilityLeadTime << '|' << c.encounterPlannerEnabled << '|' << c.constrainedVehicleSelection;
    for (double weight : c.vehicleTypeWeights) out << '|' << weight;
    for (double limit : c.rankTimeLimits) out << '|' << limit;
    out << '|' << c.useFixedTrafficSeed;
    if (c.useFixedTrafficSeed) out << '|' << c.fixedTrafficSeed;
    return out.str();
}
std::string encodeBestTime(double seconds, const GameplayConfig& config) {
    if (!std::isfinite(seconds) || seconds <= 0) throw std::invalid_argument("Invalid best time.");
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << scoreRulesKey(config) << '\n' << std::setprecision(17) << seconds << '\n';
    return out.str();
}
std::optional<double> decodeBestTime(std::string_view data, const GameplayConfig& config) {
    if (data.size() > 4096) return {};
    std::istringstream in{std::string(data)};
    in.imbue(std::locale::classic());
    std::string key; double seconds = 0;
    if (!std::getline(in, key) || key != scoreRulesKey(config) || !(in >> seconds) ||
        !std::isfinite(seconds) || seconds <= 0) return {};
    in >> std::ws;
    if (!in.eof()) return {};
    return seconds;
}
TimeAttack::TimeAttack(GameplayConfig config, std::uint32_t seed)
    : config_(config), game_(config), traffic_(config, seed) { validateRaceConfig(config); }
bool TimeAttack::requestTransfer(int direction) {
    return !finished() && game_.requestTransfer(direction, traffic_.current());
}
void TimeAttack::update(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("Invalid race delta.");
    if (finished()) return;
    Game predicted = game_;
    predicted.update(seconds);
    double actualSeconds = seconds;
    if (predicted.player().forwardPosition() >= config_.finishDistance) {
        // Forward motion is strictly positive, even across catch/miss events.
        // Locate the FIRST crossing using the real Game integrator, avoiding a
        // linear interpolation that would be wrong during acceleration/catches.
        double low = 0, high = seconds;
        for (int i = 0; i < 60; ++i) {
            const double middle = low + (high - low) / 2;
            Game probe = game_;
            probe.update(middle);
            if (probe.player().forwardPosition() >= config_.finishDistance) high = middle;
            else low = middle;
        }
        actualSeconds = high;
        game_.update(actualSeconds);
        finishTime_ = game_.player().elapsedSeconds();
    } else game_ = std::move(predicted);
    traffic_.update(actualSeconds, game_);
}
void TimeAttack::restart(std::uint32_t seed) {
    game_ = Game(config_);
    traffic_ = LiveTraffic(config_, seed);
    finishTime_.reset();
}
double TimeAttack::remainingDistance() const { return std::max(0.0, config_.finishDistance - game_.player().forwardPosition()); }
double TimeAttack::progress() const { return std::clamp(game_.player().forwardPosition() / config_.finishDistance, 0.0, 1.0); }
}
