#pragma once
#include "gameplay_config.hpp"
#include <cstddef>
#include <optional>
#include <random>
#include <vector>

namespace lc {
struct Vehicle {
    std::size_t id; // stable sequence index; independent of visibility
    VehicleType type;
    Lane lane;
    double forwardPosition; // body center, same world units as the player
    double speed;
    double length;
    double entryDelay; // seconds added by visible-traffic admission planning
};

class VehicleStream {
public:
    explicit VehicleStream(GameplayConfig config = {});
    // Phase 3 assumes a camera moving at normalRunSpeed. Plan entry times once,
    // then sample constant-speed trajectories; no visible braking or teleporting.
    std::array<Vehicle, 8> sample(double elapsedSeconds) const;
private:
    GameplayConfig config_;
    std::array<double, 8> initialPositions_{};
    std::array<double, 8> entryDelays_{};
};

class RandomVehicleStream {
public:
    RandomVehicleStream(GameplayConfig config, std::uint32_t seed);
    // Stateful generation accepts monotonic simulation time, including equal times.
    // Same seed + config reproduces the same stream regardless of frame partitioning.
    std::vector<Vehicle> sample(double elapsedSeconds);
    std::uint32_t seed() const { return seed_; }
    std::size_t generatedCount() const { return nextId_; }
    const std::array<std::size_t, 4>& typeCounts() const { return typeCounts_; }
    std::size_t leftCount() const { return leftCount_; }

private:
    struct PlannedVehicle {
        Vehicle vehicle;
        double initialPosition;
        double entryTime;
        double exitTime;
    };
    double randomUnit();
    VehicleType drawType();
    void generateNext();
    void removeExited(double time);

    GameplayConfig config_;
    std::uint32_t seed_;
    std::mt19937 random_;
    double totalTypeWeight_ = 0.0;
    double nextRequestTime_;
    double lastSampleTime_ = 0.0;
    std::size_t nextId_ = 0;
    std::array<std::size_t, 4> typeCounts_{};
    std::size_t leftCount_ = 0;
    std::vector<PlannedVehicle> active_;
    std::array<std::optional<PlannedVehicle>, 2> lastInLane_{};
};
const char* vehicleName(VehicleType type);
}
