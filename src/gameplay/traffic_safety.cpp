#include "traffic_safety.hpp"
#include <algorithm>
#include <iomanip>
#include <locale>
#include <sstream>

namespace lc {
double trafficWorldEnd(const GameplayConfig& config) { return config.finishDistance + config.visibleAheadDistance; }

TrafficSafety inspectTrafficPair(const Vehicle& a, const Vehicle& b, const GameplayConfig& config) {
    TrafficSafety result;
    if (a.id == b.id || a.lane != b.lane) return result;
    const bool aBehind = a.forwardPosition < b.forwardPosition;
    const auto& rear = aBehind ? a : b;
    const auto& front = aBehind ? b : a;
    result.leaderId = front.id;
    result.followerId = rear.id;
    result.gap = front.forwardPosition - front.length/2 - rear.forwardPosition - rear.length/2;
    const double closing = rear.speed - front.speed;
    result.frontExitTime = std::max(0.0, (trafficWorldEnd(config) + front.length/2 - front.forwardPosition) / front.speed);
    if (result.gap <= 0) result.ttc = 0;
    else if (closing > 0) result.ttc = result.gap / closing;
    if (closing > 0) result.clearanceTime = (result.gap - config.vehicleMinimumGap) / closing;
    if (result.gap < config.vehicleMinimumGap - 1e-8) {
        result.allowed = false;
        result.reason = result.gap < 0 ? "ACTUAL OVERLAP" : "BODY GAP";
    } else if (closing > 0 && result.frontExitTime > result.clearanceTime + 1e-10) {
        result.allowed = false;
        result.reason = "TTC BEFORE WORLD EXIT";
    }
    // No absolute minimum-TTC rejection: even a short TTC is safe if the
    // leader exits first. The existing one-unit clearance is still preserved.
    // Neither player nor camera velocity is an input to this calculation.
    return result;
}

TrafficSafety checkTrafficAdmission(const Vehicle& candidate, std::span<const Vehicle> active,
                                    const GameplayConfig& config) {
    TrafficSafety result;
    for (const auto& other : active) {
        const auto pair = inspectTrafficPair(candidate, other, config);
        if (!pair.allowed) return pair;
        if (pair.leaderId && (!result.leaderId || pair.ttc < result.ttc)) result = pair;
    }
    return result;
}

std::optional<TrafficViolation> findTrafficViolation(std::span<const TrafficRecord> active,
                                                   const GameplayConfig& config, double simulationTime) {
    // All same-lane pairs, including neighbors; only a few bodies are active.
    for (std::size_t i=0; i<active.size(); ++i) for (std::size_t j=i+1; j<active.size(); ++j) {
        const auto safety = inspectTrafficPair(active[i].vehicle, active[j].vehicle, config);
        if (!safety.allowed) {
            const bool iLeads = safety.leaderId == active[i].vehicle.id;
            return TrafficViolation{simulationTime, iLeads ? active[i] : active[j], iLeads ? active[j] : active[i], safety};
        }
    }
    return {};
}

std::string formatTrafficViolation(const TrafficViolation& v) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17) << "TRAFFIC SAFETY VIOLATION"
        << " | simulationTime=" << v.simulationTime << " | reason=" << v.safety.reason
        << " | lane=" << static_cast<int>(v.leader.vehicle.lane)
        << " | Leader ID=" << v.leader.vehicle.id << " | Leader speed=" << v.leader.vehicle.speed
        << " | Leader position=" << v.leader.vehicle.forwardPosition << " | Leader length=" << v.leader.vehicle.length
        << " | Leader Spawn time=" << v.leader.spawnTime << " | Leader Spawn position=" << v.leader.spawnPosition
        << " | Follower ID=" << v.follower.vehicle.id << " | Follower speed=" << v.follower.vehicle.speed
        << " | Follower position=" << v.follower.vehicle.forwardPosition << " | Follower length=" << v.follower.vehicle.length
        << " | Follower Spawn time=" << v.follower.spawnTime << " | Follower Spawn position=" << v.follower.spawnPosition
        << " | Gap=" << v.safety.gap << " | TTC=" << v.safety.ttc
        << " | Front exit time=" << v.safety.frontExitTime << " | Clearance time=" << v.safety.clearanceTime;
    return out.str();
}
}
