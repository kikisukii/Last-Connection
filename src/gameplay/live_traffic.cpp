#include "live_traffic.hpp"
#include "encounter_planner.hpp"
#include "traffic_safety.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string_view>
#include <cstdio>

namespace lc {
LiveTraffic::LiveTraffic(GameplayConfig config, std::uint32_t seed, std::span<const Vehicle> initial)
    : config_(config), seed_(seed) {
    const auto positive = [](double v) { return std::isfinite(v) && v > 0; };
    if (!positive(config.visibleAheadDistance) || !positive(config.visibleBehindDistance) ||
        !std::isfinite(config.vehicleMinimumGap) || config.vehicleMinimumGap < 0 ||
        !positive(config.trafficEntryMargin) || !positive(config.minimumSpawnTTC) ||
        !positive(config.finishDistance) || !positive(config.encounterTimeMin) ||
        !std::isfinite(config.encounterTimeMax) || config.encounterTimeMax < config.encounterTimeMin ||
        !positive(config.encounterReviewInterval) || !positive(config.serviceContinuationLeadTime) ||
        !positive(config.encounterVisibilityLeadTime)) throw std::invalid_argument("Invalid live traffic configuration.");
    for (const auto& spec : config.vehicleSpecs)
        if (!positive(spec.speed) || !positive(spec.length)) throw std::invalid_argument("Invalid vehicle specification.");
    for (double weight : config.vehicleTypeWeights) {
        if (!std::isfinite(weight) || weight < 0) throw std::invalid_argument("Invalid vehicle weight.");
        totalWeight_ += weight;
    }
    if (!positive(totalWeight_)) throw std::invalid_argument("Vehicle weights must have a positive finite sum.");
    for (std::size_t i = 0; i < lanes_.size(); ++i) {
        if (!std::isfinite(config.laneFirstSpawnTimes[i]) || config.laneFirstSpawnTimes[i] < 0 ||
            !positive(config.laneMinimumSpawnIntervals[i]) || !std::isfinite(config.laneMaximumSpawnIntervals[i]) ||
            config.laneMaximumSpawnIntervals[i] < config.laneMinimumSpawnIntervals[i])
            throw std::invalid_argument("Invalid lane scheduler configuration.");
        lanes_[i].random.seed(seed ^ (0x9e3779b9u * static_cast<std::uint32_t>(i + 1)));
        lanes_[i].nextSpawnTime = config.laneFirstSpawnTimes[i];
        lanes_[i].debug.nextSpawnTime = config.laneFirstSpawnTimes[i];
    }
    for (const auto& v : initial) {
        if (v.lane == Lane::Center || !std::isfinite(v.forwardPosition) || !positive(v.speed) || !positive(v.length))
            throw std::invalid_argument("Invalid initial traffic fixture.");
        lanes_[v.lane == Lane::Left ? 0 : 1].active.push_back({v, false, 0, v.forwardPosition});
        active_.push_back(v);
        auto& debug=lanes_[v.lane==Lane::Left?0:1].debug;
        debug.lastSpawnedType=v.type; ++debug.spawnCounts[static_cast<std::size_t>(v.type)];
        ++typeCounts_[static_cast<std::size_t>(v.type)];
        nextId_ = std::max(nextId_, v.id + 1);
    }
    auditSafety();
}
double LiveTraffic::randomUnit(LaneScheduler& lane) { return static_cast<double>(lane.random()) / 4294967296.0; }
bool LiveTraffic::typeAvailable(Lane lane, VehicleType type) const {
    if(!config_.constrainedVehicleSelection) return true;
    const auto index=lane==Lane::Left?0:1;
    if(lanes_[index].debug.lastSpawnedType==type) return false;
    const auto& opposite=lanes_[1-index];
    if(opposite.pending && opposite.pending->type==type) return false;
    return std::none_of(opposite.active.begin(),opposite.active.end(),[&](const auto& r){return r.vehicle.type==type;});
}
std::optional<VehicleType> LiveTraffic::drawType(LaneScheduler& lane, Lane which) {
    double weight=0;
    for(std::size_t i=0;i<4;++i) if(typeAvailable(which,static_cast<VehicleType>(i))) weight+=config_.vehicleTypeWeights[i];
    if(weight<=0) return {};
    const double draw = randomUnit(lane) * weight;
    double sum = 0;
    for (std::size_t i = 0; i < config_.vehicleTypeWeights.size(); ++i) {
        if(!typeAvailable(which,static_cast<VehicleType>(i))) continue;
        sum += config_.vehicleTypeWeights[i];
        if (draw < sum) return static_cast<VehicleType>(i);
    }
    return {};
}
void LiveTraffic::selectEncounters(const Game& game) {
    const bool existing=hasUpcomingEncounter(game,active_);
    auto candidates=planEncounters(game,{});
    const auto prepared=planPreparedEncounters(game,active_);
    candidates.insert(candidates.end(),prepared.begin(),prepared.end());
    encounter_.status=existing?"EXISTING OPPORTUNITY":"WAITING / LEGAL ENCOUNTER";
    for(std::size_t i=0;i<2;++i) {
        auto& scheduler=lanes_[i]; const Lane lane=i==0?Lane::Left:Lane::Right;
        if(scheduler.pending || time_+1e-8<scheduler.nextSpawnTime) continue;
        std::array<std::vector<const EncounterCandidate*>,4> options;
        int best=100;
        for(const auto& candidate:candidates) {
            if(candidate.lane!=lane || !typeAvailable(lane,candidate.type)) continue;
            if(existing && candidate.kind==OpportunityKind::Immediate) continue;
            const auto type=static_cast<std::size_t>(candidate.type);
            const auto& spec=config_.vehicleSpecs[type];
            Vehicle v{nextId_,candidate.type,lane,candidate.position,spec.speed,spec.length,0};
            if(!checkTrafficAdmission(v,active_,config_).allowed) continue;
            if(v.forwardPosition-v.length/2>=trafficWorldEnd(config_)) continue;
            // Immediate upgrades first, recovery next, then one-step preparation.
            const int tier=candidate.kind==OpportunityKind::Prepared?20:(spec.speed>game.player().forwardSpeed()?static_cast<int>(type):10);
            if(tier>best) continue;
            if(tier<best) { for(auto& group:options) group.clear(); best=tier; }
            options[type].push_back(&candidate);
        }
        double weight=0;
        for(std::size_t t=0;t<4;++t) if(!options[t].empty()) weight+=config_.vehicleTypeWeights[t];
        if(weight<=0) { scheduler.debug.blocked=true; scheduler.debug.blockReason="NO LEGAL ENCOUNTER"; continue; }
        double draw=randomUnit(scheduler)*weight;
        std::size_t chosen=0;
        for(std::size_t t=0;t<4;++t) if(!options[t].empty()) {
            chosen=t; draw-=config_.vehicleTypeWeights[t]; if(draw<0) break;
        }
        const auto& group=options[chosen];
        const auto& candidate=*group[std::min(group.size()-1,static_cast<std::size_t>(randomUnit(scheduler)*group.size()))];
        scheduler.pending=Request{candidate.type,nextId_++,time_,candidate.position,candidate.kind,candidate.prerequisiteId};
        scheduler.nextSpawnTime=time_+config_.laneMinimumSpawnIntervals[i]+randomUnit(scheduler)*(config_.laneMaximumSpawnIntervals[i]-config_.laneMinimumSpawnIntervals[i]);
        encounter_.status=candidate.kind==OpportunityKind::Prepared?"PREPARED PROGRESSION":(candidate.serviceContinuation?"SERVICE CONTINUATION":"IMMEDIATE CONNECTION");
        encounter_.vehicleId=scheduler.pending->id; encounter_.catchTime=time_+candidate.catchTime;
        encounter_.continuation=candidate.serviceContinuation;
    }
}
void LiveTraffic::updateCandidateDebug(const Game& game) {
    encounter_.immediateCandidates.fill(0); encounter_.preparedCandidates.fill(0);
    for(const auto& v:active_) {
        if(isUpcomingCandidate(game,v)) ++encounter_.immediateCandidates[static_cast<std::size_t>(v.type)];
        else if(isPreparedCandidate(game,v,active_)) ++encounter_.preparedCandidates[static_cast<std::size_t>(v.type)];
    }
}
bool LiveTraffic::visible(const Vehicle& v, double p) const {
    const double r = v.forwardPosition - p;
    return r + v.length / 2 >= -config_.visibleBehindDistance && r - v.length / 2 <= config_.visibleAheadDistance;
}
void LiveTraffic::rebuildSnapshot() {
    active_.clear();
    for (auto& lane : lanes_) {
        for (const auto& record : lane.active) active_.push_back(record.vehicle);
        lane.debug.activeCount = lane.active.size();
    }
}
void LiveTraffic::auditSafety() {
    std::vector<TrafficRecord> records;
    for (const auto& lane : lanes_) for (const auto& r : lane.active)
        records.push_back({r.vehicle, r.spawnTime, r.spawnPosition});
    if (const auto violation = findTrafficViolation(records, config_, time_)) {
        safetyViolation_ = violation;
        const auto message = formatTrafficViolation(*violation);
        std::fprintf(stderr, "%s\n", message.c_str());
        // Fail fast with evidence. Never hide a spawn bug by altering kinematics.
        throw std::logic_error(message);
    }
}
void LiveTraffic::update(double seconds, double position, double speed) { updateImpl(seconds, position, speed, nullptr); }
void LiveTraffic::update(double seconds, const Game& game) {
    updateImpl(seconds, game.player().forwardPosition(), game.player().forwardSpeed(), &game);
}
void LiveTraffic::updateImpl(double seconds, double position, double speed, const Game* game) {
    if (!std::isfinite(seconds) || seconds < 0 || !std::isfinite(position) || !std::isfinite(speed) || speed <= 0)
        throw std::invalid_argument("Invalid traffic update.");
    // Audit before movement/removal so a bad future collision course cannot be
    // hidden by a later viewport retirement or a large caller timestep.
    auditSafety();
    time_ += seconds;
    for (auto& lane : lanes_) {
        for (auto& record : lane.active) {
            record.vehicle.forwardPosition += record.vehicle.speed * seconds;
            if (visible(record.vehicle, position)) record.entered = true;
        }
        std::erase_if(lane.active, [&](const TrackedVehicle& record) {
            const auto& v = record.vehicle;
            // Fixed world end beyond all views before the race finish. A camera
            // exit can retire a body earlier, never later than this boundary.
            if (v.forwardPosition - v.length / 2 >= trafficWorldEnd(config_)) return true;
            if (visible(v, position)) return false;
            const bool approaching = v.forwardPosition < position ? v.speed > speed : v.speed < speed;
            return record.entered || !approaching;
        });
    }
    rebuildSnapshot();
    auditSafety();
    const bool reviewing=game && config_.encounterPlannerEnabled && time_ >= nextEncounterReview_;
    if (reviewing) {
        nextEncounterReview_ = time_ + config_.encounterReviewInterval;
        if(config_.constrainedVehicleSelection) selectEncounters(*game);
        else {
        encounter_.status = "EXISTING OPPORTUNITY";
        if (!hasUpcomingEncounter(*game, active_)) {
            encounter_.status = "WAITING FOR SAFE GAP";
            for (const auto& candidate : planEncounters(*game, active_)) {
                auto& lane = lanes_[candidate.lane == Lane::Left ? 0 : 1];
                const auto& spec = config_.vehicleSpecs[static_cast<std::size_t>(candidate.type)];
                Vehicle v{nextId_, candidate.type, candidate.lane, candidate.position, spec.speed, spec.length, 0};
                if (!checkTrafficAdmission(v, active_, config_).allowed) continue;
                // Replace only an unspawned background request; never move a car.
                lane.pending = Request{candidate.type, nextId_++, time_, candidate.position};
                encounter_ = {candidate.serviceContinuation ? "SERVICE CONTINUATION" : "PLANNED CONNECTION",
                              v.id, time_ + candidate.catchTime, candidate.serviceContinuation};
                break;
            }
        }
        }
    }
    for (std::size_t index = 0; index < lanes_.size(); ++index) {
        auto& scheduler = lanes_[index];
        const Lane lane = index == 0 ? Lane::Left : Lane::Right;
        if (!(game && config_.constrainedVehicleSelection && config_.encounterPlannerEnabled) && !scheduler.pending && time_ >= scheduler.nextSpawnTime) {
            const auto type = drawType(scheduler,lane);
            if(type) scheduler.pending = Request{*type, nextId_++, time_, {}};
            scheduler.nextSpawnTime = time_ + config_.laneMinimumSpawnIntervals[index] + randomUnit(scheduler) *
                (config_.laneMaximumSpawnIntervals[index] - config_.laneMinimumSpawnIntervals[index]);
        }
        scheduler.debug.blocked = false;
        scheduler.debug.blockReason = "CLEAR";
        scheduler.debug.plannedEncounter = false;
        if(game && config_.constrainedVehicleSelection && !scheduler.pending && time_>=scheduler.nextSpawnTime) {
            scheduler.debug.blocked=true; scheduler.debug.blockReason="NO LEGAL ENCOUNTER";
        }
        if (scheduler.pending) {
            const auto request = *scheduler.pending;
            scheduler.debug.pendingKind=request.kind;
            const auto& spec = config_.vehicleSpecs[static_cast<std::size_t>(request.type)];
            const double relative = spec.speed >= speed ? -config_.visibleBehindDistance - spec.length / 2 - config_.trafficEntryMargin
                                                       : config_.visibleAheadDistance + spec.length / 2 + config_.trafficEntryMargin;
            const double center = request.plannedPosition ? *request.plannedPosition + spec.speed * (time_ - request.time) : position + relative;
            Vehicle v{request.id, request.type, lane, center, spec.speed, spec.length, time_ - request.time};
            scheduler.debug.plannedEncounter = request.plannedPosition.has_value();
            if (request.plannedPosition && visible(v, position)) {
                scheduler.pending.reset();
                scheduler.debug.blockReason = "REPLAN / WINDOW PASSED";
            } else {
                const auto safety = checkTrafficAdmission(v, active_, config_);
                if (safety.allowed && v.forwardPosition - v.length / 2 <= trafficWorldEnd(config_)) {
                    scheduler.active.push_back({v, false, time_, v.forwardPosition});
                    active_.push_back(v);
                    scheduler.debug.lastSpawnedType=v.type;
                    ++scheduler.debug.spawnCounts[static_cast<std::size_t>(v.type)];
                    ++typeCounts_[static_cast<std::size_t>(v.type)];
                    scheduler.pending.reset();
                } else {
                    scheduler.debug.blocked = true;
                    scheduler.debug.blockReason = safety.allowed ? "BEYOND WORLD END" : safety.reason;
                }
            }
        }
        scheduler.debug.pendingVehicle = scheduler.pending ? std::optional{scheduler.pending->type} : std::nullopt;
        scheduler.debug.nextSpawnTime = scheduler.nextSpawnTime;
    }
    rebuildSnapshot();
    if(game && reviewing) updateCandidateDebug(*game);
}
}
