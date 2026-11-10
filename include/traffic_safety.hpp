#pragma once
#include "vehicles.hpp"
#include <limits>
#include <span>
#include <string>
namespace lc {
struct TrafficSafety {
    bool allowed = true;
    double ttc = std::numeric_limits<double>::infinity();
    double frontExitTime = 0;
    const char* reason = "CLEAR";
    std::optional<std::size_t> leaderId, followerId;
    double gap = 0;
    double clearanceTime = std::numeric_limits<double>::infinity();
};
struct TrafficRecord {
    Vehicle vehicle;
    double spawnTime;
    double spawnPosition;
};
struct TrafficViolation {
    double simulationTime;
    TrafficRecord leader, follower;
    TrafficSafety safety;
};
// Camera-independent safety proof. All bodies retire beyond this fixed world
// boundary; an ordinary viewport exit may retire them earlier, never later.
double trafficWorldEnd(const GameplayConfig& config);
TrafficSafety inspectTrafficPair(const Vehicle& a, const Vehicle& b, const GameplayConfig& config);
TrafficSafety checkTrafficAdmission(const Vehicle& candidate, std::span<const Vehicle> active,
                                    const GameplayConfig& config);
std::optional<TrafficViolation> findTrafficViolation(std::span<const TrafficRecord> active,
                                                   const GameplayConfig& config, double simulationTime);
std::string formatTrafficViolation(const TrafficViolation& violation);
}
