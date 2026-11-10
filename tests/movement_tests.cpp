#include "movement.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool condition, const char* label) {
    ++checks;
    if (!condition) throw std::runtime_error(label);
}
void near(double actual, double expected, const char* label) {
    check(std::abs(actual - expected) < 1e-8, label);
}
}

int main() {
    try {
        lc::GameplayConfig historical; historical.lateralTransferDuration = .25;
        lc::Movement p(historical);
        check(p.lane() == lc::Lane::Center && !p.transferring(), "starts running in center");
        p.update(2.0);
        near(p.forwardPosition(), 24.0, "automatic 12 units/s progress");
        check(!p.requestTransfer(2), "reject direct cross-three-lane input");
        check(!p.requestTransfer(0), "reject zero direction");
        check(p.requestTransfer(-1), "center to left accepted");
        near(p.lateralPosition(), 0.0, "no instantaneous lateral teleport");
        near(p.lateralVelocity(), 0.0, "starts with zero lateral velocity");
        check(p.lateralAcceleration() < 0.0, "accelerates toward left");
        check(!p.requestTransfer(1), "ignore reversal during transfer");
        p.update(0.125);
        near(p.transferProgress(), 0.5, "half duration");
        near(p.lateralPosition(), -2.0, "half lane displacement");
        near(p.forwardPosition(), 25.5, "continues forward during transfer");
        check(p.lane() == lc::Lane::Center, "logical lane stays at source until arrival");
        check(p.lateralVelocity() < 0.0, "lateral velocity exists mid transfer");
        p.update(0.1);
        check(p.lateralAcceleration() > 0.0, "decelerates before arrival");
        p.update(0.026);
        check(!p.transferring() && p.lane() == lc::Lane::Left, "lands on left");
        near(p.lateralPosition(), -4.0, "no overshoot");
        near(p.lateralVelocity(), 0.0, "stops lateral motion on arrival");
        check(!p.requestTransfer(-1), "left boundary blocks input");
        check(p.requestTransfer(1), "left to center accepted");
        p.update(1.0);
        check(p.lane() == lc::Lane::Center, "one input never crosses center");
        check(p.requestTransfer(1), "center to right needs second input");
        p.update(0.25);
        check(p.lane() == lc::Lane::Right && !p.transferring(), "exact-duration arrival");
        check(!p.requestTransfer(1), "right boundary blocks input");

        lc::Movement a(historical), b(historical);
        a.requestTransfer(1);
        b.requestTransfer(1);
        a.update(0.2);
        for (int i = 0; i < 24; ++i) b.update(1.0 / 120.0);
        near(a.lateralPosition(), b.lateralPosition(), "trajectory independent of dt partition");
        near(a.forwardPosition(), b.forwardPosition(), "forward motion independent of dt partition");
        b.update(1.0);
        check(b.lane() == lc::Lane::Right, "large step lands without overshoot");

        lc::Movement fixed(historical);
        fixed.requestTransfer(-1);
        for (int i = 0; i < 30; ++i) fixed.update(1.0 / 120.0);
        check(!fixed.transferring() && fixed.lane() == lc::Lane::Left,
              "0.25-second transfer completes in exactly 30 simulation steps");
        fixed.update(2.0);
        check(fixed.lane() == lc::Lane::Left, "arrival never starts another transfer automatically");

        lc::GameplayConfig custom;
        custom.normalRunSpeed = 18.0;
        custom.laneSpacing = 6.0;
        custom.lateralTransferDuration = 0.5;
        lc::Movement tuned(custom);
        tuned.requestTransfer(-1);
        tuned.update(0.25);
        near(tuned.lateralPosition(), -3.0, "custom lane spacing and duration applied");
        near(tuned.forwardPosition(), 4.5, "custom forward speed applied");
        bool rejected = false;
        try { tuned.update(-1.0); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "negative dt rejected");
        rejected = false;
        custom.lateralTransferDuration = 0.0;
        try { lc::Movement invalid(custom); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "zero duration rejected");
        rejected = false;
        try { tuned.update(std::numeric_limits<double>::quiet_NaN()); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "NaN dt rejected");
        std::cout << "PASS: " << checks << " deterministic movement checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL after check " << checks << ": " << error.what() << '\n';
        return 1;
    }
}
