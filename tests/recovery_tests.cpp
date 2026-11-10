#include "forward_motion.hpp"
#include "game.hpp"
#include "live_traffic.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;
void require(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
bool near(double a, double b) { return std::abs(a - b) < 1e-8; }
void mount(lc::Game& game, lc::VehicleType type, lc::Lane lane) {
    const auto spec = game.player().config().vehicleSpecs[static_cast<std::size_t>(type)];
    static std::size_t nextId = 50; // each mount fixture represents a new connection
    const std::vector<lc::Vehicle> target{{nextId++, type, lane, game.player().forwardPosition(), spec.speed, spec.length, 0}};
    require(game.requestTransfer(static_cast<int>(lane), target), "mount transfer rejected");
    game.update(.25);
    require(game.attachedVehicle().has_value(), "fixture mount failed");
}
}
int main() {
    try {
        lc::GameplayConfig config;
        // Fixed historical trajectory fixture; V0.3 defaults covered separately.
        config.lateralTransferDuration = .25; config.normalDeceleration = 10;
        // Isolate existing recovery physics from the new boarding/service tuning.
        for (auto& spec : config.vehicleSpecs) { spec.maxBoardingRelativeSpeed = 30; spec.rideServiceDistance = 100000; }
        auto step = lc::advanceForward(42, 1, config);
        require(near(step.speed, 32) && near(step.distance, 37), "high-speed integration");
        step = lc::advanceForward(42, 4, config);
        require(step.speed == 12 && near(step.distance, 93), "deceleration overshot equilibrium or lost distance");
        step = lc::advanceForward(7, 2, config);
        require(step.speed == 12 && near(step.distance, 21.5), "low-speed recovery overshot equilibrium");
        double speed = 42, distance = 0;
        for (int i = 0; i < 480; ++i) { step = lc::advanceForward(speed, 1.0 / 120, config); speed = step.speed; distance += step.distance; }
        require(near(speed, 12) && near(distance, 93), "speed integration depends on timestep");

        lc::Game low(config);
        low.requestTransfer(-1, {}); low.update(.249);
        require(low.player().forwardSpeed() == 12, "penalty before transfer completion");
        low.update(.001);
        require(near(low.player().forwardSpeed(), 7) && near(low.player().forwardPosition(), 3), "12 -> 7 miss or forward position wrong");
        require(low.transferDebug().penaltyApplied == 5, "missing penalty debug");
        low.update(.5);
        require(near(low.player().forwardSpeed(), 9.5), "recovery should be gradual and penalty applied only once");
        low.update(.5);
        require(near(low.player().forwardSpeed(), 12) && near(low.player().forwardPosition(), 12.5), "recovery to normal");
        low.update(1);
        require(low.player().forwardSpeed() == 12, "settled speed drifted");

        lc::Game large(config), small(config);
        large.requestTransfer(-1, {}); small.requestTransfer(-1, {});
        large.update(2);
        for (int i = 0; i < 240; ++i) small.update(1.0 / 120);
        require(near(large.player().forwardPosition(), 21.5) && near(large.player().forwardPosition(), small.player().forwardPosition()), "large update must split at miss then recovery equilibrium");
        require(large.transferDebug().penaltyApplied == 5 && small.transferDebug().penaltyApplied == 5, "partition repeated miss penalty");

        lc::Game high(config);
        mount(high, lc::VehicleType::Bus, lc::Lane::Left);
        high.update(10);
        require(high.player().forwardSpeed() == 32, "riding must not decay");
        high.requestTransfer(1, {}); high.update(.25);
        require(near(high.player().forwardSpeed(), 29.5) && high.transferDebug().penaltyApplied == 0, "normal departure must decay without miss");
        mount(high, lc::VehicleType::Taxi, lc::Lane::Right);
        high.requestTransfer(-1, {}); high.update(.25); // 42.5
        high.requestTransfer(-1, {}); high.update(.25); // 40 -> 35
        require(near(high.transferDebug().speedBeforePenalty, 40) && near(high.player().forwardSpeed(), 35), "high-speed miss should preserve remaining momentum");
        high.requestTransfer(1, {}); high.update(.25); // 32.5, center
        mount(high, lc::VehicleType::Bus, lc::Lane::Right);
        require(high.player().forwardSpeed() == 32, "recovery onto slower bus failed");

        lc::Game repeated(config);
        for (int i = 0; i < 30; ++i) {
            repeated.requestTransfer(-1, {}); repeated.update(.25);
            require(repeated.player().forwardSpeed() >= config.minimumForwardSpeed && repeated.player().forwardSpeed() < 12, "repeated miss violates positive speed floor");
            repeated.requestTransfer(1, {}); repeated.update(.25);
        }
        const double beforeInvalid = repeated.player().forwardSpeed();
        require(!repeated.requestTransfer(0, {}) && repeated.player().forwardSpeed() == beforeInvalid, "invalid input caused penalty");

        // Curved trajectories can intersect where linear extrapolation misses.
        require(lc::predictCatch(0, 20, 1.9, 20, 2, .5, 30).intersects == false, "linear fixture");
        const auto curved = lc::predictRecoveringCatch(0, 20, -2, 20, 2, .5, config);
        require(curved.canCatch() && near(curved.enterTime, std::sqrt(.2)), "quadratic crossing not found");
        require(!lc::predictCatch(0, 20, -2, 20, 2, .5, 30).intersects, "curved case must differ from constant-speed prediction");
        const auto settled = lc::predictRecoveringCatch(0, 13, -2, 13, 2, 2, config);
        require(settled.canCatch() && near(settled.enterTime, 1.05), "intersection after equilibrium clamp lost");
        const auto tangent = lc::predictRecoveringCatch(0, 17, 2.25, 12, 2, 1, config);
        require(tangent.canCatch() && near(tangent.enterTime, .5), "tangent contact at equilibrium lost");
        const auto turning = lc::predictRecoveringCatch(0, 17, 1.45, 14, 2, .5, config);
        require(turning.canCatch() && near(turning.enterTime, .3), "tangent at relative-velocity reversal lost");
        auto limited = config; limited.maxTransferRelativeSpeed = 12;
        const auto speedGate = lc::predictRecoveringCatch(0, 7, 0, 20, 40, 1, limited);
        require(speedGate.canCatch() && near(speedGate.enterTime, .2) && near(speedGate.relativeSpeed, 12), "speed eligibility must use event velocity, not start velocity");

        // Real state path: miss to 7, return to center while recovering, catch Bike
        // between samples, then keep exactly the intersecting forward offset.
        lc::Game recovering(config);
        recovering.requestTransfer(-1, {}); recovering.update(.25);
        recovering.requestTransfer(1, {}); recovering.update(.25); // 8.25
        const double startPosition = recovering.player().forwardPosition();
        const double startTime = recovering.player().elapsedSeconds();
        std::vector<lc::Vehicle> bike{{75, lc::VehicleType::Bike, lc::Lane::Left, startPosition - 2, 20, 3, 0}};
        recovering.requestTransfer(-1, bike);
        const double hit = recovering.transferDebug().prediction.enterTime;
        require(hit > 0 && hit < .25, "recovering catch fixture has no mid-window hit");
        recovering.update(hit / 2);
        require(!recovering.attachedVehicle(), "recovering catch happened early");
        recovering.update(1 - hit / 2);
        require(recovering.attachedVehicle() == 75 && recovering.player().forwardSpeed() == 20, "recovering catch failed");
        const double targetPosition = bike[0].forwardPosition + 20 * (recovering.player().elapsedSeconds() - startTime);
        require(near(recovering.player().forwardPosition() - targetPosition, 1), "curved prediction and integration disagree / snap at catch");

        // Dense numerical oracle checks both acceleration pieces and speed gates.
        for (double initial : {1.0,7.0,11.8,12.0,12.2,20.0,45.0,60.0})
        for (double vehicleSpeed : {20.0,32.0,45.0,60.0}) for (double offset : {-8.0,-2.0,0.0,2.0,8.0}) {
            const auto prediction = lc::predictRecoveringCatch(0, initial, offset, vehicleSpeed, 2, .25, config);
            bool sampledHit = false;
            for (int i = 0; i <= 2500; ++i) {
                const double t = i * .0001;
                const auto m = lc::advanceForward(initial, t, config);
                if (std::abs(m.distance - offset - vehicleSpeed * t) <= 1 && std::abs(m.speed - vehicleSpeed) <= 30) sampledHit = true;
            }
            require(!sampledHit || prediction.canCatch(), "continuous solver missed a sampled valid intersection");
            if (prediction.canCatch()) {
                const auto m = lc::advanceForward(initial, prediction.enterTime, config);
                require(std::abs(m.distance - offset - vehicleSpeed * prediction.enterTime) <= 1 + 1e-8 && std::abs(m.speed - vehicleSpeed) <= 30 + 1e-8, "predicted catch is not valid on actual trajectory");
            }
        }
        auto invalid = config; invalid.recoveryAcceleration = -1;
        bool threw = false; try { lc::Game bad(invalid); } catch (const std::invalid_argument&) { threw = true; }
        require(threw, "negative recovery accepted");
        int catches = 0, misses = 0;
        for (unsigned seed = 0; seed < 8; ++seed) {
            auto crowded = config;
            crowded.laneMinimumSpawnIntervals.fill(.1); crowded.laneMaximumSpawnIntervals.fill(.3);
            lc::Game simulated(crowded); lc::LiveTraffic traffic(crowded, seed);
            for (int tick = 0; tick < 12000; ++tick) {
                if (!simulated.player().transferring() && tick % 43 == 0) {
                    const int lane = static_cast<int>(simulated.player().lane());
                    simulated.requestTransfer(lane == 0 ? ((tick / 43) % 2 ? -1 : 1) : -lane, traffic.current());
                }
                const bool transferring = simulated.player().transferring();
                const double oldPosition = simulated.player().forwardPosition();
                simulated.update(config.fixedStepSeconds);
                traffic.update(config.fixedStepSeconds, simulated.player().forwardPosition(), simulated.player().forwardSpeed());
                require(simulated.player().forwardPosition() > oldPosition, "recovery run stopped or reset");
                require(simulated.player().forwardSpeed() >= 1 && simulated.player().forwardSpeed() <= 60, "speed escaped bounds");
                if (transferring && !simulated.player().transferring()) {
                    if (simulated.attachedVehicle()) ++catches;
                    if (simulated.transferDebug().outcome == lc::TransferOutcome::Miss || simulated.transferDebug().outcome == lc::TransferOutcome::SpeedRejected) ++misses;
                }
                const auto vehicles = traffic.current();
                for (std::size_t i = 0; i < vehicles.size(); ++i) for (std::size_t j = i + 1; j < vehicles.size(); ++j) {
                    if (vehicles[i].lane == vehicles[j].lane)
                        require(std::abs(vehicles[i].forwardPosition - vehicles[j].forwardPosition) + 1e-8 >=
                            (vehicles[i].length + vehicles[j].length) / 2 + config.vehicleMinimumGap, "traffic overlap during real miss/recovery run");
                }
            }
        }
        require(catches > 0 && misses > 0, "integrated random runs did not exercise both outcomes");
        std::cout << "Integrated traffic: " << catches << " catches, " << misses << " misses across 8 seeds\n";
        std::cout << "PASS: " << checks << " recovery, miss timing, equilibrium, curved catch and momentum-chain checks\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
