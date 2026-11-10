#include "encounter_planner.hpp"
#include "forward_motion.hpp"
#include <algorithm>
#include <cmath>

namespace lc {
namespace {
struct Forecast { Game start; double delay; bool continuation; };
std::optional<Forecast> forecast(const Game& game, double encounterTime) {
    const auto& c = game.player().config();
    const double startDelay = encounterTime - c.lateralTransferDuration;
    if (startDelay < 0) return {};
    Game future = game;
    double elapsed = 0;
    if (future.player().transferring()) {
        elapsed = future.player().transferRemainingSeconds();
        if (elapsed > startDelay) return {};
        future.update(elapsed);
    }
    bool continuation = false;
    if (future.currentVehicle()) {
        const double serviceTime = future.remainingRideDistance() / future.player().forwardSpeed();
        // Express forecasts its natural service end. Other rides may still
        // upgrade before expiry; merely entering the warning period must not
        // suppress Taxi -> Express opportunities.
        const double hold = startDelay - elapsed - c.lateralTransferDuration;
        if (serviceTime <= c.serviceContinuationLeadTime &&
            (future.currentVehicle()->type == VehicleType::Express || hold >= serviceTime)) {
            const double untilCenter = serviceTime + c.lateralTransferDuration;
            if (elapsed + untilCenter > startDelay + 1e-8) return {};
            future.update(untilCenter);
            elapsed += untilCenter;
            continuation = true;
        } else {
            if (hold < 0) return {};
            future.update(hold);
            if (!future.requestTransfer(-static_cast<int>(future.player().lane()), {})) return {};
            future.update(c.lateralTransferDuration);
            elapsed = startDelay;
        }
    } else if (future.player().lane() != Lane::Center) {
        if (elapsed + c.lateralTransferDuration > startDelay + 1e-8) return {};
        future.requestTransfer(-static_cast<int>(future.player().lane()), {});
        future.update(c.lateralTransferDuration);
        elapsed += c.lateralTransferDuration;
    }
    if (elapsed < startDelay) future.update(startDelay - elapsed);
    if (future.player().transferring() || future.player().lane() != Lane::Center) return {};
    return Forecast{future, startDelay, continuation};
}
std::vector<double> desiredTimes(const Game& game, bool immediate) {
    const auto& c = game.player().config();
    std::vector<double> times;
    for (double t = c.encounterTimeMin; t <= c.encounterTimeMax + 1e-8; t += c.encounterReviewInterval) times.push_back(t);
    if (game.currentVehicle()) {
        const double end = game.remainingRideDistance() / game.player().forwardSpeed();
        if (end <= c.serviceContinuationLeadTime) {
            const double earliest = end + 2 * c.lateralTransferDuration;
            for (double extra : {0.0, c.lateralTransferDuration, 2*c.lateralTransferDuration}) times.push_back(earliest + extra);
        }
    }
    // A surprise high-speed miss can require an earlier recovery connection:
    // waiting 1.5 seconds may make every reachable type unsafe after decay.
    for (double t = c.lateralTransferDuration; t < c.encounterTimeMin; t += c.encounterReviewInterval)
        if (immediate || t >= c.encounterVisibilityLeadTime) times.push_back(t);
    return times;
}
CatchWindow windowFor(const Forecast& f, const Vehicle& vehicle, const GameplayConfig& c) {
    auto spec = c.vehicleSpecs[static_cast<std::size_t>(vehicle.type)];
    spec.speed = vehicle.speed;
    return predictBoardingCatch(f.start.player().forwardPosition(), f.start.player().forwardSpeed(),
        vehicle.forwardPosition + vehicle.speed * f.delay, spec, c.lateralTransferDuration, c);
}
}
bool isUpcomingCandidate(const Game& game, const Vehicle& vehicle) {
    if (game.serviceConsumed(vehicle.id) || game.attachedVehicle()==vehicle.id) return false;
    for (double t:desiredTimes(game,true)) {
        const auto f=forecast(game,t);
        if(f && windowFor(*f,vehicle,game.player().config()).canCatch()) return true;
    }
    return false;
}
namespace {
struct Progression { Game future; double elapsed; Vehicle first; };
std::vector<Progression> progressions(const Game& game, std::span<const Vehicle> active) {
    std::vector<Progression> results;
    const auto& c=game.player().config();
    for(const auto& v:active) {
        if(game.serviceConsumed(v.id) || game.attachedVehicle()==v.id || v.speed<=game.player().forwardSpeed()) continue;
        for(double t:desiredTimes(game,true)) {
            const auto f=forecast(game,t);
            if(!f || !windowFor(*f,v,c).canCatch()) continue;
            Game future=f->start;
            auto atStart=v; atStart.forwardPosition+=v.speed*f->delay;
            future.requestTransfer(static_cast<int>(v.lane),std::span<const Vehicle>(&atStart,1));
            future.update(c.lateralTransferDuration);
            if(future.attachedVehicle()==v.id) results.push_back({future,t,v});
            break;
        }
    }
    return results;
}
bool needsProgression(const Game& game, VehicleType type) {
    const auto& c=game.player().config();
    return !predictBoardingCatch(0,game.player().forwardSpeed(),0,c.vehicleSpecs[static_cast<std::size_t>(type)],c.lateralTransferDuration,c).speedAllowed;
}
}
bool isPreparedCandidate(const Game& game, const Vehicle& target, std::span<const Vehicle> active) {
    if(game.serviceConsumed(target.id) || !needsProgression(game,target.type)) return false;
    for(const auto& p:progressions(game,active)) {
        if(static_cast<int>(target.type)!=static_cast<int>(p.first.type)+1) continue;
        auto futureTarget=target; futureTarget.forwardPosition+=target.speed*p.elapsed;
        if(isUpcomingCandidate(p.future,futureTarget)) return true;
    }
    return false;
}
std::vector<EncounterCandidate> planPreparedEncounters(const Game& game, std::span<const Vehicle> active) {
    std::vector<EncounterCandidate> results;
    const auto& c=game.player().config();
    for(const auto& p:progressions(game,active)) {
        for(auto candidate:planEncounters(p.future,{})) {
            if(static_cast<int>(candidate.type)!=static_cast<int>(p.first.type)+1 || !needsProgression(game,candidate.type)) continue;
            const auto& spec=c.vehicleSpecs[static_cast<std::size_t>(candidate.type)];
            candidate.position-=spec.speed*p.elapsed;
            const double relative=candidate.position-game.player().forwardPosition();
            if(relative+spec.length/2> -c.visibleBehindDistance-c.trafficEntryMargin &&
               relative-spec.length/2< c.visibleAheadDistance+c.trafficEntryMargin) continue;
            candidate.transferStart+=p.elapsed; candidate.catchTime+=p.elapsed;
            candidate.firstVisibleTime+=p.elapsed;
            candidate.kind=OpportunityKind::Prepared; candidate.prerequisiteId=p.first.id;
            results.push_back(candidate);
        }
    }
    return results;
}
bool hasUpcomingEncounter(const Game& game, std::span<const Vehicle> active) {
    const auto& c = game.player().config();
    for (double time : desiredTimes(game, true)) {
        const auto f = forecast(game, time);
        if (!f) continue;
        for (const auto& vehicle : active) {
            if (game.serviceConsumed(vehicle.id)) continue;
            if (game.attachedVehicle() == vehicle.id) continue;
            if (game.currentVehicle() && vehicle.lane == game.currentVehicle()->lane) continue;
            if (game.currentVehicle() && game.currentVehicle()->type != VehicleType::Express &&
                !f->continuation &&
                vehicle.speed <= game.player().forwardSpeed()) continue;
            if (windowFor(*f, vehicle, c).canCatch()) return true;
        }
    }
    return false;
}
std::vector<EncounterCandidate> planEncounters(const Game& game, std::span<const Vehicle> active) {
    const auto& c = game.player().config();
    std::vector<EncounterCandidate> results;
    if (hasUpcomingEncounter(game, active)) return results;
    std::array<int, 4> types{{0, 1, 2, 3}};
    if (game.currentVehicle()) {
        const int current = static_cast<int>(game.currentVehicle()->type);
        const int preferred = current == 3 ? 2 : current + 1;
        std::rotate(types.begin(), std::find(types.begin(), types.end(), preferred), types.end());
    }
    for (double time : desiredTimes(game, false)) {
        const auto f = forecast(game, time);
        if (!f) continue;
        Game end = f->start;
        end.update(c.lateralTransferDuration);
        for (int index : types) {
            const auto& spec = c.vehicleSpecs[static_cast<std::size_t>(index)];
            // Return viable recovery alternatives too. LiveTraffic applies
            // upgrade priority after type reservations and TTC filtering.
            if (c.vehicleTypeWeights[static_cast<std::size_t>(index)] == 0) continue;
            std::vector<double> positions{end.player().forwardPosition() - spec.speed * time};
            // Preserve the original alignment when it passes. If the new
            // first-intersection gate rejects it, also forecast earlier zone
            // entries. No type-specific speed or progression exceptions.
            const Vehicle original{0,static_cast<VehicleType>(index),Lane::Left,positions.front(),spec.speed,spec.length,0};
            if (!windowFor(*f,original,c).canCatch()) {
                for (double fraction : {0.0, .25, .5, .75, 1.0}) {
                    const double hit=fraction*c.lateralTransferDuration;
                    const auto motion=advanceForward(f->start.player().forwardSpeed(),hit,c);
                    const double edge=spec.speed>=motion.speed ? -spec.catchZoneLength/2 : spec.catchZoneLength/2;
                    positions.push_back(f->start.player().forwardPosition()+motion.distance+edge-spec.speed*(f->delay+hit));
                }
            }
            for (double position : positions) {
                const double relative = position - game.player().forwardPosition();
                const bool behind = relative + spec.length / 2 <= -c.visibleBehindDistance - c.trafficEntryMargin;
                const bool ahead = relative - spec.length / 2 >= c.visibleAheadDistance + c.trafficEntryMargin;
                if (!behind && !ahead) continue; // never materialize inside the canvas
                // Conservative preview lead under the currently observed relative
                // velocity; actual crossing/eligibility is checked on the full model.
                const double approach = behind ? spec.speed - game.player().forwardSpeed() : game.player().forwardSpeed() - spec.speed;
                if (approach <= 0) continue;
                const double edgeGap = behind ? -c.visibleBehindDistance - (relative + spec.length / 2)
                                              : relative - spec.length / 2 - c.visibleAheadDistance;
                const double visibleTime = edgeGap / approach;
                for (Lane lane : {Lane::Left, Lane::Right}) {
                    if (game.currentVehicle() && lane == game.currentVehicle()->lane) continue;
                    Vehicle proposed{0, static_cast<VehicleType>(index), lane, position, spec.speed, spec.length, 0};
                    const auto window = windowFor(*f, proposed, c);
                    const double hit = f->delay + window.enterTime;
                    if (!window.canCatch() || hit - visibleTime < c.encounterVisibilityLeadTime) continue;
                    results.push_back({proposed.type, lane, position, f->delay, hit, visibleTime, f->continuation});
                }
            }
        }
    }
    return results;
}
}
