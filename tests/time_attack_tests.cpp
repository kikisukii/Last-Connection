#include "time_attack.hpp"
#include "frame_clock.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int checks = 0;
void require(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
bool near(double a, double b) { return std::abs(a - b) < 1e-8; }
}
int main() {
    try {
        lc::GameplayConfig c;
        for (const auto [time, rank] : {std::pair{30.0,'S'}, {40.0,'A'}, {50.0,'B'}, {65.0,'C'}, {90.0,'D'}, {90.000001,'F'}})
            require(lc::rankForTime(time, c) == rank, "inclusive rank boundary incorrect");
        require(lc::rankForTime(30.000001, c) == 'A' && lc::rankForTime(65.000001, c) == 'D', "rank rounds time before classification");
        require(lc::formatRaceTime(59.999) == "01:00.00" && lc::formatRaceTime(43.72) == "00:43.72", "time formatting rollover");
        for (double renderDelta : {1.0/144, 1.0/60, 1.0/30, .1, .5}) {
            auto config = c; config.finishDistance = 12.345;
            lc::TimeAttack race(config, 123); lc::FrameClock clock(config);
            double wallTime = 0;
            for (int frame = 0; frame < 10000 && !race.finished(); ++frame) {
                wallTime += renderDelta;
                const int steps = clock.advance(renderDelta);
                for (int i = 0; i < steps; ++i) race.update(config.fixedStepSeconds);
            }
            require(race.finished() && near(*race.finishTime(), 12.345 / 12), "finish depends on render FPS or overshoots final tick");
            require(near(race.game().player().forwardPosition(), 12.345) && race.remainingDistance() == 0 && race.progress() == 1, "player not stopped on finish line");
            require(race.rank() == 'S', "short run rank incorrect");
            const auto position = race.game().player().forwardPosition();
            race.update(100);
            require(!race.requestTransfer(-1) && race.game().player().forwardPosition() == position, "finished race still accepts motion/input");
            if (renderDelta == .5) require(wallTime > race.elapsed() * 10, "score must use simulation time under stalls");
            race.restart(456);
            require(!race.finished() && race.elapsed() == 0 && race.progress() == 0 && race.traffic().seed() == 456 && race.traffic().current().empty(), "restart retained prior run state");
            require(race.game().player().lane() == lc::Lane::Center && race.game().player().forwardSpeed() == 12, "restart retained lane/speed");
        }
        lc::TimeAttack late(c, 1);
        for (int i = 0; i < 10920; ++i) late.update(c.fixedStepSeconds); // 91 sec
        require(late.late() && !late.finished() && near(late.elapsed(), 91), "deadline must not terminate a late run");
        for (int i = 0; i < 1200 && !late.finished(); ++i) late.update(c.fixedStepSeconds);
        require(late.finished() && late.rank() == 'F' && near(*late.finishTime(), 100), "late arrival must retain actual 100-second finish");
        auto boundary = c; boundary.finishDistance = 1080;
        lc::TimeAttack exact(boundary, 1);
        for (int i = 0; i < 10801 && !exact.finished(); ++i) exact.update(c.fixedStepSeconds);
        require(exact.finished() && exact.rank() == 'D' && !exact.late(), "exact deadline should count as D despite accumulated floating-point error");

        // Finish inside a recovering segment; solve independently from x=3+7t+2.5t².
        auto recoveryConfig = c; recoveryConfig.lateralTransferDuration = .25; recoveryConfig.finishDistance = 4; // historical analytic fixture
        lc::TimeAttack recovering(recoveryConfig, 1);
        recovering.requestTransfer(-1);
        recovering.update(1);
        const double expected = .25 + (-7 + std::sqrt(59.0)) / 5;
        require(recovering.finished() && near(*recovering.finishTime(), expected), "finish interpolated linearly across miss/recovery event");
        require(near(recovering.game().player().forwardPosition(), 4), "recovery finish position incorrect");

        // Catch a generated bike, then finish while riding. No fixture-only mount path.
        auto bikeConfig = c; bikeConfig.finishDistance = 100; bikeConfig.vehicleTypeWeights = {1,0,0,0}; bikeConfig.leftLaneProbability = 1;
        lc::TimeAttack riding(bikeConfig, 4);
        bool requested = false;
        for (int i = 0; i < 2400 && !riding.finished(); ++i) {
            if (!requested) for (const auto& v : riding.traffic().current()) {
                const double relative = v.forwardPosition - riding.game().player().forwardPosition();
                if (relative > -2.5 && relative < -1) { requested = riding.requestTransfer(-1); break; }
            }
            riding.update(c.fixedStepSeconds);
        }
        require(riding.finished() && riding.game().attachedVehicle() && *riding.finishTime() < 100.0/12, "riding did not improve time / riding finish failed");
        const auto& attempt = riding.game().transferDebug();
        const double catchTime = attempt.startTime + attempt.prediction.enterTime;
        const double analyticFinish = catchTime + (100 - 12 * catchTime) / 20;
        require(near(*riding.finishTime(), analyticFinish), "riding finish time inaccurate");

        const auto record = lc::encodeBestTime(43.72, c);
        require(lc::decodeBestTime(record, c) == 43.72, "best record round-trip failed");
        for (auto invalid : {std::string{}, std::string("garbage"), lc::scoreRulesKey(c) + "\n-1\n", lc::scoreRulesKey(c) + "\n0\n", lc::scoreRulesKey(c) + "\nnan\n", record + "trailing"})
            require(!lc::decodeBestTime(invalid, c), "corrupt best record accepted");
        auto changed = c; changed.missPenalty = 3;
        require(!lc::decodeBestTime(record, changed), "best leaked across gameplay parameter change");
        changed = c; changed.finishDistance = 1000;
        require(!lc::decodeBestTime(record, changed), "best leaked across finish distance change");
        changed = c; changed.rankTimeLimits[1] = changed.rankTimeLimits[0];
        bool threw = false; try { lc::TimeAttack invalid(changed, 0); } catch (const std::invalid_argument&) { threw = true; }
        require(threw, "invalid rank ordering accepted");
        std::cout << "PASS: " << checks << " race clock, exact finish, ranks, late arrival, restart and best-record checks\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
