#include "traffic_safety.hpp"
#include "live_traffic.hpp"
#include "game.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks = 0;
void require(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
bool near(double a, double b) { return std::abs(a-b)<1e-8; }
void advance(lc::Vehicle& v, double dt) { v.forwardPosition += v.speed*dt; }
}
int main() {
    try {
        lc::GameplayConfig c;
        // Fixed historical trajectory fixture; V0.3 defaults covered separately.
        c.lateralTransferDuration = .25; c.normalDeceleration = 10;
        // Preserve the safety-only fixture; V0.2 behavior has its own tests.
        c.constrainedVehicleSelection=false;
        for(std::size_t i=0;i<4;++i) c.vehicleSpecs[i].maxBoardingRelativeSpeed=std::array<double,4>{8,17,18,20}[i];
        require(c.vehicleSpecs[0].maxBoardingRelativeSpeed==8 && c.vehicleSpecs[1].maxBoardingRelativeSpeed==17 &&
                c.vehicleSpecs[2].maxBoardingRelativeSpeed==18 && c.vehicleSpecs[3].maxBoardingRelativeSpeed==20,
                "deferred V0.2 thresholds leaked into safety task");
        lc::Vehicle bus{10,lc::VehicleType::Bus,lc::Lane::Left,100,32,10,0};
        lc::Vehicle taxi{11,lc::VehicleType::Taxi,lc::Lane::Left,0,45,5,0};
        auto unsafe=lc::inspectTrafficPair(taxi,bus,c);
        require(unsafe.ttc>1 && !unsafe.allowed, "large TTC alone incorrectly admitted collision course");
        require(near(unsafe.gap,92.5) && near(unsafe.ttc,92.5/13), "TTC is not bumper gap / closing speed");
        taxi.forwardPosition=bus.forwardPosition-7.5-6.5;
        require(!lc::inspectTrafficPair(taxi,bus,c).allowed, "short unsafe TTC accepted");
        bus.forwardPosition=lc::trafficWorldEnd(c)+bus.length/2-bus.speed*.1;
        taxi.forwardPosition=bus.forwardPosition-7.5-6.5;
        const auto shortSafe=lc::inspectTrafficPair(taxi,bus,c);
        require(shortSafe.allowed && near(shortSafe.ttc,.5) && near(shortSafe.frontExitTime,.1), "safe short TTC rejected by absolute threshold");
        require(lc::checkTrafficAdmission(bus,std::vector<lc::Vehicle>{taxi},c).allowed, "candidate-as-leader check differs");
        auto otherLane=taxi; otherLane.lane=lc::Lane::Right; otherLane.forwardPosition=bus.forwardPosition;
        require(lc::checkTrafficAdmission(otherLane,std::vector<lc::Vehicle>{bus},c).allowed, "opposite lane blocked admission");
        auto equal=taxi; equal.speed=bus.speed;
        require(lc::inspectTrafficPair(equal,bus,c).allowed && std::isinf(lc::inspectTrafficPair(equal,bus,c).ttc), "equal-speed pair incorrectly catches up");
        std::cout<<"PASS: short/long unsafe TTC rejected; 0.5s TTC with 0.1s exit allowed; candidate direction and lanes\n";

        // Keep both bodies in the fixed effective region until the guaranteed
        // leader exit. Test continuous endpoints under different step partitions.
        for(double dt : {1.0/120, .001, .037}) {
            lc::Vehicle front{1,lc::VehicleType::Bus,lc::Lane::Left,1100,32,10,0};
            lc::Vehicle rear{2,lc::VehicleType::Taxi,lc::Lane::Left,892.5,45,5,0};
            double elapsed=0;
            while(front.forwardPosition-front.length/2<lc::trafficWorldEnd(c)) {
                const auto s=lc::inspectTrafficPair(rear,front,c);
                require(s.allowed && s.gap>=c.vehicleMinimumGap-1e-8, "retained same-lane vehicles converge unsafely");
                const double step=std::min(dt,s.frontExitTime);
                if(step<1e-10) break;
                advance(front,step); advance(rear,step); elapsed+=step;
                require(front.speed==32 && rear.speed==45, "safety modified generated velocity");
            }
            require(elapsed>3.8 && elapsed<4, "leader did not leave at predicted world time");
            require(lc::checkTrafficAdmission(rear,{},c).allowed, "retired leader still blocks follower");
        }
        std::cout<<"PASS: faster rear keeps clearance until leader retires, across three integration partitions\n";

        // A real Game mounts Bike, leaves it, then mounts Bus. Two independent
        // traffic bodies advance on the same clock; boarding cannot change TTC.
        auto mountedConfig=c; mountedConfig.finishDistance=200;
        mountedConfig.encounterPlannerEnabled=false; mountedConfig.laneFirstSpawnTimes={10000,10000};
        lc::Game player(mountedConfig); player.update(15); // world position 180
        const std::vector<lc::Vehicle> pair{
            {10,lc::VehicleType::Bus,lc::Lane::Left,198,32,10,0},
            {11,lc::VehicleType::Taxi,lc::Lane::Left,172,45,5,0}};
        lc::LiveTraffic flow(mountedConfig,5,pair);
        const double initialTTC=lc::inspectTrafficPair(pair[0],pair[1],mountedConfig).ttc;
        player.requestTransfer(-1,std::vector<lc::Vehicle>{{77,lc::VehicleType::Bike,lc::Lane::Left,180,20,3,0}});
        player.update(.25); flow.update(.25,player);
        require(player.player().forwardSpeed()==20 && flow.current().size()==2, "Bike acceleration fixture failed");
        player.requestTransfer(1,{}); player.update(.25); flow.update(.25,player);
        require(flow.current().size()==2, "comparison bodies exited too early");
        const auto before=lc::inspectTrafficPair(flow.current()[0],flow.current()[1],mountedConfig);
        require(near(before.ttc,initialTTC-.5), "TTC did not decrease solely by elapsed time");
        player.requestTransfer(1,std::vector<lc::Vehicle>{{78,lc::VehicleType::Bus,lc::Lane::Right,player.player().forwardPosition(),32,10,0}});
        player.update(0); flow.update(0,player);
        require(player.player().forwardSpeed()==32, "Bus did not accelerate player");
        const auto after=lc::inspectTrafficPair(flow.current()[0],flow.current()[1],mountedConfig);
        require(near(before.ttc,after.ttc) && near(before.frontExitTime,after.frontExitTime), "player acceleration changed TTC/horizon");
        for(int i=0;i<120;++i) {
            player.update(c.fixedStepSeconds); flow.update(c.fixedStepSeconds,player);
            for(const auto& v:flow.current()) {
                const auto& original=pair[v.id==10?0:1];
                require(v.speed==original.speed && near(v.forwardPosition,original.forwardPosition+v.speed*(.5+(i+1)*c.fixedStepSeconds)), "mounted-player traffic altered velocity/position");
            }
        }
        require(!flow.safetyViolation(), "valid mounted-camera scenario raised safety error");
        std::cout<<"PASS: actual Run -> Bike -> Bus acceleration leaves pair TTC unchanged at the catch instant\n";

        auto laneConfig=c; laneConfig.encounterPlannerEnabled=false;
        laneConfig.vehicleTypeWeights={0,0,1,0}; laneConfig.laneFirstSpawnTimes={0,0};
        const lc::Vehicle slow{100,lc::VehicleType::Bus,lc::Lane::Left,0,32,10,0};
        lc::LiveTraffic blocked(laneConfig,91,std::vector<lc::Vehicle>{slow}), clear(laneConfig,91);
        bool sawBlock=false, sawRight=false, spawnedAfterExit=false;
        for(int i=0;i<360;++i) {
            const double p=12*(i+1)*c.fixedStepSeconds;
            blocked.update(c.fixedStepSeconds,p,12); clear.update(c.fixedStepSeconds,p,12);
            sawBlock=sawBlock||blocked.laneDebug(lc::Lane::Left).blocked;
            std::vector<double> a,b;
            bool leaderPresent=false, leftTaxi=false;
            for(const auto& v:blocked.current()) {
                if(v.lane==lc::Lane::Right) a.push_back(v.forwardPosition);
                if(v.id==100) leaderPresent=true;
                if(v.lane==lc::Lane::Left && v.type==lc::VehicleType::Taxi) leftTaxi=true;
            }
            for(const auto& v:clear.current()) if(v.lane==lc::Lane::Right) b.push_back(v.forwardPosition);
            require(a==b && blocked.laneDebug(lc::Lane::Right).nextSpawnTime==clear.laneDebug(lc::Lane::Right).nextSpawnTime, "left TTC changed right scheduling/RNG");
            require(!leaderPresent || !leftTaxi, "pending fast rear admitted before unsafe leader exited");
            sawRight=sawRight||!a.empty();
            spawnedAfterExit=spawnedAfterExit||(!leaderPresent&&leftTaxi);
        }
        require(sawBlock&&sawRight&&spawnedAfterExit, "blocked-lane retirement/release scenario not exercised");
        std::cout<<"PASS: blocked left does not change right; pending left admitted after leader viewport exit\n";

        // Deliberately corrupted existing traffic: inspect/log, never repair speed.
        std::vector<lc::TrafficRecord> corrupted{
            {{7,lc::VehicleType::Bus,lc::Lane::Left,100,32,10,0},1.25,60},
            {{8,lc::VehicleType::Taxi,lc::Lane::Left,0,45,5,0},2.5,-112.5}};
        auto violation=lc::findTrafficViolation(corrupted,c,5);
        require(violation.has_value() && violation->safety.gap>0, "future collision course not reported");
        const auto message=lc::formatTrafficViolation(*violation);
        for(const auto* field : {"TRAFFIC SAFETY VIOLATION","Leader ID=7","Leader speed=32","Follower ID=8","Follower speed=45","Gap=","TTC=","Leader Spawn time=1.25","Follower Spawn position=-112.5"})
            require(message.find(field)!=std::string::npos,"missing diagnostic evidence");
        corrupted[1].vehicle.forwardPosition=96;
        violation=lc::findTrafficViolation(corrupted,c,5);
        require(violation && violation->safety.gap<0 && violation->safety.ttc==0 && std::string(violation->safety.reason)=="ACTUAL OVERLAP", "actual overlap not diagnosed");
        require(corrupted[0].vehicle.speed==32 && corrupted[1].vehicle.speed==45 && corrupted[1].vehicle.forwardPosition==96, "audit mutated corrupt traffic to conceal bug");
        bool runtimeReported=false;
        try { lc::LiveTraffic invalid(c,7,std::vector<lc::Vehicle>{corrupted[0].vehicle,corrupted[1].vehicle}); }
        catch(const std::logic_error& e) { runtimeReported=std::string(e.what()).find("TRAFFIC SAFETY VIOLATION")!=std::string::npos; }
        require(runtimeReported,"runtime audit did not surface structured failure");
        std::cout<<"PASS: injected future collision and overlap emit full diagnostics without changing speeds\n";
        std::cout<<"PASS: "<<checks<<" TTC safety checks\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
