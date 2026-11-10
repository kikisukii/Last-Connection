#pragma once
#include "vehicles.hpp"
#include "game.hpp"
#include "traffic_safety.hpp"
#include "encounter_planner.hpp"
#include <span>

namespace lc {
struct LaneTrafficDebug {
    std::size_t activeCount = 0;
    std::optional<VehicleType> pendingVehicle;
    double nextSpawnTime = 0;
    bool blocked = false;
    const char* blockReason = "CLEAR";
    bool plannedEncounter = false;
    std::optional<VehicleType> lastSpawnedType;
    std::array<std::size_t,4> spawnCounts{};
    OpportunityKind pendingKind = OpportunityKind::Immediate;
};
struct EncounterDebug {
    const char* status = "WAITING";
    std::optional<std::size_t> vehicleId;
    double catchTime = 0;
    bool continuation = false;
    std::array<std::size_t,4> immediateCandidates{}, preparedCandidates{};
};
class LiveTraffic {
public:
    LiveTraffic(GameplayConfig config, std::uint32_t seed, std::span<const Vehicle> initial = {});
    void update(double seconds, double playerPosition, double playerSpeed);
    void update(double seconds, const Game& game);
    std::span<const Vehicle> current() const { return active_; }
    std::uint32_t seed() const { return seed_; }
    const std::array<std::size_t, 4>& typeCounts() const { return typeCounts_; }
    const LaneTrafficDebug& laneDebug(Lane lane) const { return lanes_[lane == Lane::Left ? 0 : 1].debug; }
    const EncounterDebug& encounterDebug() const { return encounter_; }
    const std::optional<TrafficViolation>& safetyViolation() const { return safetyViolation_; }
    bool typeAvailable(Lane lane, VehicleType type) const;
private:
    struct Request { VehicleType type; std::size_t id; double time; std::optional<double> plannedPosition;
        OpportunityKind kind = OpportunityKind::Immediate; std::optional<std::size_t> prerequisiteId = {}; };
    struct TrackedVehicle { Vehicle vehicle; bool entered = false; double spawnTime = 0; double spawnPosition = 0; };
    struct LaneScheduler {
        double nextSpawnTime = 0;
        std::optional<Request> pending;
        std::vector<TrackedVehicle> active;
        std::mt19937 random;
        LaneTrafficDebug debug;
    };
    double randomUnit(LaneScheduler& lane);
    std::optional<VehicleType> drawType(LaneScheduler& lane, Lane which);
    void selectEncounters(const Game& game);
    void updateCandidateDebug(const Game& game);
    bool visible(const Vehicle& vehicle, double playerPosition) const;
    void updateImpl(double seconds, double position, double speed, const Game* game);
    void rebuildSnapshot();
    void auditSafety();
    GameplayConfig config_;
    std::uint32_t seed_;
    double totalWeight_ = 0;
    double time_ = 0;
    double nextEncounterReview_ = 0;
    std::size_t nextId_ = 0;
    std::array<std::size_t, 4> typeCounts_{};
    std::array<LaneScheduler, 2> lanes_;
    std::vector<Vehicle> active_;
    EncounterDebug encounter_;
    std::optional<TrafficViolation> safetyViolation_;
};
}
