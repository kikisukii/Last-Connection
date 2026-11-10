#include "game.hpp"
#include "forward_motion.hpp"
#include "live_traffic.hpp"
#include "encounter_planner.hpp"
#include "traffic_safety.hpp"
#include "time_attack.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>
#include <set>

namespace {
int checks = 0;
void require(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
bool near(double a, double b) { return std::abs(a - b) < 1e-7; }
void board(lc::Game& game, lc::VehicleType type, lc::Lane lane, bool atEnd = true) {
    const auto& c = game.player().config();
    const auto spec = c.vehicleSpecs[static_cast<std::size_t>(type)];
    const double hit = type == lc::VehicleType::Bus ? .04 : c.lateralTransferDuration;
    const double distance = lc::advanceForward(game.player().forwardSpeed(), hit, c).distance;
    const double position = atEnd ? game.player().forwardPosition() + distance - spec.catchZoneLength / 2 - spec.speed * hit
                                  : game.player().forwardPosition();
    std::vector<lc::Vehicle> targets{{100 + static_cast<std::size_t>(type), type, lane, position, spec.speed, spec.length, 0}};
    require(game.requestTransfer(static_cast<int>(lane), targets), "fixture transfer rejected");
    game.update(c.lateralTransferDuration);
    require(game.currentVehicle() && game.currentVehicle()->type == type, "speed ladder boarding failed");
}
void returnCenter(lc::Game& game) {
    require(game.requestTransfer(-static_cast<int>(game.player().lane()), {}), "departure rejected");
    game.update(game.player().config().lateralTransferDuration);
    require(game.player().lane() == lc::Lane::Center && game.transferDebug().penaltyApplied == 0, "departure charged miss");
}
void ladder(lc::Game& game, int last = 3) {
    for (int type = 0; type <= last; ++type) {
        if (type) returnCenter(game);
        board(game, static_cast<lc::VehicleType>(type), type % 2 ? lc::Lane::Right : lc::Lane::Left);
    }
}
lc::Vehicle ridingBody(const lc::Game& game) {
    auto body = *game.currentVehicle();
    body.forwardPosition += body.speed * (game.player().elapsedSeconds() - game.transferDebug().startTime);
    return body;
}
bool attemptAvailable(lc::Game& game, std::span<const lc::Vehicle> vehicles) {
    if (game.player().transferring() || game.currentVehicle() || game.player().lane() != lc::Lane::Center) return false;
    const auto& c = game.player().config();
    for (const auto& v : vehicles) {
        if(game.serviceConsumed(v.id)) continue;
        const auto& spec = c.vehicleSpecs[static_cast<std::size_t>(v.type)];
        if (lc::predictBoardingCatch(game.player().forwardPosition(), game.player().forwardSpeed(), v.forwardPosition, spec, c.lateralTransferDuration, c).canCatch())
            return game.requestTransfer(static_cast<int>(v.lane), vehicles);
    }
    return false;
}
}
int main() {
    try {
        lc::GameplayConfig c;
        require(c.normalRunSpeed == 12 && c.normalDeceleration == 15 && c.missPenalty == 5 && c.lateralTransferDuration == .40 && c.finishDistance == 1200 && c.deadline == 90,
                "protected gameplay rules changed");
        require(c.vehicleSpecs[3].length == 15 && c.vehicleSpecs[3].catchZoneLength == 7, "Express body/zone separation");
        // A: live ladder with an early Bus intersection and later Taxi/Express catches.
        lc::Game speedLadder(c); ladder(speedLadder);
        require(speedLadder.player().forwardSpeed() == 60, "ladder did not reach Express");
        lc::Game direct(c);
        direct.requestTransfer(1, std::vector<lc::Vehicle>{{1,lc::VehicleType::Express,lc::Lane::Right,0,60,15,0}});
        direct.update(c.lateralTransferDuration);
        require(!direct.attachedVehicle() && direct.transferDebug().outcome == lc::TransferOutcome::SpeedRejected, "Run -> Express accepted");
        auto tighter = c; tighter.vehicleSpecs[1].maxBoardingRelativeSpeed = 16;
        lc::Game wholeWindow(tighter); board(wholeWindow,lc::VehicleType::Bike,lc::Lane::Left); returnCenter(wholeWindow);
        wholeWindow.requestTransfer(1, std::vector<lc::Vehicle>{{5,lc::VehicleType::Bus,lc::Lane::Right,wholeWindow.player().forwardPosition(),32,10,0}});
        wholeWindow.update(c.lateralTransferDuration);
        require(!wholeWindow.attachedVehicle() && wholeWindow.transferDebug().outcome == lc::TransferOutcome::SpeedRejected, "intersection exceeded configured speed threshold");
        std::cout << "A PASS: 12 -> 20 -> 32 -> 45 -> 60; intersection gate\n";

        // B: service starts at actual catch, not generation or transfer start.
        const double service = speedLadder.remainingRideDistance();
        require(near(service, c.vehicleSpecs[3].rideServiceDistance), "service charged before catch at end of transfer");
        speedLadder.update(1);
        require(near(speedLadder.remainingRideDistance(), service - 60) && speedLadder.player().forwardSpeed() == 60, "riding distance/speed incorrect");
        speedLadder.update(speedLadder.remainingRideDistance() / 60);
        require(!speedLadder.attachedVehicle() && speedLadder.returningFromService() && speedLadder.player().forwardSpeed() == 60 && speedLadder.transferDebug().penaltyApplied == 0, "service end speed or penalty incorrect");
        speedLadder.update(c.lateralTransferDuration);
        require(speedLadder.player().lane() == lc::Lane::Center && near(speedLadder.player().forwardSpeed(),54) && speedLadder.transferDebug().outcome == lc::TransferOutcome::ServiceEnd, "automatic center return failed");
        require(speedLadder.serviceConsumed(103), "Express ID was not consumed");
        speedLadder.requestTransfer(1,std::vector<lc::Vehicle>{{103,lc::VehicleType::Express,lc::Lane::Right,speedLadder.player().forwardPosition(),60,15,0}});
        speedLadder.update(c.lateralTransferDuration);
        require(!speedLadder.attachedVehicle() && speedLadder.serviceConsumed(103), "consumed Express was boarded again");
        require(!speedLadder.transferDebug().targetAtStart, "consumed ID remained a transfer candidate");
        returnCenter(speedLadder);
        speedLadder.update(.1);
        require(!speedLadder.currentVehicle(), "service completion or rejection forced a replacement ride");
        speedLadder.requestTransfer(1,std::vector<lc::Vehicle>{{104,lc::VehicleType::Express,lc::Lane::Right,speedLadder.player().forwardPosition(),60,15,0}});
        speedLadder.update(c.lateralTransferDuration);
        require(speedLadder.attachedVehicle()==104 && speedLadder.serviceConsumed(103), "a new vehicle ID was blocked by the consumed old connection");
        lc::Game partial(c); board(partial,lc::VehicleType::Bike,lc::Lane::Left);
        partial.update(1);
        returnCenter(partial);
        partial.requestTransfer(-1,std::vector<lc::Vehicle>{{100,lc::VehicleType::Bike,lc::Lane::Left,partial.player().forwardPosition(),20,3,0}});
        partial.update(c.lateralTransferDuration);
        require(!partial.attachedVehicle() && partial.vehicleConsumed(100) && !partial.serviceCompleted(100), "manual departure allowed a second ride");
        auto shortService = c; shortService.vehicleSpecs[0].rideServiceDistance = 1;
        lc::Game midTransfer(shortService);
        midTransfer.requestTransfer(-1, std::vector<lc::Vehicle>{{8,lc::VehicleType::Bike,lc::Lane::Left,0,20,3,0}});
        midTransfer.update(.04); const double x = midTransfer.player().lateralPosition();
        midTransfer.update(.01);
        require(midTransfer.returningFromService() && midTransfer.player().forwardSpeed() == 20 && midTransfer.player().lateralPosition() < x, "mid-transfer service end was duplicated or snapped");
        midTransfer.update(c.lateralTransferDuration);
        require(midTransfer.player().lane() == lc::Lane::Center && midTransfer.transferDebug().penaltyApplied == 0, "short service caused miss");
        lc::Game large(c), small(c); ladder(large); ladder(small);
        large.update(4);
        for (int i=0;i<480;++i) small.update(1.0/120);
        require(near(large.player().forwardPosition(),small.player().forwardPosition()) && near(large.player().forwardSpeed(),small.player().forwardSpeed()), "service integration depends on partition");
        std::cout << "B PASS: finite Express, exact service event, center return, no miss\n";

        // C: arrange a visible slower connection before Express expires.
        auto plannedConfig = c; plannedConfig.laneFirstSpawnTimes = {0,0};
        lc::Game continuation(plannedConfig); ladder(continuation);
        const auto initialBody = ridingBody(continuation);
        lc::LiveTraffic continuedTraffic(plannedConfig, 17, std::vector<lc::Vehicle>{initialBody});
        continuedTraffic.update(0, continuation);
        require(continuedTraffic.encounterDebug().continuation, "Express continuation was not planned early");
        bool ahead = false;
        for (const auto& v : continuedTraffic.current()) if (v.id != initialBody.id && v.forwardPosition > continuation.player().forwardPosition()) ahead = true;
        require(ahead, "no forward traffic after boarding Express");
        bool connected = false, consumedStillMoving = false;
        double firstSeen = -1, start = continuation.player().elapsedSeconds(), caughtAt = 0;
        for (int i=0;i<600 && !connected;++i) {
            attemptAvailable(continuation, continuedTraffic.current());
            continuation.update(c.fixedStepSeconds); continuedTraffic.update(c.fixedStepSeconds, continuation);
            if(continuation.returningFromService()) for(const auto& v:continuedTraffic.current())
                if(v.id==initialBody.id) consumedStillMoving = continuation.serviceConsumed(v.id) && v.speed==60;
            for (const auto& v : continuedTraffic.current()) if (v.id != initialBody.id &&
                v.forwardPosition-v.length/2 <= continuation.player().forwardPosition()+c.visibleAheadDistance &&
                v.forwardPosition+v.length/2 >= continuation.player().forwardPosition()-c.visibleBehindDistance && firstSeen < 0)
                firstSeen = continuation.player().elapsedSeconds()-start;
            connected = continuation.currentVehicle() && continuation.currentVehicle()->id != initialBody.id;
            if (connected) caughtAt = continuation.player().elapsedSeconds()-start;
        }
        require(connected && continuation.currentVehicle()->type != lc::VehicleType::Express, "Express ended into an empty recovery gap");
        require(consumedStillMoving, "consumed Express disappeared at service end or changed speed");
        require(firstSeen >= 0 && caughtAt-firstSeen >= c.encounterVisibilityLeadTime, "continuation appeared too late to observe");
        std::cout << "C PASS: Express -> " << lc::vehicleName(continuation.currentVehicle()->type) << " at " << caughtAt << "s, visible lead " << caughtAt-firstSeen << "s\n";

        // D: right scheduling, RNG and positions are independent of blocked left.
        auto independent = c; independent.encounterPlannerEnabled = false;
        independent.constrainedVehicleSelection = false; // isolate TTC scheduler independence from global type reservations
        independent.vehicleTypeWeights = {0,0,1,0};
        independent.laneFirstSpawnTimes = {0,0};
        lc::LiveTraffic blocked(independent, 91, std::vector<lc::Vehicle>{{100,lc::VehicleType::Bike,lc::Lane::Left,0,20,3,0}});
        lc::LiveTraffic clear(independent, 91);
        bool rightSpawned = false, leftBlocked = false;
        for (int i=0;i<600;++i) {
            const double p=12*(i+1)*c.fixedStepSeconds;
            blocked.update(c.fixedStepSeconds,p,12); clear.update(c.fixedStepSeconds,p,12);
            leftBlocked = leftBlocked || blocked.laneDebug(lc::Lane::Left).blocked;
            std::vector<double> a,b;
            for (const auto& v : blocked.current()) if(v.lane==lc::Lane::Right) a.push_back(v.forwardPosition);
            for (const auto& v : clear.current()) if(v.lane==lc::Lane::Right) b.push_back(v.forwardPosition);
            require(a==b && blocked.laneDebug(lc::Lane::Right).nextSpawnTime==clear.laneDebug(lc::Lane::Right).nextSpawnTime,"left blocked right scheduler or RNG");
            rightSpawned = rightSpawned || !a.empty();
        }
        require(leftBlocked && rightSpawned,"lane independence fixture did not block left/spawn right");
        std::cout << "D PASS: left pending cannot stop right\n";

        // E: TTC uses only vehicle gap/speeds. Large-gap faster rear is permitted
        // when the fixed-world exit occurs before even the safety gap is consumed.
        lc::Vehicle front{1,lc::VehicleType::Bus,lc::Lane::Left,100,32,10,0};
        lc::Vehicle rear{2,lc::VehicleType::Taxi,lc::Lane::Left,90,45,5,0};
        require(!lc::checkTrafficAdmission(rear,std::vector<lc::Vehicle>{front},c).allowed,"small TTC accepted");
        front.forwardPosition=1100; rear.forwardPosition=892.5; // bumper gap 200
        auto safe=lc::checkTrafficAdmission(rear,std::vector<lc::Vehicle>{front},c);
        require(safe.allowed && near(safe.ttc,200.0/13) && safe.frontExitTime < safe.ttc,"large TTC with guaranteed exit rejected");
        const double ttc=safe.ttc;
        for(double playerSpeed : {12.0,32.0,60.0}) {
            (void)playerSpeed;
            require(lc::checkTrafficAdmission(rear,std::vector<lc::Vehicle>{front},c).ttc==ttc,"player speed changed two-vehicle TTC");
        }
        std::cout << "E PASS: TTC 200/13 = " << ttc << "; no player velocity input\n";

        // F: high-speed miss -> planner's earlier, observable Taxi connection.
        lc::Game recovery(plannedConfig); ladder(recovery,2); returnCenter(recovery);
        recovery.requestTransfer(1,{}); recovery.update(c.lateralTransferDuration); // 39 -> 33 -> penalty 28
        require(near(recovery.player().forwardSpeed(),28),"recovery fixture penalty changed");
        lc::LiveTraffic recoveryTraffic(plannedConfig,29);
        recoveryTraffic.update(0,recovery);
        require(!recoveryTraffic.current().empty(),"no recovery encounter after high-speed miss");
        returnCenter(recovery);
        // Keep the test traffic at the same elapsed time as the explicit return.
        recoveryTraffic.update(c.lateralTransferDuration,recovery);
        bool recovered=false;
        for(int i=0;i<360 && !recovered;++i) {
            attemptAvailable(recovery,recoveryTraffic.current());
            recovery.update(c.fixedStepSeconds); recoveryTraffic.update(c.fixedStepSeconds,recovery);
            recovered=recovery.attachedVehicle().has_value();
        }
        require(recovered && recovery.player().forwardSpeed()>12,"recovery connection failed before momentum was lost");
        std::cout << "F PASS: high-speed miss -> " << lc::vehicleName(recovery.currentVehicle()->type) << " recovery\n";
        int expressCatches = 0, serviceEnds = 0, completed = 0;
        std::array<int,4> boardedCounts{};
        std::set<std::string> spawnSequences;
        int preparedCatches=0;
        for (unsigned seed=0;seed<16;++seed) {
            lc::TimeAttack race(c,seed);
            std::optional<std::size_t> previousRide;
            std::map<std::size_t,lc::Vehicle> oldVehicles;
            bool wasReturning = false;
            std::array<std::optional<lc::VehicleType>,2> lastSpawn;
            std::array<std::array<std::size_t,4>,2> actualCounts{};
            std::string sequence;
            std::set<std::size_t> preparedIds, usedIds;
            for(int tick=0;tick<18000 && !race.finished();++tick) {
                const auto& g=race.game();
                if (!g.player().transferring()) {
                    if (g.currentVehicle() && g.currentVehicle()->type != lc::VehicleType::Express) {
                        lc::Game afterDeparture=g;
                        const int direction=-static_cast<int>(g.player().lane());
                        afterDeparture.requestTransfer(direction,{}); afterDeparture.update(c.lateralTransferDuration);
                        for(const auto& v:race.traffic().current()) {
                            if(g.serviceConsumed(v.id)) continue;
                            if(v.lane==g.player().lane() || v.speed<=g.player().forwardSpeed()) continue;
                            const auto& spec=c.vehicleSpecs[static_cast<std::size_t>(v.type)];
                            if(lc::predictBoardingCatch(afterDeparture.player().forwardPosition(),afterDeparture.player().forwardSpeed(),v.forwardPosition+v.speed*c.lateralTransferDuration,spec,c.lateralTransferDuration,c).canCatch()) {
                                race.requestTransfer(direction); break;
                            }
                        }
                    } else if(!g.currentVehicle() && g.player().lane()==lc::Lane::Center) {
                        const lc::Vehicle* best = nullptr;
                        for(const auto& v:race.traffic().current()) {
                            if(g.serviceConsumed(v.id)) continue;
                            const auto& spec=c.vehicleSpecs[static_cast<std::size_t>(v.type)];
                            if(lc::predictBoardingCatch(g.player().forwardPosition(),g.player().forwardSpeed(),v.forwardPosition,spec,c.lateralTransferDuration,c).canCatch()) {
                                if(!best || v.speed>best->speed) best=&v;
                            }
                        }
                        if(best) race.requestTransfer(static_cast<int>(best->lane));
                    } else if(!g.currentVehicle()) race.requestTransfer(-static_cast<int>(g.player().lane()));
                }
                const double before=race.elapsed();
                race.update(c.fixedStepSeconds);
                const double dt=race.elapsed()-before;
                if(race.game().currentVehicle() && previousRide!=race.game().attachedVehicle()) {
                    if(preparedIds.contains(*race.game().attachedVehicle())) ++preparedCatches;
                    require(usedIds.insert(*race.game().attachedVehicle()).second && race.game().vehicleConsumed(*race.game().attachedVehicle()), "full run reboarded a consumed ID");
                    ++boardedCounts[static_cast<std::size_t>(race.game().currentVehicle()->type)];
                    if(race.game().currentVehicle()->type==lc::VehicleType::Express) ++expressCatches;
                }
                if(race.game().returningFromService() && !wasReturning) ++serviceEnds;
                previousRide=race.game().attachedVehicle(); wasReturning=race.game().returningFromService();
                std::map<std::size_t,lc::Vehicle> next;
                const auto vehicles=race.traffic().current();
                for(std::size_t i=0;i<vehicles.size();++i) {
                    const auto& v=vehicles[i];
                    require(v.speed==c.vehicleSpecs[static_cast<std::size_t>(v.type)].speed,"generated car speed was modified");
                    if(oldVehicles.contains(v.id)) require(near(v.forwardPosition,oldVehicles.at(v.id).forwardPosition+v.speed*dt),"generated car teleported");
                    else {
                        require(v.forwardPosition+v.length/2<race.game().player().forwardPosition()-c.visibleBehindDistance ||
                                 v.forwardPosition-v.length/2>race.game().player().forwardPosition()+c.visibleAheadDistance,"encounter appeared inside viewport");
                        const int lane=v.lane==lc::Lane::Left?0:1;
                        require(lastSpawn[lane]!=v.type,"same lane repeated its last spawned type");
                        lastSpawn[lane]=v.type; ++actualCounts[lane][static_cast<std::size_t>(v.type)];
                        sequence+=static_cast<char>('A'+static_cast<int>(v.type));
                        if(lc::isPreparedCandidate(race.game(),v,vehicles)) preparedIds.insert(v.id);
                    }
                    for(std::size_t j=i+1;j<vehicles.size();++j) {
                        if(v.lane==vehicles[j].lane)
                            require(std::abs(v.forwardPosition-vehicles[j].forwardPosition)+1e-7 >= (v.length+vehicles[j].length)/2+c.vehicleMinimumGap,"overlap during full service/encounter cycle");
                        else require(v.type!=vehicles[j].type,"opposite active lanes share a type");
                    }
                    const auto opposite=v.lane==lc::Lane::Left?lc::Lane::Right:lc::Lane::Left;
                    require(race.traffic().laneDebug(opposite).pendingVehicle!=v.type,"opposite pending duplicates active type");
                    next.emplace(v.id,v);
                }
                oldVehicles=std::move(next);
            }
            if(race.finished()) ++completed;
            for(int lane=0;lane<2;++lane) {
                const auto& d=race.traffic().laneDebug(lane?lc::Lane::Right:lc::Lane::Left);
                require(d.spawnCounts==actualCounts[lane] && d.lastSpawnedType==lastSpawn[lane],"spawn statistics counted requests instead of actual admission");
            }
            spawnSequences.insert(sequence);
        }
        std::cout << "Full-cycle stress: " << completed << " completed runs, " << expressCatches << " Express catches, " << serviceEnds << " service ends; constant speeds/no overlap\n";
        std::cout << "Boarding counts Bike/Bus/Taxi/Express: " << boardedCounts[0] << '/' << boardedCounts[1] << '/' << boardedCounts[2] << '/' << boardedCounts[3] << '\n';
        require(completed==16 && expressCatches>0 && serviceEnds>0,"full runs did not exercise Express/service completion");
        require(spawnSequences.size()>1,"selection became a completely fixed type sequence");
        require(preparedCatches>0,"no actual Prepared vehicle became catchable after progression");
        std::cout<<"V0.2 diversity: "<<spawnSequences.size()<<" distinct spawn sequences; cross-lane reservations, per-lane history and actual counts verified\n";
        std::cout<<"Prepared -> actual catch: "<<preparedCatches<<"\n";
        std::cout << "PASS: " << checks << " traffic/service checks\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
}
