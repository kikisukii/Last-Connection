#include "live_traffic.hpp"
#include "time_attack.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void require(bool ok,const char* message) { ++checks; if(!ok) throw std::runtime_error(message); }
bool near(double a,double b) { return std::abs(a-b)<1e-8; }
void bikeToCenter(lc::Game& g) {
    const lc::Vehicle bike{50,lc::VehicleType::Bike,lc::Lane::Left,g.player().forwardPosition(),20,3,0};
    g.requestTransfer(-1,std::span<const lc::Vehicle>(&bike,1)); g.update(g.player().config().lateralTransferDuration);
    require(g.attachedVehicle()==50,"Bike fixture failed");
    g.requestTransfer(1,{});g.update(g.player().config().lateralTransferDuration);
}
}
int main() {
    try {
        lc::GameplayConfig c;
        for(std::size_t t=0;t<4;++t) {
            require(c.vehicleSpecs[t].maxBoardingRelativeSpeed==std::array<double,4>{9,19,27,29}[t],"wrong V0.3 threshold");
            require(c.vehicleSpecs[t].rideServiceDistance()==std::array<double,4>{80,96,90,90}[t],"service tuning changed");
            require(c.vehicleSpecs[t].catchZoneLength==std::array<double,4>{2,7,3.5,7}[t],"Catch Zone changed");
        }
        require(c.lateralTransferDuration==.40 && c.normalRunSpeed==12 && c.normalDeceleration==15 && c.missPenalty==5 && c.finishDistance==1200 && c.deadline==90,"protected rules changed");
        for(int t=1;t<4;++t) {
            lc::Game direct(c);
            const auto& s=c.vehicleSpecs[t];
            const lc::Vehicle target{100+static_cast<std::size_t>(t),static_cast<lc::VehicleType>(t),lc::Lane::Right,0,s.speed,s.length,0};
            direct.requestTransfer(1,std::span<const lc::Vehicle>(&target,1)); direct.update(c.lateralTransferDuration);
            const auto& d=direct.lastBoardingDebug();
            require(!direct.attachedVehicle() && !d.diagnosticPrediction.speedAllowed && d.diagnosticPrediction.intersects && d.outcome==lc::TransferOutcome::SpeedRejected,"Run skipped speed progression");
        }
        lc::Game positionMiss(c);bikeToCenter(positionMiss);
        const double begin=positionMiss.player().forwardPosition();
        const lc::Vehicle farBus{80,lc::VehicleType::Bus,lc::Lane::Right,begin+100,32,10,0};
        positionMiss.requestTransfer(1,std::span<const lc::Vehicle>(&farBus,1)); positionMiss.update(c.lateralTransferDuration);
        const auto diagnostic=positionMiss.lastBoardingDebug();
        require(!positionMiss.transferDebug().targetAtStart && diagnostic.diagnosticTarget->id==80,"diagnostic fallback changed actual target selection");
        require(!diagnostic.intersectionTime && !diagnostic.diagnosticPrediction.intersects && diagnostic.outcome==lc::TransferOutcome::Miss,"no intersection must leave actual speed check N/A");
        require(diagnostic.allowedDifference==19 && near(diagnostic.endSpeed,12),"debug unmounted endpoint incorrect");
        require(near(diagnostic.endPosition-begin,74.0/15) && near(diagnostic.zoneEndRear-diagnostic.zoneStartRear,12.8),"debug trajectory ranges incorrect");
        positionMiss.requestTransfer(-1,{});positionMiss.update(c.lateralTransferDuration);
        require(positionMiss.lastBoardingDebug().diagnosticTarget->id==80 && positionMiss.lastBoardingDebug().outcome==lc::TransferOutcome::Miss,"CENTER return erased failure diagnostics");
        lc::Game forgiving(c);bikeToCenter(forgiving);forgiving.update(.04);
        const lc::Vehicle bus{81,lc::VehicleType::Bus,lc::Lane::Right,forgiving.player().forwardPosition(),32,10,0};
        forgiving.requestTransfer(1,std::span<const lc::Vehicle>(&bus,1));forgiving.update(c.lateralTransferDuration);
        require(forgiving.attachedVehicle()==81 && near(forgiving.lastBoardingDebug().absoluteDifferenceAtIntersection,18.6),"Bus prototype margin did not allow 18.6 event difference");
        std::cout<<"PASS: 9/19/27/29, no direct Run skips, independent speed/position diagnostics, retained snapshot, Bus margin\n";

        auto p=c;p.laneFirstSpawnTimes={0,0};
        lc::Game running(p);lc::LiveTraffic traffic(p,17), replay(p,17);
        bool sawPrepared=false,sawImmediate=false;
        for(int tick=0;tick<1800;++tick) {
            running.update(c.fixedStepSeconds);traffic.update(c.fixedStepSeconds,running);replay.update(c.fixedStepSeconds,running);
            require(traffic.current().size()==replay.current().size(),"seed replay population mismatch");
            for(std::size_t i=0;i<traffic.current().size();++i) {
                const auto& v=traffic.current()[i];
                require(v.id==replay.current()[i].id && v.type==replay.current()[i].type && v.forwardPosition==replay.current()[i].forwardPosition,"seed replay changed selection");
                if(v.type==lc::VehicleType::Bus && lc::isPreparedCandidate(running,v,traffic.current())) {
                    sawPrepared=true;
                    require(!lc::isUpcomingCandidate(running,v),"Prepared Bus was also Immediate at Run12");
                    lc::Game attempt=running;attempt.requestTransfer(static_cast<int>(v.lane),traffic.current());attempt.update(c.lateralTransferDuration);
                    require(attempt.attachedVehicle()!=v.id,"Prepared Bus boarded directly from Run");
                }
                if(v.type==lc::VehicleType::Bike && lc::isUpcomingCandidate(running,v)) sawImmediate=true;
            }
        }
        require(sawImmediate && sawPrepared,"Run -> Bike -> prepared Bus was not physically planned");
        std::size_t spawned=0;for(auto n:traffic.typeCounts()) spawned+=n;
        require(spawned>4,"type-history constraints stalled all no-input traffic");
        require(traffic.encounterDebug().immediateCandidates[1]==0,"debug labels Bus Immediate at Run12");
        std::cout<<"PASS: actual Bike immediate + Bus prepared, seeded replay, no Prepared bypass\n";

        auto reservations=c;reservations.encounterPlannerEnabled=false;reservations.vehicleTypeWeights={0,0,1,0};reservations.laneFirstSpawnTimes={0,0};
        const lc::Vehicle slow{500,lc::VehicleType::Bike,lc::Lane::Left,0,20,3,0};
        lc::LiveTraffic pending(reservations,9,std::span<const lc::Vehicle>(&slow,1));
        pending.update(0,0,12);
        require(pending.laneDebug(lc::Lane::Left).pendingVehicle==lc::VehicleType::Taxi,"unsafe Taxi was not retained as pending");
        require(!pending.typeAvailable(lc::Lane::Right,lc::VehicleType::Taxi),"pending failed to reserve type across lanes");
        require(pending.laneDebug(lc::Lane::Left).spawnCounts[2]==0 && pending.laneDebug(lc::Lane::Right).spawnCounts[2]==0,"pending counted as actual spawn");
        require(!pending.typeAvailable(lc::Lane::Left,lc::VehicleType::Bike),"same-lane history was not applied");
        pending.update(3,36,12);
        require(pending.typeAvailable(lc::Lane::Right,lc::VehicleType::Bike),"retired active type reservation was not released");
        lc::TimeAttack reset(c,0);reset.update(2);reset.restart(1);
        for(auto lane:{lc::Lane::Left,lc::Lane::Right}) {
            const auto& d=reset.traffic().laneDebug(lane);
            require(!d.lastSpawnedType && d.spawnCounts==std::array<std::size_t,4>{},"retry did not reset history/statistics");
        }
        std::cout<<"PASS: pending reservation, lifecycle release, admission-only counts, retry reset\n";
        std::cout<<"PASS: "<<checks<<" V0.3 selection/debug checks\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
