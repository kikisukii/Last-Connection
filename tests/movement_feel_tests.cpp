#include "game.hpp"
#include "forward_motion.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
int checks = 0;
void check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
bool near(double a, double b) { return std::abs(a-b) < 1e-8; }
lc::Vehicle target(const lc::Game& game, lc::VehicleType type, std::size_t id, double hit, lc::Lane lane) {
    const auto& c=game.player().config(); const auto& s=c.vehicleSpecs[static_cast<int>(type)];
    // Rearward car's front catch boundary reaches the unmounted player at hit.
    const double position=game.player().forwardPosition()+lc::advanceForward(game.player().forwardSpeed(),hit,c).distance-s.speed*hit-s.catchZoneLength/2;
    return {id,type,lane,position,s.speed,s.length,0};
}
void attempt(lc::Game& game, lc::Vehicle v) {
    check(game.requestTransfer(static_cast<int>(v.lane),std::span<const lc::Vehicle>(&v,1)),"request rejected");
}
void board(lc::Game& game, lc::VehicleType type, std::size_t id, lc::Lane lane, double hit=0) {
    attempt(game,target(game,type,id,hit,lane)); game.update(game.player().config().lateralTransferDuration);
    check(game.attachedVehicle()==id,"ladder catch failed");
}
void leave(lc::Game& game) {
    check(game.requestTransfer(-static_cast<int>(game.player().lane()),{}),"departure rejected");
    game.update(game.player().config().lateralTransferDuration);
}
void bikeCenter(lc::Game& game) { board(game,lc::VehicleType::Bike,1,lc::Lane::Left); leave(game); }
void reach(lc::Game& game,int last) {
    for(int i=0;i<=last;++i) {
        if(i) leave(game);
        board(game,static_cast<lc::VehicleType>(i),10+i,i%2?lc::Lane::Right:lc::Lane::Left,i==1?.04:.2);
    }
}
}
int main() {
    try {
        const lc::GameplayConfig c;
        check(c.lateralTransferDuration==.4 && c.normalDeceleration==15,"V0.3 movement parameters");
        check(c.normalRunSpeed==12 && c.recoveryAcceleration==5 && c.missPenalty==5 && c.deadline==90 && c.finishDistance==1200,"protected parameters changed");
        for(int i=0;i<4;++i) {
            const auto& s=c.vehicleSpecs[i];
            check(s.speed==std::array<double,4>{20,32,45,60}[i] && s.length==std::array<double,4>{3,10,5,15}[i] &&
                  s.catchZoneLength==std::array<double,4>{2,7,3.5,7}[i] && s.rideServiceDistance==std::array<double,4>{160,220,200,150}[i],"vehicle tuning changed");
            check(s.maxBoardingRelativeSpeed==std::array<double,4>{9,19,27,29}[i],"V0.3 thresholds");
        }
        lc::Movement m;
        check(!m.requestTransfer(2) && m.requestTransfer(-1),"one adjacent lane only");
        for(int i=0;i<47;++i) { m.update(c.fixedStepSeconds); check(!m.requestTransfer(1),"input buffered/reversed/cancelled transfer"); }
        check(m.transferring(),"transfer finished before 0.4s"); m.update(c.fixedStepSeconds);
        check(!m.transferring() && m.lane()==lc::Lane::Left,"transfer must finish at exactly 48 steps");
        m.update(1); check(m.lane()==lc::Lane::Left,"queued input fired");
        std::cout<<"A PASS: 0.40s / 48 steps, adjacent only, input discarded\n";

        lc::Game used;
        attempt(used,target(used,lc::VehicleType::Bike,1,.1,lc::Lane::Left)); used.update(.1);
        check(used.vehicleConsumed(1) && used.attachedVehicle()==1 && used.player().transferring(),"consume exactly on catch before transfer ends");
        check(!used.serviceCompleted(1) && near(used.remainingRideDistance(),160),"consume incorrectly ended service");
        check(!used.requestTransfer(1,{}),"attached lateral transfer cancelled");
        used.update(.3); leave(used);
        check(near(*used.timeSinceVehicleExit(),.4),"manual exit clock incorrect");
        // Deliberately put the used ID in an ideal zone: exclusion must be by ID.
        attempt(used,target(used,lc::VehicleType::Bike,1,0,lc::Lane::Left)); used.update(.4);
        check(!used.attachedVehicle() && !used.transferDebug().targetAtStart && !used.transferDebug().diagnosticTarget && used.transferDebug().excludedUsedVehicle==1,"used vehicle became a target");
        std::cout<<"B PASS: immediate consumption, current service continues, no reboarding\n";

        lc::Game failed;
        auto bus=target(failed,lc::VehicleType::Bus,2,0,lc::Lane::Right);
        attempt(failed,bus); failed.update(.4);
        check(!failed.vehicleConsumed(2) && failed.transferDebug().outcome==lc::TransferOutcome::SpeedRejected,"failed attempt consumed Bus");
        leave(failed); bus.forwardPosition+=bus.speed*.8; // same vehicle keeps moving
        attempt(failed,bus);
        check(failed.transferDebug().diagnosticTarget && failed.transferDebug().diagnosticTarget->id==2 && !failed.transferDebug().excludedUsedVehicle,"failed Bus excluded from later selection");
        std::cout<<"C PASS: failed Bus remains unused and eligible for future attempts\n";

        lc::Game express; reach(express,3);
        express.update(express.remainingRideDistance()/60);
        check(!express.attachedVehicle() && express.returningFromService() && express.serviceCompleted(13) && express.vehicleConsumed(13),"Express service end state");
        check(express.player().forwardSpeed()==60 && express.transferDebug().penaltyApplied==0 && near(*express.timeSinceVehicleExit(),0),"service end release speed/clock/penalty");
        express.update(.4); check(express.player().lane()==lc::Lane::Center && near(express.player().forwardSpeed(),54),"service return decay");
        attempt(express,target(express,lc::VehicleType::Express,13,0,lc::Lane::Right)); express.update(.4);
        check(!express.attachedVehicle() && !express.transferDebug().targetAtStart,"Express reused after service completion");
        std::cout<<"D PASS: Express service ends without penalty, used ID stays excluded\n";

        lc::Movement decay; decay.setForwardSpeed(60);
        for(double expected:{45.,30.,15.}) { decay.update(1,true); check(near(decay.forwardSpeed(),expected),"15/s decay"); }
        decay.update(.2,true); check(near(decay.forwardSpeed(),12),"60 reaches 12 at 3.2s");
        check(near(lc::advanceForward(20,.4,c).speed,14) && near(lc::advanceForward(20,8./15,c).speed,12),"exact Bike decay calculation");
        lc::Game miss; miss.requestTransfer(1,{}); miss.update(.4);
        check(near(miss.player().forwardSpeed(),7) && miss.transferDebug().penaltyApplied==5,"miss order");
        for(double expected:{8.,9.,10.,11.,12.}) { miss.update(.2); check(near(miss.player().forwardSpeed(),expected),"5/s recovery"); }
        std::cout<<"E/F PASS: 15/s decay with clamp; miss 12->7 and 5/s recovery\n";

        lc::Game progression; reach(progression,3); check(progression.player().forwardSpeed()==60,"natural progression");
        for(int from=-1;from<=1;++from) for(int to=from+2;to<4;++to) {
            lc::Game skip; if(from>=0) { reach(skip,from); leave(skip); }
            attempt(skip,target(skip,static_cast<lc::VehicleType>(to),100+to,0,lc::Lane::Right)); skip.update(.4);
            check(!skip.attachedVehicle() && skip.transferDebug().outcome==lc::TransferOutcome::SpeedRejected,"direct skip allowed by thresholds");
        }
        std::cout<<"G PASS: all adjacent upgrades feasible; all six direct skips rejected\n";

        for(double hit:{0.,.04,1./15,.08,.12,.17,.4}) {
            lc::Game one, partitioned; bikeCenter(one); bikeCenter(partitioned);
            auto v=target(one,lc::VehicleType::Bus,88,hit,lc::Lane::Right);
            attempt(one,v); attempt(partitioned,v);
            one.update(.4); for(int i=0;i<48;++i) partitioned.update(c.fixedStepSeconds);
            const bool pass=hit<=1./15+1e-10;
            const auto& d=one.lastBoardingDebug(); const auto& e=partitioned.lastBoardingDebug();
            check(d.intersectionTime && near(*d.intersectionTime,hit) && e.intersectionTime && near(*e.intersectionTime,hit),"event timestamp not recorded accurately");
            const double expected=std::max(12.,14-15*hit);
            check(near(d.playerSpeedAtIntersection,expected) && near(d.absoluteDifferenceAtIntersection,32-expected) && near(e.playerSpeedAtIntersection,expected),"snapshot captured inherited speed or endpoint speed");
            check(d.speedPassedAtIntersection==pass && bool(one.attachedVehicle())==pass && bool(partitioned.attachedVehicle())==pass,"first-contact gate / partition mismatch");
            check(one.vehicleConsumed(88)==pass,"rejected event consumed ID");
            check(near(one.player().forwardPosition(),partitioned.player().forwardPosition()),"event integration depends on dt");
            std::cout<<"Bus contact="<<hit<<"s player="<<d.playerSpeedAtIntersection<<" diff="<<d.absoluteDifferenceAtIntersection<<" limit="<<d.allowedDifference<<" result="<<(pass?"SUCCESS":"MISS")<<'\n';
            if(pass) check(one.player().forwardSpeed()==32,"did not inherit speed immediately");
            else check(near(d.speedBeforePenalty,12) && near(d.speedAfterPenalty,7),"failed Bus penalty not at transfer end");
        }
        // No hardcoded speed 13 or forbidden Run->Bus rule: config alone decides.
        auto relaxed=c; relaxed.vehicleSpecs[1].maxBoardingRelativeSpeed=20;
        lc::Game configurable(relaxed); board(configurable,lc::VehicleType::Bus,200,lc::Lane::Right,.2);
        // Geometry can overlap for a long time, but a failed FIRST entry must
        // not be retroactively accepted when recovery later narrows the gap.
        auto recovering=c; recovering.vehicleSpecs[0].catchZoneLength=3;
        lc::Game lateEligibility(recovering); lateEligibility.requestTransfer(-1,{});lateEligibility.update(.4);leave(lateEligibility);lateEligibility.update(.3); // 10.5
        attempt(lateEligibility,target(lateEligibility,lc::VehicleType::Bike,300,0,lc::Lane::Left));lateEligibility.update(.4);
        check(!lateEligibility.attachedVehicle() && near(lateEligibility.lastBoardingDebug().playerSpeedAtIntersection,10.5),"later speed pass replaced first intersection");
        std::cout<<"Intersection PASS: early success / late rejection / inclusive boundary / actual snapshot / dt partition / configurable gate\n";
        std::cout<<"PASS: "<<checks<<" V0.3 checks\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n'; return 1; }
}
