#include "game.hpp"
#include "forward_motion.hpp"
#include "time_attack.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a-b) < 1e-7; }
lc::Vehicle target(const lc::Game& game, int type, std::size_t id, lc::Lane lane, double hit) {
    const auto& c = game.player().config();
    const auto& spec = c.vehicleSpecs[type];
    const double position = game.player().forwardPosition() +
        lc::advanceForward(game.player().forwardSpeed(), hit, c).distance - spec.speed*hit - spec.catchZoneLength/2;
    return {id, static_cast<lc::VehicleType>(type), lane, position, spec.speed, spec.length, 0};
}
// Reach each vehicle through the existing natural progression. Stop at its
// actual intersection, BEFORE lateral completion, to expose the timer origin.
void reachIntersection(lc::Game& game, int last) {
    const auto& c = game.player().config();
    for (int type=0; type<=last; ++type) {
        if (type) {
            check(game.requestTransfer(-static_cast<int>(game.player().lane()), {}), "departure rejected");
            game.update(c.lateralTransferDuration);
        }
        const double hit = type==1 ? .04 : .1;
        const auto v = target(game, type, 100+type, type%2 ? lc::Lane::Right : lc::Lane::Left, hit);
        check(game.requestTransfer(static_cast<int>(v.lane), {&v,1}), "boarding request rejected");
        game.update(hit);
        check(game.attachedVehicle()==v.id, "natural progression catch failed");
        if (type<last) game.update(c.lateralTransferDuration-hit);
    }
}
void runPartitioned(lc::Game& game, double seconds, double dt) {
    while (seconds > 1e-12) {
        const double step = std::min(seconds, dt);
        game.update(step);
        seconds -= step;
    }
}
}

int main() {
    try {
        const lc::GameplayConfig c;
        const std::array<double,4> durations{4,3,2,1.5}, distances{80,96,90,90};
        for (int type=0; type<4; ++type) {
            const auto& spec = c.vehicleSpecs[type];
            check(spec.rideServiceDuration==durations[type] && spec.rideServiceDistance()==distances[type], "wrong prototype service tuning");
            for (double dt : {1.0/120, 1.0/60, .137}) {
                lc::Game game(c);
                reachIntersection(game, type);
                const auto ride = *game.currentVehicle();
                const double start = game.player().elapsedSeconds();
                const double origin = game.player().forwardPosition();
                check(game.player().transferring(), "fixture must catch before lateral completion");
                check(near(game.elapsedRideTime(),0) && near(game.currentRideDuration(),durations[type]) &&
                      near(game.remainingRideTime(),durations[type]), "timer did not start at actual catch");
                check(near(game.remainingRideDistance(),distances[type]), "service charged before actual catch");
                lc::Game singleStep = game;
                const double halfway = durations[type]/2;
                runPartitioned(game,halfway,dt);
                check(near(game.elapsedRideTime(),halfway) && near(game.remainingRideTime(),halfway) &&
                      near(game.remainingRideDistance(),spec.speed*halfway), "debug timer/distance disagree");
                runPartitioned(game,halfway-1e-5,dt);
                check(game.attachedVehicle()==ride.id && !game.serviceCompleted(ride.id), "service ended early");
                game.update(1e-5);
                check(near(game.player().elapsedSeconds()-start,durations[type]), "incorrect end time");
                check(!game.attachedVehicle() && !game.currentVehicle() && game.returningFromService(), "service did not release");
                check(near(game.player().forwardPosition()-origin,distances[type]), "incorrect service distance");
                check(near(game.player().forwardSpeed(),spec.speed), "service end reset speed or charged penalty");
                check(game.transferDebug().outcome==lc::TransferOutcome::ServiceEnd && game.transferDebug().penaltyApplied==0, "service end counted as miss");
                check(game.vehicleConsumed(ride.id) && game.serviceCompleted(ride.id), "completed ID not consumed");
                check(game.player().targetLane()==lc::Lane::Center, "service did not target CENTER");
                check(game.remainingRideTime()==0 && game.remainingRideDistance()==0 && game.elapsedRideTime()==0, "inactive debug retained stale ride");
                game.update(c.lateralTransferDuration);
                check(game.player().lane()==lc::Lane::Center && !game.player().transferring(), "CENTER return not completed");
                check(near(game.player().forwardSpeed(),spec.speed-6), "normal decay during return changed");
                check(game.transferDebug().penaltyApplied==0 && game.transferDebug().outcome==lc::TransferOutcome::ServiceEnd, "return applied miss penalty");
                singleStep.update(durations[type]+c.lateralTransferDuration);
                check(near(singleStep.player().forwardPosition(),game.player().forwardPosition()) &&
                      near(singleStep.player().forwardSpeed(),game.player().forwardSpeed()) &&
                      singleStep.player().lane()==game.player().lane(), "service end depends on update partition");
                lc::Game rejected = game;
                auto old = target(rejected,type,ride.id,ride.lane,0);
                rejected.requestTransfer(static_cast<int>(old.lane),{&old,1});
                check(!rejected.transferDebug().targetAtStart && rejected.transferDebug().excludedUsedVehicle==ride.id, "old vehicle remained a candidate");
                rejected.update(c.lateralTransferDuration);
                check(!rejected.attachedVehicle(), "completed vehicle boarded again");
                // An independently positioned NEW encounter is still catchable;
                // no cooldown or hard prohibition on consecutive types was added.
                auto next = target(game,type,200+type,ride.lane,0);
                check(game.requestTransfer(static_cast<int>(next.lane),{&next,1}), "new encounter blocked");
                game.update(0);
                check(game.attachedVehicle()==next.id && near(game.remainingRideTime(),durations[type]), "new ID did not receive its own service");
            }
            std::cout << lc::vehicleName(static_cast<lc::VehicleType>(type)) << " PASS: " << durations[type]
                      << "s / " << distances[type] << " units; exact end, retained speed, no miss, CENTER, consumed, next ID\n";
        }
        auto tuned = c;
        tuned.vehicleSpecs[0].rideServiceDuration = 1.25;
        check(tuned.vehicleSpecs[0].rideServiceDistance()==25, "distance not derived from duration");
        lc::Game shorter(tuned); reachIntersection(shorter,0);
        check(near(shorter.remainingRideTime(),1.25) && near(shorter.remainingRideDistance(),25), "runtime ignored duration tuning");
        shorter.update(1.25);
        check(shorter.serviceCompleted(100), "custom duration did not expire");
        check(lc::scoreRulesKey(tuned)!=lc::scoreRulesKey(c), "different service rules share a best-time key");
        tuned.vehicleSpecs[0].speed=24;
        check(tuned.vehicleSpecs[0].rideServiceDistance()==30, "derived distance cached obsolete speed");
        for (double invalid : {0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
            auto bad=c; bad.vehicleSpecs[0].rideServiceDuration=invalid;
            bool rejected=false;
            try { lc::Game game(bad); } catch(const std::invalid_argument&) { rejected=true; }
            check(rejected,"invalid duration accepted");
        }
        std::cout << "PASS: " << checks << " ride duration checks\n";
    } catch(const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
