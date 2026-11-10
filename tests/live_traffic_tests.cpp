#include "live_traffic.hpp"
#include "game.hpp"
#include "frame_clock.hpp"
#include "road_view.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
bool visible(const lc::Vehicle& v, double p, const lc::GameplayConfig& c) {
    return v.forwardPosition + v.length / 2 >= p - c.visibleBehindDistance &&
           v.forwardPosition - v.length / 2 <= p + c.visibleAheadDistance;
}
}
int main() {
    try {
        lc::GameplayConfig config;
        config.laneMinimumSpawnIntervals.fill(.1); config.laneMaximumSpawnIntervals.fill(.3);
        config.finishDistance = 1000000; // long-running traffic fixture, not live race tuning
        std::array<std::size_t, 4> counts{};
        for (unsigned seed = 0; seed < 32; ++seed) {
            lc::LiveTraffic traffic(config, seed), replay(config, seed);
            double position = 0;
            std::map<std::size_t, lc::Vehicle> previous;
            for (int tick = 0; tick < 24000; ++tick) {
                const double speed = std::array<double, 7>{12,20,45,60,32,20,12}[(tick / 500) % 7];
                position += speed * config.fixedStepSeconds;
                traffic.update(config.fixedStepSeconds, position, speed);
                replay.update(config.fixedStepSeconds, position, speed);
                const auto vehicles = traffic.current();
                require(vehicles.size() == replay.current().size(), "seed replay population mismatch");
                require(vehicles.size() < 32, "unbounded active traffic");
                std::map<std::size_t, lc::Vehicle> next;
                for (std::size_t i = 0; i < vehicles.size(); ++i) {
                    const auto& v = vehicles[i];
                    require(v.id == replay.current()[i].id && v.forwardPosition == replay.current()[i].forwardPosition, "seed replay mismatch");
                    if (previous.contains(v.id)) {
                        const auto old = previous.at(v.id);
                        require(std::abs(v.forwardPosition - old.forwardPosition - v.speed * config.fixedStepSeconds) < 1e-8, "vehicle teleported or changed speed");
                    } else require(!visible(v, position, config), "new vehicle appeared inside viewport");
                    next.emplace(v.id, v);
                    for (std::size_t j = i + 1; j < vehicles.size(); ++j) {
                        const auto& other = vehicles[j];
                        if (v.lane != other.lane) continue;
                        require(std::abs(v.forwardPosition - other.forwardPosition) + 1e-8 >= (v.length + other.length) / 2 + config.vehicleMinimumGap, "same-lane bodies overlap after camera speed change");
                    }
                }
                previous = std::move(next);
            }
            for (std::size_t i = 0; i < 4; ++i) counts[i] += traffic.typeCounts()[i];
        }
        std::size_t total = 0; for (auto count : counts) total += count;
        require(total>0,"no admitted vehicles");
        for (auto count : counts) require(count>0,"stress run missed a vehicle type");
        // Exercise the LIVE implementation at both camera edges and low render FPS.
        for (std::size_t type = 0; type < 4; ++type) for (double playerSpeed : {1.0, 12.0, 70.0})
        for (double renderDelta : {1.0/144, 1.0/60, 1.0/30, .1, .5}) {
            lc::GameplayConfig c;
            c.vehicleTypeWeights.fill(0); c.vehicleTypeWeights[type] = 1;
            lc::LiveTraffic traffic(c, 123); lc::FrameClock clock(c); lc::RoadView view(c);
            double position = 0; bool appeared = false;
            for (int frame = 0; frame < 1000 && !appeared; ++frame) {
                const int steps = clock.advance(renderDelta);
                for (int step = 0; step < steps; ++step) {
                    position += playerSpeed * c.fixedStepSeconds;
                    traffic.update(c.fixedStepSeconds, position, playerSpeed);
                }
                for (const auto& v : traffic.current()) if (visible(v, position, c)) {
                    const auto b = view.bounds(v, position);
                    const double fraction = (std::min(b.bottom, view.bottom) - std::max(b.top, view.top)) / (b.bottom - b.top);
                    const double maximumFraction = playerSpeed == 12 ? .12 : (playerSpeed == 1 ? .16 : .3);
                    require(fraction <= maximumFraction, "short car first appears deep inside view");
                    require(playerSpeed < v.speed ? b.bottom > view.bottom : b.top < view.top, "wrong entry edge");
                    appeared = true;
                }
            }
            require(appeared, "live traffic never entered");
        }
        // Catch a real generated bike and keep its ID/offset through 30s of riding.
        lc::GameplayConfig c; c.vehicleTypeWeights = {1,0,0,0};
        c.laneFirstSpawnTimes[1] = 1000;
        c.vehicleSpecs[0].rideServiceDistance = 1000; // isolate offset retention from service-end test
        lc::LiveTraffic traffic(c, 7); lc::Game game(c); bool attempted = false; double offset = 0;
        for (int i = 0; i < 4200; ++i) {
            if (!attempted) for (const auto& v : traffic.current()) {
                const double relative = v.forwardPosition - game.player().forwardPosition();
                if (relative > -2.5 && relative < -1) { game.requestTransfer(-1, traffic.current()); attempted = true; break; }
            }
            game.update(c.fixedStepSeconds);
            traffic.update(c.fixedStepSeconds, game.player().forwardPosition(), game.player().forwardSpeed());
            if (game.attachedVehicle()) {
                const auto found = std::find_if(traffic.current().begin(), traffic.current().end(), [&](const auto& v) { return v.id == *game.attachedVehicle(); });
                require(found != traffic.current().end(), "riding vehicle pruned using old camera speed");
                const double currentOffset = game.player().forwardPosition() - found->forwardPosition;
                if (offset == 0) offset = currentOffset;
                require(std::abs(currentOffset - offset) < 1e-8, "rider offset drifted");
            }
        }
        require(game.attachedVehicle().has_value(), "integration catch never occurred");
        std::cout << "PASS: 32 seeded camera-speed stress runs, all types exercised, both-edge entry, constant trajectories and sustained riding\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
