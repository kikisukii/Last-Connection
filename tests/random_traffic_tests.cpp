#include "vehicles.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class Action> void rejects(Action action) {
    bool rejected = false;
    try { action(); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "invalid random traffic configuration/time was accepted");
}
bool sameSnapshot(const std::vector<lc::Vehicle>& a, const std::vector<lc::Vehicle>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id || a[i].type != b[i].type || a[i].lane != b[i].lane ||
            std::abs(a[i].forwardPosition - b[i].forwardPosition) > 1e-8 ||
            a[i].speed != b[i].speed || a[i].length != b[i].length || a[i].entryDelay != b[i].entryDelay) return false;
    }
    return true;
}
void verifySharedVisibleIntervals(const lc::GameplayConfig& config, const std::vector<lc::Vehicle>& cars, double time) {
    for (std::size_t i = 0; i < cars.size(); ++i) {
        const auto& a = cars[i];
        require(a.lane == lc::Lane::Left || a.lane == lc::Lane::Right, "traffic entered center lane");
        require(a.speed == config.vehicleSpecs[static_cast<std::size_t>(a.type)].speed, "admission changed speed");
        for (std::size_t j = i + 1; j < cars.size(); ++j) {
            const auto& b = cars[j];
            if (a.lane != b.lane) continue;
            const double initialA = a.forwardPosition - a.speed * time;
            const double initialB = b.forwardPosition - b.speed * time;
            const double enterA = (-config.visibleBehindDistance - a.length / 2 - initialA) / (a.speed - config.normalRunSpeed);
            const double enterB = (-config.visibleBehindDistance - b.length / 2 - initialB) / (b.speed - config.normalRunSpeed);
            const double exitA = (config.visibleAheadDistance + a.length / 2 - initialA) / (a.speed - config.normalRunSpeed);
            const double exitB = (config.visibleAheadDistance + b.length / 2 - initialB) / (b.speed - config.normalRunSpeed);
            const double start = std::max(enterA, enterB);
            const double end = std::min(exitA, exitB);
            if (start > end) continue;
            const double deltaStart = initialA - initialB + (a.speed - b.speed) * start;
            const double deltaEnd = initialA - initialB + (a.speed - b.speed) * end;
            const double sign = deltaStart >= 0 ? 1.0 : -1.0;
            const double required = (a.length + b.length) / 2 + config.vehicleMinimumGap;
            require(sign * deltaStart >= required - 1e-7 && sign * deltaEnd >= required - 1e-7,
                    "random traffic violates continuous visible clearance");
        }
    }
}
}

int main() {
    try {
        lc::GameplayConfig config;
        lc::RandomVehicleStream empty(config, 123);
        require(empty.sample(0.0).empty(), "startup should safely contain no vehicles");
        require(empty.sample(0.99).empty(), "first spawn occurred early");
        require(empty.sample(1.0).size() == 1, "first configured entry missing");

        lc::RandomVehicleStream direct(config, 123), stepped(config, 123), other(config, 456);
        for (int i = 0; i <= 400; ++i) (void)stepped.sample(i * 0.25);
        require(sameSnapshot(direct.sample(100.0), stepped.sample(100.0)), "frame partitions changed seeded replay");
        require(direct.typeCounts() == stepped.typeCounts(), "frame partitions changed random choices");
        require(!sameSnapshot(direct.sample(100.0), other.sample(100.0)), "different seeds did not change stream");
        require(direct.generatedCount() > 8, "stream stopped at the old eight-vehicle limit");
        rejects([&] { (void)direct.sample(99.0); });
        rejects([&] { (void)direct.sample(std::numeric_limits<double>::quiet_NaN()); });

        // Fixed seeds make this distribution check reproducible, not probabilistically flaky.
        lc::RandomVehicleStream statistics(config, 20261109);
        const auto end = statistics.sample(100000.0);
        const double total = static_cast<double>(statistics.generatedCount());
        require(total > 20000, "distribution sample is too small");
        for (auto count : statistics.typeCounts()) {
            require(std::abs(count / total - 0.25) < 0.015, "equal type weights are unexpectedly biased");
        }
        require(std::abs(statistics.leftCount() / total - 0.5) < 0.015, "lane probability is unexpectedly biased");
        require(end.size() < 16, "exited vehicle history was not pruned");
        std::cout << "Distribution over " << statistics.generatedCount() << " draws:";
        for (auto count : statistics.typeCounts()) std::cout << ' ' << count;
        std::cout << "; left=" << statistics.leftCount() << '\n';

        bool sawDelay = false;
        bool sawConsecutiveType = false;
        // Crowded attempts stress admission, not just the initial sparse fixture.
        config.minimumSpawnInterval = 0.05;
        config.maximumSpawnInterval = 0.1;
        for (std::uint32_t seed = 0; seed < 64; ++seed) {
            lc::RandomVehicleStream dense(config, seed);
            for (int step = 0; step <= 400; ++step) {
                const double time = step * 0.25;
                const auto cars = dense.sample(time);
                verifySharedVisibleIntervals(config, cars, time);
                require(cars.size() < 16, "dense generation accumulated an unbounded queue");
                for (std::size_t i = 0; i < cars.size(); ++i) {
                    sawDelay = sawDelay || cars[i].entryDelay > 0.0;
                    if (i && cars[i].id == cars[i - 1].id + 1 && cars[i].type == cars[i - 1].type) sawConsecutiveType = true;
                }
            }
        }
        require(sawDelay, "clearance delay path was not exercised");
        require(sawConsecutiveType, "random stream acts like a nonrepeating four-type cycle");

        config = {};
        config.vehicleTypeWeights = {0.0, 0.0, 1.0, 0.0};
        config.leftLaneProbability = 1.0;
        lc::RandomVehicleStream taxis(config, 7);
        (void)taxis.sample(300.0);
        require(taxis.typeCounts()[2] == taxis.generatedCount() && taxis.leftCount() == taxis.generatedCount(),
                "configured type/lane weights not honored");
        config = {};
        config.vehicleTypeWeights.fill(0.0);
        rejects([&] { lc::RandomVehicleStream invalid(config, 0); });
        config = {};
        config.vehicleTypeWeights[1] = -1.0;
        rejects([&] { lc::RandomVehicleStream invalid(config, 0); });
        config = {};
        config.minimumSpawnInterval = 0.0;
        rejects([&] { lc::RandomVehicleStream invalid(config, 0); });
        config = {};
        config.maximumSpawnInterval = 1.0;
        rejects([&] { lc::RandomVehicleStream invalid(config, 0); });
        config = {};
        config.leftLaneProbability = 1.1;
        rejects([&] { lc::RandomVehicleStream invalid(config, 0); });
        std::cout << "PASS: random replay, probability, continuous clearance across 64 dense seeds, pruning, and configuration contracts\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
