#include "vehicles.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace lc {
namespace {
void validateTrafficConfig(const GameplayConfig& config) {
    if (!std::isfinite(config.normalRunSpeed) || config.normalRunSpeed <= 0.0 ||
        !std::isfinite(config.visibleBehindDistance) || config.visibleBehindDistance <= 0.0 ||
        !std::isfinite(config.visibleAheadDistance) || config.visibleAheadDistance <= 0.0 ||
        !std::isfinite(config.vehicleMinimumGap) || config.vehicleMinimumGap < 0.0) {
        throw std::invalid_argument("Traffic camera bounds/speed must be positive and gap nonnegative, all finite.");
    }
    for (const auto& spec : config.vehicleSpecs) {
        if (!std::isfinite(spec.speed) || spec.speed <= config.normalRunSpeed ||
            !std::isfinite(spec.length) || spec.length <= 0.0) {
            throw std::invalid_argument("Phase 3 traffic must be faster than the fixed-speed camera; length must be positive, all finite.");
        }
    }
}

double safeEntryAfter(const GameplayConfig& config, const VehicleSpec& follower,
                      const VehicleSpec& leader, double leaderEntry) {
    const double viewLength = config.visibleBehindDistance + config.visibleAheadDistance;
    const double leaderRelativeSpeed = leader.speed - config.normalRunSpeed;
    const double leaderExit = leaderEntry + (viewLength + leader.length) / leaderRelativeSpeed;
    // Relative separation is linear: protect first and last shared visible instants.
    const double safeAtEntry = leaderEntry + (leader.length + config.vehicleMinimumGap) / leaderRelativeSpeed;
    const double safeAtExit = leaderExit - (viewLength - config.vehicleMinimumGap) /
                             (follower.speed - config.normalRunSpeed);
    return std::max(safeAtEntry, safeAtExit);
}
}

VehicleStream::VehicleStream(GameplayConfig config) : config_(config) {
    validateTrafficConfig(config_);

    // Validate seeds before using their type as a table index in the planner.
    for (const auto& seed : config_.vehicleSequence) {
        const int type = static_cast<int>(seed.type);
        if (type < 0 || type >= static_cast<int>(config_.vehicleSpecs.size()) ||
            (seed.lane != Lane::Left && seed.lane != Lane::Right) ||
            !std::isfinite(seed.initialForwardPosition)) {
            throw std::invalid_argument("Vehicle seed must name a valid type, outer lane and finite position.");
        }
    }

    const double nearEdge = -config_.visibleBehindDistance;
    std::array<double, 8> requestedEntry{};
    std::array<double, 8> entry{};
    std::array<std::size_t, 8> order{};
    std::iota(order.begin(), order.end(), 0);
    for (std::size_t i = 0; i < order.size(); ++i) {
        const auto& seed = config_.vehicleSequence[i];
        const auto& spec = config_.vehicleSpecs[static_cast<std::size_t>(seed.type)];
        requestedEntry[i] = (nearEdge - spec.length / 2.0 - seed.initialForwardPosition) /
                            (spec.speed - config_.normalRunSpeed);
    }
    // Authored entry order, with stable IDs for ties; changes happen off screen.
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return requestedEntry[a] < requestedEntry[b];
    });
    for (std::size_t rank = 0; rank < order.size(); ++rank) {
        const auto id = order[rank];
        const auto& seed = config_.vehicleSequence[id];
        const auto& spec = config_.vehicleSpecs[static_cast<std::size_t>(seed.type)];
        const double relativeSpeed = spec.speed - config_.normalRunSpeed;
        entry[id] = requestedEntry[id];
        for (std::size_t previous = 0; previous < rank; ++previous) {
            const auto leaderId = order[previous];
            const auto& leaderSeed = config_.vehicleSequence[leaderId];
            if (leaderSeed.lane != seed.lane) continue;
            const auto& leader = config_.vehicleSpecs[static_cast<std::size_t>(leaderSeed.type)];
            entry[id] = std::max(entry[id], safeEntryAfter(config_, spec, leader, entry[leaderId]));
        }
        entryDelays_[id] = entry[id] - requestedEntry[id];
        // Move the initial position back, never snap or brake a visible vehicle.
        initialPositions_[id] = seed.initialForwardPosition - relativeSpeed * entryDelays_[id];
    }
}

std::array<Vehicle, 8> VehicleStream::sample(double elapsedSeconds) const {
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0) {
        throw std::invalid_argument("Vehicle sample time must be finite and nonnegative.");
    }
    std::array<Vehicle, 8> vehicles{};
    for (std::size_t i = 0; i < vehicles.size(); ++i) {
        const auto& seed = config_.vehicleSequence[i];
        const auto& spec = config_.vehicleSpecs[static_cast<std::size_t>(seed.type)];
        vehicles[i] = Vehicle{i, seed.type, seed.lane,
            initialPositions_[i] + spec.speed * elapsedSeconds, spec.speed, spec.length, entryDelays_[i]};
    }
    return vehicles;
}

RandomVehicleStream::RandomVehicleStream(GameplayConfig config, std::uint32_t seed)
    : config_(config), seed_(seed), random_(seed), nextRequestTime_(config.firstVehicleEntryTime) {
    validateTrafficConfig(config_);
    for (double weight : config_.vehicleTypeWeights) {
        if (!std::isfinite(weight) || weight < 0.0) throw std::invalid_argument("Vehicle weights must be finite and nonnegative.");
        totalTypeWeight_ += weight;
    }
    if (!std::isfinite(totalTypeWeight_) || totalTypeWeight_ <= 0.0 ||
        !std::isfinite(config_.leftLaneProbability) || config_.leftLaneProbability < 0.0 || config_.leftLaneProbability > 1.0 ||
        !std::isfinite(config_.firstVehicleEntryTime) || config_.firstVehicleEntryTime < 0.0 ||
        !std::isfinite(config_.minimumSpawnInterval) || config_.minimumSpawnInterval <= 0.0 ||
        !std::isfinite(config_.maximumSpawnInterval) || config_.maximumSpawnInterval < config_.minimumSpawnInterval) {
        throw std::invalid_argument("Invalid traffic probabilities or spawn timing.");
    }
}

double RandomVehicleStream::randomUnit() {
    // mt19937's specified 32-bit output + explicit mapping is portable across
    // native and Web builds; standard distribution implementations may differ.
    constexpr double outcomes = 4294967296.0;
    return static_cast<double>(random_()) / outcomes;
}

VehicleType RandomVehicleStream::drawType() {
    const double draw = randomUnit() * totalTypeWeight_;
    double cumulative = 0.0;
    std::size_t lastPositive = 0;
    for (std::size_t i = 0; i < config_.vehicleTypeWeights.size(); ++i) {
        if (config_.vehicleTypeWeights[i] > 0.0) lastPositive = i;
        cumulative += config_.vehicleTypeWeights[i];
        if (draw < cumulative) return static_cast<VehicleType>(i);
    }
    return static_cast<VehicleType>(lastPositive); // rounding guard, never a disabled type
}

void RandomVehicleStream::removeExited(double time) {
    std::erase_if(active_, [time](const auto& plan) { return plan.exitTime < time; });
}

void RandomVehicleStream::generateNext() {
    const auto type = drawType();
    const auto lane = randomUnit() < config_.leftLaneProbability ? Lane::Left : Lane::Right;
    const std::size_t laneIndex = lane == Lane::Left ? 0 : 1;
    const auto& spec = config_.vehicleSpecs[static_cast<std::size_t>(type)];
    double entry = nextRequestTime_;
    if (const auto& leader = lastInLane_[laneIndex]; leader && leader->exitTime >= entry) {
        entry = std::max(entry, safeEntryAfter(config_, spec,
            VehicleSpec{leader->vehicle.speed, leader->vehicle.length}, leader->entryTime));
    }
    const double relativeSpeed = spec.speed - config_.normalRunSpeed;
    const double initialPosition = -config_.visibleBehindDistance - spec.length / 2 - relativeSpeed * entry;
    const double exitTime = entry + (config_.visibleBehindDistance + config_.visibleAheadDistance + spec.length) / relativeSpeed;
    PlannedVehicle plan{Vehicle{nextId_++, type, lane, initialPosition, spec.speed, spec.length,
                               entry - nextRequestTime_}, initialPosition, entry, exitTime};
    active_.push_back(plan);
    lastInLane_[laneIndex] = plan;
    ++typeCounts_[static_cast<std::size_t>(type)];
    if (lane == Lane::Left) ++leftCount_;
    // Keep a delayed draw rather than rerolling it. Start the next interval at
    // actual entry to prevent an unbounded queue of unsafe off-screen candidates.
    nextRequestTime_ = entry + config_.minimumSpawnInterval +
        randomUnit() * (config_.maximumSpawnInterval - config_.minimumSpawnInterval);
}

std::vector<Vehicle> RandomVehicleStream::sample(double elapsedSeconds) {
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds < lastSampleTime_) {
        throw std::invalid_argument("Random traffic time must be finite, nonnegative and monotonic; recreate stream to replay.");
    }
    while (nextRequestTime_ <= elapsedSeconds) {
        removeExited(nextRequestTime_);
        generateNext();
    }
    removeExited(elapsedSeconds);
    lastSampleTime_ = elapsedSeconds;
    std::vector<Vehicle> result;
    result.reserve(active_.size());
    for (const auto& plan : active_) {
        auto vehicle = plan.vehicle;
        vehicle.forwardPosition = plan.initialPosition + vehicle.speed * elapsedSeconds;
        result.push_back(vehicle);
    }
    return result;
}

const char* vehicleName(VehicleType type) {
    switch (type) {
    case VehicleType::Bike: return "BIKE";
    case VehicleType::Bus: return "BUS";
    case VehicleType::Taxi: return "TAXI";
    case VehicleType::Express: return "EXPRESS";
    }
    return "UNKNOWN";
}
}
