#pragma once
#include <array>
#include <cstdint>

namespace lc {
enum class Lane : int { Left = -1, Center = 0, Right = 1 };
enum class VehicleType : int { Bike, Bus, Taxi, Express };

struct VehicleSpec {
    double speed;
    double length; // forward units; position denotes the body center
    double catchZoneLength = 1.0; // centered inside body; prototype tuning
    double maxBoardingRelativeSpeed = 30.0;
    double rideServiceDistance = 200.0; // starts at actual attachment, not spawn
};

struct VehicleSeed {
    VehicleType type;
    Lane lane;
    double initialForwardPosition;
};

// Initial prototype parameters. The user determines final values by playtesting.
struct GameplayConfig {
    double normalRunSpeed = 12.0;           // forward units / second
    double normalDeceleration = 15.0;      // V0.3 prototype, above equilibrium
    double recoveryAcceleration = 5.0;     // provisional, below equilibrium
    double missPenalty = 5.0;              // applied once at failed transfer completion
    double minimumForwardSpeed = 1.0;      // positive floor for repeated misses, not normal speed
    double laneSpacing = 4.0;               // lateral units between lane centers
    double lateralTransferDuration = 0.40;  // V0.3 seconds per adjacent transfer
    double fixedStepSeconds = 1.0 / 120.0;
    // At most two 120 Hz steps between visible frames. A 100 ms catch-up made
    // the short, fast Taxi appear ~65% inside the road on its first draw.
    // Excess time is dropped for the whole sandbox, not caught up by one car.
    double maxFrameSeconds = 1.0 / 60.0;
    // Camera-relative road bounds used by BOTH admission planning and rendering.
    double visibleBehindDistance = 100.0 / 13.0;
    double visibleAheadDistance = 258.0 / 13.0;
    double vehicleMinimumGap = 1.0;          // initial visible bumper clearance, tunable
    double maxTransferRelativeSpeed = 30.0; // legacy catch-math fixture; live boarding uses VehicleSpec
    double trafficEntryMargin = 0.02;       // spawn fully beyond the visible bumper edge

    // Order: Bike, Bus, Taxi, Express. Speeds/lengths from the supplied brief.
    std::array<VehicleSpec, 4> vehicleSpecs{{
        // V0.3 prototype tuning, NOT final balance. Gate at first intersection.
        // With 0.40s / -15/s, Bike departure is 20 -> 14; Bus limit 19
        // permits an intersection in the first 1/15 s of the next transfer.
        // Taxi/Express margins cover 32 -> 26 -> 20 and 45 -> 39 -> 33.
        // See docs/GAMEPLAY_V0_3.md for exact derivation and playtest limits.
        {20.0, 3.0, 2.0, 9.0, 160.0}, {32.0, 10.0, 7.0, 19.0, 220.0},
        {45.0, 5.0, 3.5, 27.0, 200.0}, {60.0, 15.0, 7.0, 29.0, 150.0}
    }};
    // Relative weights within a legal opportunity tier; live frequencies need
    // not be equal. Historical fixtures can still draw these weights directly.
    std::array<double, 4> vehicleTypeWeights{{1.0, 1.0, 1.0, 1.0}};
    // Legacy RandomVehicleStream fixture ONLY; LiveTraffic uses per-lane timers below.
    double leftLaneProbability = 0.5;
    double firstVehicleEntryTime = 1.0;
    double minimumSpawnInterval = 2.0;
    double maximumSpawnInterval = 4.0;
    bool useFixedTrafficSeed = false;
    std::uint32_t fixedTrafficSeed = 1337;
    std::array<double, 2> laneFirstSpawnTimes{{1.0, 1.5}};
    std::array<double, 2> laneMinimumSpawnIntervals{{2.0, 2.0}};
    std::array<double, 2> laneMaximumSpawnIntervals{{4.0, 4.0}};
    double minimumSpawnTTC = 1.0;
    double encounterTimeMin = 1.5;
    double encounterTimeMax = 2.5;
    double encounterReviewInterval = 0.25;
    double serviceContinuationLeadTime = 3.0;
    double encounterVisibilityLeadTime = 0.35;
    bool encounterPlannerEnabled = true;
    bool constrainedVehicleSelection = true; // false only for historical isolated traffic fixtures

    double finishDistance = 1200.0; // initial airport distance, not final balance
    // Inclusive S / A / B / C upper times; D extends through deadline, F is later.
    std::array<double, 4> rankTimeLimits{{30.0, 40.0, 50.0, 65.0}};
    double deadline = 90.0; // simulation seconds; late runs continue to the finish

    // Historical deterministic regression fixture ONLY; not the live game stream.
    // Requested center-pass times at player speed 12: 2/5/8/11/14/18/20/23 s.
    // The admission planner shifts an initial position further back if needed
    // to prevent same-lane overlap on screen. These are not balanced catch windows.
    std::array<VehicleSeed, 8> vehicleSequence{{
        {VehicleType::Bike,    Lane::Left,   -16.0},
        {VehicleType::Bus,     Lane::Right, -100.0},
        {VehicleType::Taxi,    Lane::Left,  -264.0},
        {VehicleType::Bus,     Lane::Right, -220.0},
        {VehicleType::Express, Lane::Left,  -672.0},
        {VehicleType::Bike,    Lane::Right, -144.0},
        {VehicleType::Taxi,    Lane::Right, -660.0},
        {VehicleType::Express, Lane::Right,-1104.0}
    }};
};
}
