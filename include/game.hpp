#pragma once
#include "catch_detection.hpp"
#include "movement.hpp"
#include "vehicles.hpp"
#include <optional>
#include <span>
#include <unordered_map>

namespace lc {
enum class TransferOutcome { None, Pending, Following, Success, Miss, SpeedRejected, Center, ServiceEnd };
struct TransferDebug {
    TransferOutcome outcome = TransferOutcome::None;
    std::optional<Vehicle> targetAtStart;
    double startTime = 0.0;
    double startPosition = 0.0;
    double startSpeed = 0.0;
    double duration = 0.0;
    double penaltyApplied = 0.0;
    double speedBeforePenalty = 0.0;
    double speedAfterPenalty = 0.0;
    CatchWindow prediction;
    // Diagnostic-only fallback when no vehicle geometrically intersects.
    std::optional<Vehicle> diagnosticTarget;
    CatchWindow diagnosticPrediction;
    double endSpeed = 0, endPosition = 0;
    double allowedDifference = 0;
    // Actual event snapshot, before inheriting vehicle speed (also on rejection).
    std::optional<double> intersectionTime;
    double playerSpeedAtIntersection = 0, absoluteDifferenceAtIntersection = 0;
    bool speedPassedAtIntersection = false;
    std::optional<std::size_t> excludedUsedVehicle;
    double zoneStartRear = 0, zoneStartFront = 0, zoneEndRear = 0, zoneEndFront = 0;
};

class Game {
public:
    explicit Game(GameplayConfig config = {});
    bool requestTransfer(int direction, std::span<const Vehicle> vehicles);
    void update(double seconds);
    const Movement& player() const { return player_; }
    const TransferDebug& transferDebug() const { return transfer_; }
    const TransferDebug& lastBoardingDebug() const { return boardingDebug_; }
    std::optional<std::size_t> attachedVehicle() const { return attachedVehicle_; }
    const std::optional<Vehicle>& currentVehicle() const { return currentVehicle_; }
    double remainingRideDistance() const { return remainingRideDistance_; }
    double currentRideDuration() const {
        return currentVehicle_ ? player_.config().vehicleSpecs[static_cast<std::size_t>(currentVehicle_->type)].rideServiceDuration : 0;
    }
    double remainingRideTime() const { return currentVehicle_ ? remainingRideDistance_ / currentVehicle_->speed : 0; }
    double elapsedRideTime() const { return currentRideDuration() - remainingRideTime(); }
    bool returningFromService() const { return returningFromService_; }
    bool vehicleConsumed(std::size_t id) const { return rideRemainingById_.contains(id); }
    // Compatibility name used by the traffic planner: a connection is consumed
    // on its first successful catch, independently of its remaining service.
    bool serviceConsumed(std::size_t id) const {
        return vehicleConsumed(id);
    }
    bool serviceCompleted(std::size_t id) const {
        const auto it = rideRemainingById_.find(id);
        return it != rideRemainingById_.end() && it->second <= 0;
    }
    std::optional<double> timeSinceVehicleExit() const {
        return lastVehicleExitTime_ ? std::optional<double>(player_.elapsedSeconds() - *lastVehicleExitTime_) : std::nullopt;
    }
    std::optional<std::size_t> lastConsumedVehicle() const { return lastConsumedVehicle_; }
    const char* stateName() const;
private:
    Movement player_;
    TransferDebug transfer_;
    TransferDebug boardingDebug_; // retained when returning to CENTER
    std::optional<std::size_t> attachedVehicle_;
    std::optional<double> scheduledCatchTime_;
    std::optional<double> scheduledIntersectionTime_;
    std::optional<double> lastVehicleExitTime_;
    std::optional<Vehicle> currentVehicle_;
    double remainingRideDistance_ = 0;
    bool returningFromService_ = false;
    std::unordered_map<std::size_t, double> rideRemainingById_;
    std::optional<std::size_t> lastConsumedVehicle_;
};
const char* outcomeName(TransferOutcome outcome);
}
