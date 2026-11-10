#include "movement.hpp"
#include "vehicles.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void near(double actual, double expected, const char* message) {
    check(std::abs(actual - expected) < 1e-8, message);
}
template<class Action> void rejects(Action action, const char* message) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, message);
}

void checkVisibleClearance(const lc::GameplayConfig& config) {
    const lc::VehicleStream stream(config);
    const auto initial = stream.sample(0.0);
    for (std::size_t i = 0; i < initial.size(); ++i) {
        for (std::size_t j = i + 1; j < initial.size(); ++j) {
            const auto& a = initial[i];
            const auto& b = initial[j];
            if (a.lane != b.lane) continue;
            const auto enter = [&](const lc::Vehicle& v) {
                return (-config.visibleBehindDistance - v.length / 2 - v.forwardPosition) /
                       (v.speed - config.normalRunSpeed);
            };
            const auto leave = [&](const lc::Vehicle& v) {
                return (config.visibleAheadDistance + v.length / 2 - v.forwardPosition) /
                       (v.speed - config.normalRunSpeed);
            };
            const double start = std::max({0.0, enter(a), enter(b)});
            const double end = std::min(leave(a), leave(b));
            if (start > end) continue;
            const double startDelta = stream.sample(start)[i].forwardPosition - stream.sample(start)[j].forwardPosition;
            const double endDelta = stream.sample(end)[i].forwardPosition - stream.sample(end)[j].forwardPosition;
            const double direction = startDelta >= 0.0 ? 1.0 : -1.0;
            const double requiredDistance = (a.length + b.length) / 2 + config.vehicleMinimumGap;
            // Relative position is linear. Same signed separation at both ends
            // proves clearance for the whole common visibility interval, not just frames.
            check(direction * startDelta >= requiredDistance - 1e-8, "clearance at first shared visible instant");
            check(direction * endDelta >= requiredDistance - 1e-8, "clearance throughout shared visible interval");
        }
    }
}
}

int main() {
    try {
        const lc::GameplayConfig config;
        const lc::VehicleStream stream(config);
        const auto initial = stream.sample(0.0);
        const auto later = stream.sample(3.5);
        const auto repeat = lc::VehicleStream(config).sample(3.5);
        check(initial.size() == 8, "finite authored sequence");
        for (std::size_t i = 0; i < initial.size(); ++i) {
            check(initial[i].id == i && later[i].id == i, "stable vehicle id");
            check(later[i].lane != lc::Lane::Center, "center lane has no traffic");
            check(later[i].lane == initial[i].lane, "vehicles do not change lanes");
            near(later[i].forwardPosition - initial[i].forwardPosition,
                 initial[i].speed * 3.5, "forward speed matches configuration");
            near(repeat[i].forwardPosition, later[i].forwardPosition, "fresh run repeats positions");
            check(later[i].forwardPosition > initial[i].forwardPosition, "all traffic moves forward");
        }
        near(initial[0].length, 3.0, "bike length from brief");
        near(initial[1].length, 10.0, "bus length from brief");
        near(initial[2].length, 5.0, "taxi length from brief");
        near(initial[4].length, 15.0, "express length from brief");
        near(stream.sample(2.0)[0].forwardPosition, 24.0, "bike meets a 12-speed runner at two seconds");
        near(stream.sample(5.0)[1].forwardPosition, 60.0, "bus meets a 12-speed runner at five seconds");
        near(stream.sample(8.0)[2].forwardPosition, 96.0, "taxi meets a 12-speed runner at eight seconds");
        near(stream.sample(14.0)[4].forwardPosition, 168.0, "express meets runner at fourteen seconds");
        check(stream.sample(1000.0).size() == initial.size(), "no recycling or random spawning");

        lc::Movement player(config);
        player.requestTransfer(-1);
        player.update(2.0);
        near(player.forwardPosition(), stream.sample(2.0)[0].forwardPosition, "geometric overlap scenario");
        near(player.forwardSpeed(), 12.0, "overlap does not mount or accelerate in Phase 3");

        auto custom = config;
        custom.vehicleSpecs[0] = {24.0, 4.0};
        custom.vehicleSequence[0].initialForwardPosition = -50.0;
        const auto tuned = lc::VehicleStream(custom).sample(2.0);
        near(tuned[0].forwardPosition, -2.0, "custom speed and position applied");
        near(tuned[0].length, 4.0, "custom body length applied");
        // Sampling in any order cannot alter the sequence's state.
        (void)stream.sample(100.0);
        near(stream.sample(0.0)[0].forwardPosition, -16.0, "sampling does not mutate initial state");
        rejects([&] { (void)stream.sample(-1.0); }, "negative time rejected");
        rejects([&] { (void)stream.sample(std::numeric_limits<double>::quiet_NaN()); }, "NaN time rejected");
        custom = config;
        custom.vehicleSequence[0].lane = lc::Lane::Center;
        rejects([&] { lc::VehicleStream invalid(custom); }, "center-lane vehicle rejected");
        custom = config;
        custom.vehicleSpecs[0].speed = 0.0;
        rejects([&] { lc::VehicleStream invalid(custom); }, "nonpositive speed rejected");
        custom = config;
        custom.vehicleSpecs[0].length = -1.0;
        rejects([&] { lc::VehicleStream invalid(custom); }, "negative length rejected");
        custom = config;
        custom.vehicleSequence[0].type = static_cast<lc::VehicleType>(99);
        rejects([&] { lc::VehicleStream invalid(custom); }, "invalid type rejected before indexing");

        // Regression: original right-lane bike/taxi bodies overlapped visibly at 20.5s.
        const double reproTime = 20.5;
        const double oldBike = -144.0 + 20.0 * reproTime;
        const double oldTaxi = -660.0 + 45.0 * reproTime;
        check(std::abs(oldBike - oldTaxi) < (3.0 + 5.0) / 2, "original authored trajectories reproduce body overlap");
        check(oldBike - 1.5 - 12.0 * reproTime < config.visibleAheadDistance,
              "original overlap was inside the visible road, not merely offscreen");
        const auto corrected = stream.sample(reproTime);
        check(corrected[5].forwardPosition - corrected[6].forwardPosition >=
              (corrected[5].length + corrected[6].length) / 2 + config.vehicleMinimumGap,
              "regression: taxi stays behind bike while both visible");
        check(corrected[6].entryDelay > 0.0, "conflicting entry is delayed");
        near(corrected[6].speed, 45.0, "taxi keeps its configured speed");
        near(corrected[5].entryDelay, 0.0, "leading bike is not delayed");
        near(corrected[0].entryDelay, 0.0, "unrelated left-lane bike unchanged");
        check(stream.sample(100.0)[6].forwardPosition > stream.sample(100.0)[5].forwardPosition,
              "offscreen passing is allowed without a global speed cap");
        checkVisibleClearance(config);

        custom = config;
        for (auto& seed : custom.vehicleSequence) { seed.lane = lc::Lane::Left; seed.initialForwardPosition = -40.0; }
        checkVisibleClearance(custom); // Multiple vehicles competing for entry, mixed lengths/speeds.
        std::reverse(custom.vehicleSequence.begin(), custom.vehicleSequence.end());
        checkVisibleClearance(custom); // Sequence IDs are not assumed to be physical ordering.
        custom.visibleAheadDistance = 70.0;
        custom.visibleBehindDistance = 20.0;
        custom.vehicleMinimumGap = 2.5;
        checkVisibleClearance(custom); // A longer camera window requires more spacing.
        custom.vehicleMinimumGap = 0.0;
        checkVisibleClearance(custom); // Exact touching allowed by explicit zero-gap tuning, no penetration.
        custom = config;
        custom.vehicleMinimumGap = -1.0;
        rejects([&] { lc::VehicleStream invalid(custom); }, "negative gap rejected");
        custom = config;
        custom.visibleAheadDistance = std::numeric_limits<double>::infinity();
        rejects([&] { lc::VehicleStream invalid(custom); }, "invalid camera bounds rejected");
        custom = config;
        custom.vehicleSpecs[0].speed = config.normalRunSpeed;
        rejects([&] { lc::VehicleStream invalid(custom); }, "fixed-camera planner rejects non-overtaking traffic explicitly");
        for (const auto& vehicle : corrected) {
            if (vehicle.entryDelay > 0.0) std::cout << "Vehicle #" << vehicle.id + 1 << " entry delay: "
                                                  << vehicle.entryDelay << " s\n";
        }
        std::cout << "PASS: " << checks << " deterministic vehicle checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after check " << checks << ": " << error.what() << '\n';
        return 1;
    }
}
