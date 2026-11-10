#include "game.hpp"
#include "forward_motion.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace lc {
Game::Game(GameplayConfig config) : player_(config) {
    if (!std::isfinite(config.maxTransferRelativeSpeed) || config.maxTransferRelativeSpeed < 0.0) {
        throw std::invalid_argument("Relative-speed threshold must be finite and nonnegative.");
    }
    for (const auto& spec : config.vehicleSpecs) {
        if (!std::isfinite(spec.maxBoardingRelativeSpeed) || spec.maxBoardingRelativeSpeed < 0 ||
            !std::isfinite(spec.rideServiceDuration) || spec.rideServiceDuration <= 0 ||
            !std::isfinite(spec.rideServiceDistance()) || spec.rideServiceDistance() <= 0)
            throw std::invalid_argument("Invalid boarding/service parameters.");
        if (!std::isfinite(spec.catchZoneLength) || spec.catchZoneLength <= 0.0 || spec.catchZoneLength > spec.length) {
            throw std::invalid_argument("Catch zone must be positive and contained in the vehicle body.");
        }
    }
}

bool Game::requestTransfer(int direction, std::span<const Vehicle> vehicles) {
    if (!player_.requestTransfer(direction)) return false;
    if (attachedVehicle_) lastVehicleExitTime_ = player_.elapsedSeconds();
    attachedVehicle_.reset(); // Leaving releases attachment, preserving forward momentum.
    currentVehicle_.reset();
    remainingRideDistance_ = 0;
    returningFromService_ = false;
    scheduledCatchTime_.reset();
    scheduledIntersectionTime_.reset();
    transfer_ = TransferDebug{};
    transfer_.outcome = TransferOutcome::Pending;
    transfer_.startTime = player_.elapsedSeconds();
    transfer_.startPosition = player_.forwardPosition();
    transfer_.startSpeed = player_.forwardSpeed();
    transfer_.duration = player_.config().lateralTransferDuration;
    if (player_.targetLane() == Lane::Center) return true;

    const auto motion=advanceForward(transfer_.startSpeed,transfer_.duration,player_.config());
    transfer_.endSpeed=motion.speed; transfer_.endPosition=transfer_.startPosition+motion.distance;

    std::optional<Vehicle> rejected;
    CatchWindow rejectedWindow;
    for (const auto& vehicle : vehicles) {
        if (vehicle.lane != player_.targetLane()) continue;
        if (vehicleConsumed(vehicle.id)) {
            transfer_.excludedUsedVehicle = vehicle.id;
            continue;
        }
        if(!transfer_.diagnosticTarget ||
           std::abs(vehicle.forwardPosition-transfer_.startPosition)<std::abs(transfer_.diagnosticTarget->forwardPosition-transfer_.startPosition) ||
           (std::abs(vehicle.forwardPosition-transfer_.startPosition)==std::abs(transfer_.diagnosticTarget->forwardPosition-transfer_.startPosition) && vehicle.id<transfer_.diagnosticTarget->id))
            transfer_.diagnosticTarget=vehicle;
        const auto& spec = player_.config().vehicleSpecs.at(static_cast<std::size_t>(vehicle.type));
        auto actualSpec = spec;
        actualSpec.speed = vehicle.speed;
        const auto window = predictBoardingCatch(transfer_.startPosition, transfer_.startSpeed,
            vehicle.forwardPosition, actualSpec, transfer_.duration, player_.config());
        if (!window.intersects) continue;
        if (!window.speedAllowed) {
            if (!rejected || window.enterTime < rejectedWindow.enterTime) { rejected = vehicle; rejectedWindow = window; }
            continue;
        }
        if (!scheduledCatchTime_ || window.enterTime < transfer_.prediction.enterTime ||
            (window.enterTime == transfer_.prediction.enterTime && vehicle.id < transfer_.targetAtStart->id)) {
            transfer_.targetAtStart = vehicle;
            transfer_.prediction = window;
            scheduledCatchTime_ = transfer_.startTime + window.enterTime;
        }
    }
    if (!scheduledCatchTime_ && rejected) {
        transfer_.targetAtStart = rejected;
        transfer_.prediction = rejectedWindow;
    }
    if(transfer_.targetAtStart) transfer_.diagnosticTarget=transfer_.targetAtStart;
    if(transfer_.diagnosticTarget) {
        const auto& target=*transfer_.diagnosticTarget;
        auto spec=player_.config().vehicleSpecs[static_cast<std::size_t>(target.type)]; spec.speed=target.speed;
        transfer_.diagnosticPrediction=predictBoardingCatch(transfer_.startPosition,transfer_.startSpeed,target.forwardPosition,spec,transfer_.duration,player_.config());
        transfer_.allowedDifference=spec.maxBoardingRelativeSpeed;
        if (transfer_.diagnosticPrediction.intersects)
            scheduledIntersectionTime_ = transfer_.startTime + transfer_.diagnosticPrediction.enterTime;
        transfer_.zoneStartRear=target.forwardPosition-spec.catchZoneLength/2;
        transfer_.zoneStartFront=target.forwardPosition+spec.catchZoneLength/2;
        transfer_.zoneEndRear=transfer_.zoneStartRear+target.speed*transfer_.duration;
        transfer_.zoneEndFront=transfer_.zoneStartFront+target.speed*transfer_.duration;
    }
    boardingDebug_=transfer_;
    return true;
}

void Game::update(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0) throw std::invalid_argument("Invalid game delta.");
    double remainder = seconds;
    do {
        const bool wasTransferring = player_.transferring();
        double step = remainder;
        if (wasTransferring) step = std::min(step, player_.transferRemainingSeconds());
        if (scheduledCatchTime_) step = std::min(step, std::max(0.0, *scheduledCatchTime_ - player_.elapsedSeconds()));
        if (scheduledIntersectionTime_) step = std::min(step, std::max(0.0, *scheduledIntersectionTime_ - player_.elapsedSeconds()));
        const bool wasAttached = attachedVehicle_.has_value();
        if (wasAttached) step = std::min(step, remainingRideDistance_ / player_.forwardSpeed());
        // Split at BOTH catch and transfer completion. A large update must apply
        // a miss at the correct instant, then recover for the remaining time.
        player_.update(step, !attachedVehicle_);
        if (wasAttached) {
            remainingRideDistance_ = std::max(0.0, remainingRideDistance_ - player_.forwardSpeed() * step);
            rideRemainingById_[*attachedVehicle_] = remainingRideDistance_;
        }
        remainder -= step;
        const double timeTolerance = 8 * std::numeric_limits<double>::epsilon() *
                                     std::max(1.0, player_.elapsedSeconds());
        if (scheduledIntersectionTime_ && *scheduledIntersectionTime_ <= player_.elapsedSeconds() + timeTolerance) {
            transfer_.intersectionTime = player_.elapsedSeconds() - transfer_.startTime;
            transfer_.playerSpeedAtIntersection = player_.forwardSpeed();
            transfer_.absoluteDifferenceAtIntersection = std::abs(transfer_.diagnosticTarget->speed - player_.forwardSpeed());
            transfer_.speedPassedAtIntersection = transfer_.absoluteDifferenceAtIntersection <= transfer_.allowedDifference + 1e-10;
            scheduledIntersectionTime_.reset();
            boardingDebug_ = transfer_;
        }
        if (scheduledCatchTime_ && *scheduledCatchTime_ <= player_.elapsedSeconds() + timeTolerance) {
            // The predictor chooses a candidate; actual event velocity is the
            // final authority. Never attach using only a start/end speed check.
            if (!transfer_.speedPassedAtIntersection) {
                transfer_.prediction.speedAllowed = false;
                scheduledCatchTime_.reset();
            } else {
                const auto& target = *transfer_.targetAtStart;
                player_.setForwardSpeed(target.speed);
                attachedVehicle_ = target.id;
                currentVehicle_ = target;
                // Keep the existing distance event integrator; duration is the
                // sole tuning input, using the attached vehicle's constant speed.
                const double budget = target.speed * player_.config().vehicleSpecs[static_cast<std::size_t>(target.type)].rideServiceDuration;
                const auto [entry, inserted] = rideRemainingById_.try_emplace(target.id, budget);
                if (!inserted) throw std::logic_error("Consumed vehicle selected for boarding");
                lastConsumedVehicle_ = target.id;
                remainingRideDistance_ = entry->second;
                transfer_.outcome = TransferOutcome::Following;
                scheduledCatchTime_.reset();
            }
        }
        if (wasAttached && remainingRideDistance_ <= 1e-9) {
            boardingDebug_.outcome=TransferOutcome::Success;
            lastVehicleExitTime_ = player_.elapsedSeconds();
            rideRemainingById_[*attachedVehicle_] = 0;
            attachedVehicle_.reset();
            currentVehicle_.reset();
            remainingRideDistance_ = 0;
            scheduledCatchTime_.reset();
            transfer_ = TransferDebug{};
            transfer_.outcome = TransferOutcome::ServiceEnd;
            player_.returnToCenter();
            returningFromService_ = player_.transferring();
        } else if (wasTransferring && !player_.transferring()) {
            if (attachedVehicle_) transfer_.outcome = TransferOutcome::Success;
            else if (player_.lane() == Lane::Center) {
                if (!returningFromService_) transfer_.outcome = TransferOutcome::Center;
                returningFromService_ = false;
            }
            else {
                transfer_.outcome = transfer_.targetAtStart && !transfer_.prediction.speedAllowed
                    ? TransferOutcome::SpeedRejected : TransferOutcome::Miss;
                transfer_.speedBeforePenalty = player_.forwardSpeed();
                // The floor prevents repeated misses stopping/reversing motion;
                // never increase an unusually slow configured vehicle's speed.
                transfer_.speedAfterPenalty = std::min(transfer_.speedBeforePenalty,
                    std::max(player_.config().minimumForwardSpeed, transfer_.speedBeforePenalty - player_.config().missPenalty));
                transfer_.penaltyApplied = transfer_.speedBeforePenalty - transfer_.speedAfterPenalty;
                player_.setForwardSpeed(transfer_.speedAfterPenalty);
            }
        }
        if(transfer_.outcome==TransferOutcome::Following || transfer_.outcome==TransferOutcome::Success ||
           transfer_.outcome==TransferOutcome::Miss || transfer_.outcome==TransferOutcome::SpeedRejected) boardingDebug_=transfer_;
    } while (remainder > 0);
}

const char* Game::stateName() const {
    if (returningFromService_) return "SERVICE / RETURNING";
    if (player_.transferring()) return attachedVehicle_ ? "TRANSFER / FOLLOWING" : "TRANSFERRING";
    if (attachedVehicle_) return "RIDING";
    if (player_.forwardSpeed() > player_.config().normalRunSpeed) return "DECELERATING";
    if (player_.forwardSpeed() < player_.config().normalRunSpeed) return "RECOVERING";
    return "RUNNING";
}
const char* outcomeName(TransferOutcome outcome) {
    switch (outcome) {
    case TransferOutcome::None: return "READY";
    case TransferOutcome::Pending: return "PENDING";
    case TransferOutcome::Following: return "CATCH / FINISHING TRANSFER";
    case TransferOutcome::Success: return "SUCCESS";
    case TransferOutcome::Miss: return "MISS / NO INTERSECTION";
    case TransferOutcome::SpeedRejected: return "MISS / RELATIVE SPEED";
    case TransferOutcome::Center: return "CENTER / RELEASED";
    case TransferOutcome::ServiceEnd: return "SERVICE END / NO MISS";
    }
    return "UNKNOWN";
}
}
