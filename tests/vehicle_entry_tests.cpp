#include "frame_clock.hpp"
#include "movement.hpp"
#include "road_view.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        // Do not put entry exactly on a frame boundary: that hides late first draws.
        constexpr double entryTime = 1.003;
        for (std::size_t type = 0; type < 4; ++type) {
            for (double renderDelta : {1.0 / 144.0, 1.0 / 60.0, 1.0 / 30.0, 0.1, 0.5}) {
                lc::GameplayConfig config;
                config.firstVehicleEntryTime = entryTime;
                config.vehicleTypeWeights.fill(0.0);
                config.vehicleTypeWeights[type] = 1.0;
                lc::Movement player(config);
                lc::FrameClock clock(config);
                lc::RandomVehicleStream stream(config, 123);
                const lc::RoadView view(config);
                bool appeared = false;
                for (int frame = 0; frame < 1000 && !appeared; ++frame) {
                    const int steps = clock.advance(renderDelta);
                    for (int step = 0; step < steps; ++step) player.update(config.fixedStepSeconds);
                    for (const auto& vehicle : stream.sample(player.elapsedSeconds())) {
                        const auto bounds = view.bounds(vehicle, player.forwardPosition());
                        if (bounds.top >= view.bottom) continue;
                        appeared = true;
                        const double visibleFraction = (view.bottom - bounds.top) / (bounds.bottom - bounds.top);
                        require(bounds.bottom > view.bottom, "first visible body must still extend below the bottom edge");
                        require(visibleFraction <= 0.12, "short car first appears mostly inside the road after a long frame");
                        require(player.forwardSpeed() == config.normalRunSpeed, "entry fix must not change player speed");
                        require(vehicle.speed == config.vehicleSpecs[type].speed, "entry fix must not slow just this vehicle");
                        const double expectedFront = -config.visibleBehindDistance +
                            (vehicle.speed - config.normalRunSpeed) * (player.elapsedSeconds() - entryTime);
                        const double actualFront = vehicle.forwardPosition + vehicle.length / 2 - player.forwardPosition();
                        require(std::abs(expectedFront - actualFront) < 1e-8, "vehicle teleported away from its planned trajectory");
                    }
                }
                require(appeared, "vehicle never entered the viewport");
            }
        }
        // Two runs receiving the same accepted frame time keep both clocks aligned.
        lc::GameplayConfig config;
        lc::FrameClock stalled(config), regular(config);
        for (int i = 0; i < 1000; ++i) {
            require(stalled.advance(0.5) == regular.advance(1.0 / 60.0), "stall budget changed simulation step sequence");
        }
        std::cout << "PASS: all four body lengths enter from the bottom across 144/60/30/10 FPS and 500ms stalls; shared clock and velocities preserved\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
