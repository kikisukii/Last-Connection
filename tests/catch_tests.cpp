#include "game.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;
void require(bool condition, const char* message) { ++checks; if (!condition) throw std::runtime_error(message); }
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
lc::Vehicle bike(double position, std::size_t id = 0) { return {id, lc::VehicleType::Bike, lc::Lane::Left, position, 20, 3, 0}; }
}
int main() {
    try {
        // Historical catch/motion contract; the new per-type ladder and finite
        // service contract are exercised separately in traffic_service_tests.
        lc::GameplayConfig baseline;
        // Fixed historical trajectory fixture; V0.3 defaults covered separately.
        baseline.lateralTransferDuration = .25; baseline.normalDeceleration = 10;
        for (auto& spec : baseline.vehicleSpecs) { spec.maxBoardingRelativeSpeed = 30; spec.rideServiceDuration = 100000 / spec.speed; }
        const auto crossing = lc::predictCatch(0, 12, -2, 32, 1, .25, 30);
        require(crossing.canCatch() && near(crossing.enterTime, .075) && near(crossing.exitTime, .125), "whole-window intersection lost between endpoints");
        require(lc::predictCatch(0, 12, 1, 12, 2, .25, 0).canCatch(), "equal-speed boundary should catch");
        require(!lc::predictCatch(0, 12, 1.01, 12, 2, .25, 0).intersects, "equal-speed outside should miss");
        require(lc::predictCatch(0, 12, 0, 12, 2, 0, 0).canCatch(), "zero-duration overlap");
        require(!lc::predictCatch(0, 12, 2, 20, 2, .25, 30).intersects, "body-only overlap must miss zone");
        require(lc::predictCatch(0, 12, -3, 20, 2, .25, 8).canCatch(), "inclusive end and speed threshold");
        require(!lc::predictCatch(0, 12, -3, 20, 2, .25, 7.99).canCatch(), "relative speed threshold not enforced");
        require(lc::predictCatch(0, 45, 3, 20, 2, .25, 30).canCatch(), "overtaking slower vehicle should catch");
        for (double invalid : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
            bool threw = false;
            try { (void)lc::predictCatch(0, 12, 0, 20, 2, invalid, 30); } catch (const std::invalid_argument&) { threw = true; }
            require(threw, "invalid interval accepted");
        }
        const std::vector<lc::Vehicle> targets{bike(-2)}; // hit at .125; complete lateral at .25
        lc::Game game(baseline);
        require(game.requestTransfer(-1, targets), "transfer rejected");
        require(!game.requestTransfer(1, targets), "mid-transfer input accepted");
        game.update(.1);
        require(!game.attachedVehicle() && near(game.player().forwardPosition(), 1.2), "caught early");
        game.update(.05);
        require(game.attachedVehicle() == 0 && game.player().transferring(), "must follow before lateral completion");
        require(near(game.player().forwardPosition(), 2) && game.player().forwardSpeed() == 20, "intersection must split integration without snapping");
        game.update(.1);
        require(game.transferDebug().outcome == lc::TransferOutcome::Success && game.player().lane() == lc::Lane::Left, "riding state at completion");
        require(near(game.player().forwardPosition(), 4), "incorrect end position");
        game.update(2);
        require(near(game.player().forwardPosition() - (-2 + 20 * 2.25), 1), "rider drifted from catch zone");
        require(!game.requestTransfer(-1, targets) && game.attachedVehicle() == 0, "invalid outward input released rider");
        require(game.requestTransfer(1, targets) && !game.attachedVehicle(), "leaving should release immediately");
        game.update(.25);
        require(near(game.player().forwardSpeed(), 17.5) && game.transferDebug().outcome == lc::TransferOutcome::Center, "departure should decay without penalty");
        std::size_t nextConnection=9;
        for (const auto type : {lc::VehicleType::Taxi, lc::VehicleType::Bus, lc::VehicleType::Taxi, lc::VehicleType::Express}) {
            const auto spec = game.player().config().vehicleSpecs[static_cast<std::size_t>(type)];
            const int direction = type == lc::VehicleType::Bus ? -1 : 1;
            const auto lane = direction < 0 ? lc::Lane::Left : lc::Lane::Right;
            const double position = game.player().forwardPosition();
            game.requestTransfer(direction, std::vector<lc::Vehicle>{{nextConnection, type, lane, position, spec.speed, spec.length, 0}});
            game.update(.25);
            require(game.attachedVehicle() == nextConnection && game.player().forwardSpeed() == spec.speed, "momentum chain or slower vehicle catch failed");
            game.requestTransfer(-direction, {}); game.update(.25);
            require(game.player().lane() == lc::Lane::Center && near(game.player().forwardSpeed(), spec.speed - 2.5), "chain departure decay incorrect");
            ++nextConnection;
        }
        lc::Game single(baseline), partitioned(baseline);
        single.requestTransfer(-1, targets); partitioned.requestTransfer(-1, targets);
        single.update(1);
        for (int i = 0; i < 120; ++i) partitioned.update(1.0 / 120);
        require(near(single.player().forwardPosition(), partitioned.player().forwardPosition()) && single.attachedVehicle() == partitioned.attachedVehicle(), "frame partition changed catch");
        lc::Game endpoint(baseline);
        endpoint.requestTransfer(-1, std::vector<lc::Vehicle>{bike(-3)});
        for (int i = 0; i < 30; ++i) endpoint.update(1.0 / 120);
        require(endpoint.transferDebug().outcome == lc::TransferOutcome::Success, "endpoint catch lost to floating-point completion");
        lc::Game earliest(baseline);
        earliest.requestTransfer(-1, std::vector<lc::Vehicle>{bike(-2.5, 8), bike(-2, 3), bike(-2, 1)});
        earliest.update(.25);
        require(earliest.attachedVehicle() == 1, "earliest-time/id selection unstable");
        lc::Game miss(baseline);
        auto wrongLane = bike(0); wrongLane.lane = lc::Lane::Right;
        miss.requestTransfer(-1, std::vector<lc::Vehicle>{wrongLane}); miss.update(.25);
        require(miss.transferDebug().outcome == lc::TransferOutcome::Miss && near(miss.player().forwardPosition(), 3) && miss.player().forwardSpeed() == 7, "miss should apply one penalty without resetting position");
        lc::Game rejected(baseline);
        rejected.requestTransfer(-1, std::vector<lc::Vehicle>{{2, lc::VehicleType::Express, lc::Lane::Left, -6, 60, 15, 0}});
        rejected.update(.25);
        require(rejected.transferDebug().outcome == lc::TransferOutcome::SpeedRejected && !rejected.attachedVehicle(), "unsafe express catch accepted");
        lc::GameplayConfig config; config.vehicleSpecs[0].catchZoneLength = 4;
        bool threw = false;
        try { lc::Game invalid(config); } catch (const std::invalid_argument&) { threw = true; }
        require(threw, "zone longer than body accepted");
        std::cout << "PASS: " << checks << " continuous catch, early-follow, riding, departure and boundary checks\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
