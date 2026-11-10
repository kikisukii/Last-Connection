#include "time_attack.hpp"
#include "best_time_storage.hpp"
#include "frame_clock.hpp"
#include "road_view.hpp"
#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace {
const lc::GameplayConfig config;
std::uint32_t newTrafficSeed() {
    if (config.useFixedTrafficSeed) return config.fixedTrafficSeed;
    // Wall time, not a browser performance clock whose origin resets on reload.
    const auto ticks = static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
    return static_cast<std::uint32_t>(ticks ^ (ticks >> 32));
}
lc::TimeAttack race(config, newTrafficSeed());
const lc::Game& game = race.game();
const lc::Movement& player = game.player();
const lc::LiveTraffic& traffic = race.traffic();
lc::FrameClock frameClock(player.config());
std::optional<double> bestTime;
bool storageAvailable = true;
bool resultRecorded = false;
bool newBest = false;
bool started = false;
constexpr Rectangle actionButton{350, 393, 260, 48};
const lc::RoadView roadView(player.config());
constexpr Color background{18, 24, 33, 255};
constexpr Color accent{104, 226, 180, 255};
constexpr float lanePixels = 155.0f;
constexpr float roadCenter = 340.0f;
constexpr float roadTop = lc::RoadView::top;
constexpr float roadBottom = lc::RoadView::bottom;
const float forwardPixelsPerUnit = roadView.pixelsPerUnit;
const float playerScreenY = roadView.playerY;

Color vehicleColor(lc::VehicleType type) {
    switch (type) {
    case lc::VehicleType::Bike: return Color{90, 203, 221, 255};
    case lc::VehicleType::Bus: return Color{103, 153, 225, 255};
    case lc::VehicleType::Taxi: return Color{238, 197, 90, 255};
    case lc::VehicleType::Express: return Color{184, 143, 226, 255};
    }
    return GRAY;
}

void drawVehicles(std::span<const lc::Vehicle> vehicles) {
    BeginScissorMode(120, 112, 440, 358);
    for (const auto& vehicle : vehicles) {
        const auto bounds = roadView.bounds(vehicle, player.forwardPosition());
        if (bounds.bottom < roadTop || bounds.top > roadBottom) continue;
        const float y = (bounds.top + bounds.bottom) / 2.0f;
        const float length = bounds.bottom - bounds.top;
        const float x = roadCenter + static_cast<int>(vehicle.lane) * lanePixels;
        const Rectangle body{x - 48, bounds.top, 96, length};
        const Color color = vehicleColor(vehicle.type);
        DrawRectangleRec(body, Fade(color, 0.28f));
        DrawRectangleLinesEx(body, 2.0f, color);
        const float zoneLength = static_cast<float>(player.config().vehicleSpecs[static_cast<std::size_t>(vehicle.type)].catchZoneLength) * forwardPixelsPerUnit;
        const Rectangle zone{x - 44, y - zoneLength / 2, 88, zoneLength};
        const bool riding = game.attachedVehicle() == vehicle.id;
        DrawRectangleRec(zone, Fade(riding ? accent : color, 0.48f));
        DrawRectangleLinesEx(zone, 1, riding ? accent : RAYWHITE);
        DrawLineEx(Vector2{x - 45, body.y + 3}, Vector2{x + 45, body.y + 3}, 3.0f, color);
        DrawText(game.serviceConsumed(vehicle.id) ? "USED" : lc::vehicleName(vehicle.type), static_cast<int>(x - 40), static_cast<int>(y - 15), 16, color);
        DrawText(TextFormat("%.0f u/s", vehicle.speed), static_cast<int>(x - 35), static_cast<int>(y + 3), 14, RAYWHITE);
    }
    EndScissorMode();
}

void drawSandbox() {
    const auto vehicles = traffic.current();
    BeginDrawing();
    ClearBackground(background);
    DrawText("LAST CONNECTION", 28, 20, 28, RAYWHITE);
    DrawText(TextFormat("V0.3 / MOVEMENT FEEL / SEED %u", static_cast<unsigned>(traffic.seed())), 28, 54, 16, LIGHTGRAY);
    DrawText(TextFormat("TIME %s", lc::formatRaceTime(race.elapsed()).c_str()), 600, 18, 22, RAYWHITE);
    DrawText(TextFormat("DEADLINE %s%s", lc::formatRaceTime(config.deadline).c_str(), race.late() ? " / LATE" : ""), 600, 49, 18, race.late() ? ORANGE : LIGHTGRAY);
    for (int lane = -1; lane <= 1; ++lane) {
        const float x = roadCenter + lane * lanePixels;
        DrawRectangle(static_cast<int>(x - 65), 112, 130, 358, Color{29, 39, 51, 255});
        DrawText(lc::laneName(static_cast<lc::Lane>(lane)), static_cast<int>(x - 35), 86, 16, LIGHTGRAY);
        const double offset = std::fmod(player.forwardPosition() * forwardPixelsPerUnit, 60.0);
        for (float y = 115.0f + static_cast<float>(offset); y < 450.0f; y += 60.0f) {
            DrawLine(static_cast<int>(x - 54), static_cast<int>(y), static_cast<int>(x + 54),
                     static_cast<int>(y), Color{54, 66, 80, 255});
        }
    }
    drawVehicles(vehicles);
    const float finishY = playerScreenY - static_cast<float>(race.remainingDistance()) * forwardPixelsPerUnit;
    if (finishY >= roadTop && finishY <= roadBottom) {
        DrawRectangle(120, static_cast<int>(finishY - 3), 440, 6, accent);
        DrawText("AIRPORT / FINISH", 240, static_cast<int>(finishY - 24), 16, accent);
    }
    const float x = roadCenter + static_cast<float>(player.lateralPosition() / player.config().laneSpacing) * lanePixels;
    DrawCircleV(Vector2{x, playerScreenY}, 14.0f, accent);
    DrawTriangle(Vector2{x, playerScreenY - 26}, Vector2{x - 6, playerScreenY - 17},
                 Vector2{x + 6, playerScreenY - 17}, accent);

    constexpr int debugX = 600;
    DrawText("DEBUG / ON", debugX, 90, 21, accent);
    DrawText(TextFormat("Speed: %.2f / Normal: %.0f", player.forwardSpeed(), player.config().normalRunSpeed), debugX, 119, 18, RAYWHITE);
    DrawText(TextFormat("Lane: %s -> %s", lc::laneName(player.lane()), lc::laneName(player.targetLane())), debugX, 141, 16, RAYWHITE);
    DrawText(TextFormat("State: %s", game.stateName()), debugX, 162, 16, RAYWHITE);
    DrawText(TextFormat("Transfer: %.0f%% / %.2fs", player.transferProgress() * 100.0,
                        player.config().lateralTransferDuration), debugX, 183, 16, RAYWHITE);
    DrawText(TextFormat("Forward: %.2f / Sim: %.2fs", player.forwardPosition(), player.elapsedSeconds()), debugX, 204, 16, RAYWHITE);
    if (game.currentVehicle()) {
        const auto& ride = *game.currentVehicle();
        DrawText(TextFormat("Current ID: %u / %s / %.0f u/s", static_cast<unsigned>(ride.id), lc::vehicleName(ride.type), ride.speed), debugX, 230, 16, accent);
        DrawText(TextFormat("Service: %.1f / VEHICLE USED: YES", game.remainingRideDistance()), debugX, 251, 15, accent);
    } else {
        DrawText("Current ID: -- / Speed: --", debugX, 230, 16, LIGHTGRAY);
        DrawText("Service: -- / VEHICLE USED: N/A", debugX, 251, 15, LIGHTGRAY);
    }
    if (game.lastConsumedVehicle())
        DrawText(TextFormat("Last used ID %u / Completed: %s", static_cast<unsigned>(*game.lastConsumedVehicle()), game.serviceCompleted(*game.lastConsumedVehicle())?"YES":"NO"), debugX, 272, 15, ORANGE);
    else DrawText("Last used: none", debugX, 272, 15, LIGHTGRAY);
    const auto& attempt = game.transferDebug();
    DrawText(lc::outcomeName(attempt.outcome), debugX, 297, 15, game.attachedVehicle() ? accent : RAYWHITE);
    if (attempt.targetAtStart) {
        const auto& target = *attempt.targetAtStart;
        DrawText(TextFormat("Target ID: %u / %s / Limit: %.0f", static_cast<unsigned>(target.id),
                 lc::vehicleName(target.type), config.vehicleSpecs[static_cast<std::size_t>(target.type)].maxBoardingRelativeSpeed), debugX, 317, 15, vehicleColor(target.type));
        DrawText(TextFormat("Window: %.3f-%.3fs / Penalty: %.1f", attempt.prediction.enterTime,
                 attempt.prediction.exitTime, attempt.penaltyApplied), debugX, 337, 15, LIGHTGRAY);
    } else {
        DrawText(attempt.excludedUsedVehicle?"TARGET NONE / VEHICLE_ALREADY_USED":"No target / bright box = Catch Zone", debugX, 317, 15, LIGHTGRAY);
        DrawText(TextFormat("Last miss penalty: %.1f", attempt.penaltyApplied), debugX, 337, 15, LIGHTGRAY);
    }
    for (int i = 0; i < 2; ++i) {
        const auto lane = i == 0 ? lc::Lane::Left : lc::Lane::Right;
        const auto& info = traffic.laneDebug(lane);
        const int y = 366 + i * 57;
        DrawText(TextFormat("%s: %u active / Pending: %s", lc::laneName(lane), static_cast<unsigned>(info.activeCount),
                 info.pendingVehicle ? lc::vehicleName(*info.pendingVehicle) : "--"), debugX, y, 15, RAYWHITE);
        DrawText(TextFormat("Spawn: %s / Next: %.2fs", info.blocked ? "BLOCKED" : "READY", info.nextSpawnTime), debugX, y + 18, 14, info.blocked ? ORANGE : LIGHTGRAY);
        DrawText(TextFormat("Reason: %s", info.blockReason), debugX, y + 35, 14, LIGHTGRAY);
    }
    DrawText(traffic.encounterDebug().status, debugX, 481, 14, accent);
    // Additional diagnostics live outside the unchanged 960x540 game area.
    constexpr int dx=975;
    DrawLine(950,85,950,635,Color{54,66,80,255});
    DrawText("LAST TRANSFER / SNAPSHOT",dx,90,20,accent);
    const auto& report=game.lastBoardingDebug();
    DrawText(TextFormat("Start: %.2fs / Duration: %.2fs",report.startTime,report.duration),dx,120,16,LIGHTGRAY);
    if(report.diagnosticTarget) {
        const auto& target=*report.diagnosticTarget;
        DrawText(TextFormat("TARGET: %s / ID %u",lc::vehicleName(target.type),static_cast<unsigned>(target.id)),dx,149,18,vehicleColor(target.type));
        DrawText(report.targetAtStart?(report.diagnosticPrediction.speedAllowed?"Eligible target":"Rejected car / diagnostic only"):"Nearest car / diagnostic only",dx,173,14,LIGHTGRAY);
        if (report.intersectionTime) {
            DrawText(TextFormat("INTERSECTION: %.4fs", *report.intersectionTime),dx,201,16,RAYWHITE);
            DrawText(TextFormat("PLAYER AT HIT: %.3f / CAR: %.0f",report.playerSpeedAtIntersection,target.speed),dx,224,16,RAYWHITE);
            DrawText(TextFormat("ABS DIFF: %.3f / LIMIT: %.0f",report.absoluteDifferenceAtIntersection,report.allowedDifference),dx,247,16,RAYWHITE);
            DrawText(report.speedPassedAtIntersection?"SPEED CHECK: PASS":"SPEED CHECK: FAIL",dx,295,18,report.speedPassedAtIntersection?accent:ORANGE);
        } else {
            DrawText(report.diagnosticPrediction.intersects?"INTERSECTION: pending":"INTERSECTION: none",dx,201,16,LIGHTGRAY);
            DrawText(TextFormat("TARGET SPEED: %.0f / LIMIT: %.0f",target.speed,report.allowedDifference),dx,224,16,RAYWHITE);
            DrawText("PLAYER AT HIT / ABS DIFF: --",dx,247,16,LIGHTGRAY);
            DrawText("SPEED CHECK: N/A (no event yet)",dx,295,16,LIGHTGRAY);
        }
        DrawText(game.vehicleConsumed(target.id)?"VEHICLE USED: YES":"VEHICLE USED: NO",dx,270,16,LIGHTGRAY);
        DrawText("PLAYER FORWARD RANGE",dx,330,16,LIGHTGRAY);
        DrawText(TextFormat("%.2f -> %.2f",report.startPosition,report.endPosition),dx,351,17,RAYWHITE);
        DrawText("TARGET CATCH ZONE (world units)",dx,382,16,LIGHTGRAY);
        DrawText(TextFormat("t=0:    [%.2f, %.2f]",report.zoneStartRear,report.zoneStartFront),dx,405,16,RAYWHITE);
        DrawText(TextFormat("t=%.2f: [%.2f, %.2f]",report.duration,report.zoneEndRear,report.zoneEndFront),dx,428,16,RAYWHITE);
        if(report.diagnosticPrediction.intersects)
            DrawText(TextFormat("OVERLAP: %.3f -> %.3f s",report.diagnosticPrediction.enterTime,report.diagnosticPrediction.exitTime),dx,461,17,RAYWHITE);
        else DrawText("OVERLAP: none / start -- / end --",dx,461,16,LIGHTGRAY);
        DrawText(report.intersectionTime?"POSITION CHECK: PASS":report.diagnosticPrediction.intersects?"POSITION CHECK: PENDING":"POSITION CHECK: FAIL",dx,488,18,report.intersectionTime?accent:ORANGE);
    } else {
        DrawText("TARGET: NONE",dx,149,17,LIGHTGRAY);
        DrawText(report.excludedUsedVehicle?"REASON: VEHICLE_ALREADY_USED":"No unused car in target lane",dx,178,16,LIGHTGRAY);
        DrawText(TextFormat("PLAYER SPEED: %.2f -> %.2f",report.startSpeed,report.endSpeed),dx,201,16,RAYWHITE);
        DrawText("SPEED CHECK: N/A",dx,295,18,LIGHTGRAY);
        DrawText("PLAYER FORWARD RANGE",dx,330,16,LIGHTGRAY);
        DrawText(TextFormat("%.2f -> %.2f",report.startPosition,report.endPosition),dx,351,17,RAYWHITE);
        DrawText("TARGET CATCH ZONE: --",dx,405,16,LIGHTGRAY);
        DrawText(report.duration>0?"POSITION CHECK: FAIL / no target":"POSITION CHECK: N/A",dx,488,16,LIGHTGRAY);
    }
    const bool succeeded=report.outcome==lc::TransferOutcome::Success || report.outcome==lc::TransferOutcome::Following;
    const bool failed=report.outcome==lc::TransferOutcome::Miss || report.outcome==lc::TransferOutcome::SpeedRejected;
    DrawText(succeeded?"FINAL RESULT: SUCCESS":failed?"FINAL RESULT: MISS":report.outcome==lc::TransferOutcome::None?"FINAL RESULT: --":"FINAL RESULT: PENDING",dx,526,20,succeeded?accent:failed?ORANGE:RAYWHITE);
    DrawText("Ranges predict the unmounted path.",dx,567,15,LIGHTGRAY);
    DrawText("Speed gate: first actual intersection.",dx,588,15,LIGHTGRAY);
    DrawText(TextFormat("Deceleration: %.0f/s (active: %.0f/s)",config.normalDeceleration,!game.attachedVehicle() && player.forwardSpeed()>config.normalRunSpeed?config.normalDeceleration:0),dx,609,15,LIGHTGRAY);
    if (game.timeSinceVehicleExit()) DrawText(TextFormat("Time since vehicle exit: %.2fs",*game.timeSinceVehicleExit()),dx,632,15,LIGHTGRAY);
    else DrawText("Time since vehicle exit: --",dx,632,15,LIGHTGRAY);

    DrawText("VEHICLE SPAWN STATISTICS",28,550,18,accent);
    DrawText("TYPE",28,576,15,LIGHTGRAY);
    DrawText("LEFT",150,576,15,LIGHTGRAY); DrawText("RIGHT",220,576,15,LIGHTGRAY);
    for(std::size_t t=0;t<4;++t) {
        const int y=597+static_cast<int>(t)*20;
        DrawText(lc::vehicleName(static_cast<lc::VehicleType>(t)),28,y,15,RAYWHITE);
        DrawText(TextFormat("%u",static_cast<unsigned>(traffic.laneDebug(lc::Lane::Left).spawnCounts[t])),150,y,15,RAYWHITE);
        DrawText(TextFormat("%u",static_cast<unsigned>(traffic.laneDebug(lc::Lane::Right).spawnCounts[t])),220,y,15,RAYWHITE);
    }
    for(int i=0;i<2;++i) {
        const auto lane=i?lc::Lane::Right:lc::Lane::Left;
        const auto& state=traffic.laneDebug(lane);
        DrawText(TextFormat("LAST %s: %s",lc::laneName(lane),state.lastSpawnedType?lc::vehicleName(*state.lastSpawnedType):"--"),330,580+i*25,16,RAYWHITE);
    }
    DrawText("ENCOUNTER CANDIDATES",600,550,17,accent);
    const auto& immediate=traffic.encounterDebug().immediateCandidates;
    const auto& prepared=traffic.encounterDebug().preparedCandidates;
    DrawText("Immediate",600,598,15,RAYWHITE); DrawText("Prepared",600,623,15,RAYWHITE);
    for(int t=0;t<4;++t) {
        DrawText(lc::vehicleName(static_cast<lc::VehicleType>(t)),714+t*60,576,11,LIGHTGRAY);
        DrawText(TextFormat("%u",static_cast<unsigned>(immediate[t])),714+t*60,598,15,RAYWHITE);
        DrawText(TextFormat("%u",static_cast<unsigned>(prepared[t])),714+t*60,623,15,RAYWHITE);
    }
    DrawText("Active, unconsumed / forecast checked",600,650,14,LIGHTGRAY);
    DrawRectangle(28, 488, 530, 7, Color{54,66,80,255});
    DrawRectangle(28, 488, static_cast<int>(530 * race.progress()), 7, accent);
    DrawText(TextFormat("AIRPORT: %.1f units left / %.0f%%", race.remainingDistance(), race.progress() * 100), 28, 504, 17, RAYWHITE);
    DrawText(TextFormat("BEST %s%s", bestTime ? lc::formatRaceTime(*bestTime).c_str() : "--:--.--", storageAvailable ? "" : " (session)"), 600, 504, 17, accent);
    if (race.late() && !race.finished()) DrawText("MISSED FLIGHT - keep going for your finish time", 28, 75, 14, ORANGE);

    if (!started || race.finished()) {
        DrawRectangle(0, 0, 1360, 690, Fade(background, .96f));
        if (!started) {
            DrawText("LAST CONNECTION", 275, 102, 32, RAYWHITE);
            DrawText("Reach the airport. Beat your time.", 280, 166, 21, accent);
            DrawText("LEFT / RIGHT: one lane at a time", 280, 210, 20, RAYWHITE);
            DrawText("Catch the bright zone during your transfer.", 240, 244, 19, LIGHTGRAY);
            DrawText(TextFormat("Airport %.0f units / Deadline %s", config.finishDistance,
                lc::formatRaceTime(config.deadline).c_str()), 260, 288, 19, RAYWHITE);
            DrawText("Late arrivals still finish, with rank F.", 280, 320, 18, LIGHTGRAY);
            DrawText(TextFormat("BEST %s", bestTime ? lc::formatRaceTime(*bestTime).c_str() : "--:--.--"), 360, 356, 18, accent);
        } else {
            const Color resultColor = race.rank() == 'F' ? ORANGE : accent;
            const char* headline = race.rank() == 'F' ? "MISSED FLIGHT" : "AIRPORT REACHED";
            DrawText(headline, 480 - MeasureText(headline, 32) / 2, 78, 32, resultColor);
            DrawText(TextFormat("RANK %c", race.rank()), 400, 140, 44, resultColor);
            DrawText("FINISH TIME", 390, 210, 22, LIGHTGRAY);
            const auto time = lc::formatRaceTime(*race.finishTime());
            DrawText(time.c_str(), 480 - MeasureText(time.c_str(), 40) / 2, 243, 40, RAYWHITE);
            DrawText(TextFormat("BEST TIME  %s", lc::formatRaceTime(*bestTime).c_str()), 340, 303, 22, accent);
            DrawText(newBest ? "NEW PERSONAL BEST" : "Try another connection.", 350, 342, 18, LIGHTGRAY);
        }
        const bool hover = CheckCollisionPointRec(GetMousePosition(), actionButton);
        DrawRectangleRec(actionButton, hover ? accent : Color{65,132,112,255});
        const char* label = started ? "RUN AGAIN" : "START RUN";
        DrawText(label, 480 - MeasureText(label, 23) / 2, 406, 23, background);
        DrawText("Click the button or press LEFT / RIGHT", 285, 459, 18, LIGHTGRAY);
        DrawText(storageAvailable ? "Game time / best saved on this device" : "Game time / storage unavailable: best kept for this session",
            storageAvailable ? 304 : 200, 492, 15, LIGHTGRAY);
    }
    EndDrawing();
}

void frame() {
    // Consume press events rather than comparing held states. A fast tap can
    // start and end between frames. raylib queues PRESS events, not repeats.
    // Drain the queue now so inputs during a transfer cannot fire on arrival.
    bool left = false;
    bool right = false;
    for (int key = GetKeyPressed(); key != 0; key = GetKeyPressed()) {
        left = left || key == KEY_LEFT;
        right = right || key == KEY_RIGHT;
    }
    const bool action = (left != right) || (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), actionButton));
    if (!started || race.finished()) {
        if (action) {
            if (started) race.restart(newTrafficSeed());
            started = true;
            resultRecorded = false;
            newBest = false;
            frameClock = lc::FrameClock(config);
        }
        // The start/retry press never also performs a lane transfer, and the
        // menu's frame delta never enters the new run's time accumulator.
        drawSandbox();
        return;
    }
    if (left != right) race.requestTransfer(left ? -1 : 1);

    const int steps = frameClock.advance(GetFrameTime());
    for (int step = 0; step < steps; ++step) {
        race.update(player.config().fixedStepSeconds);
    }
    if (race.finished() && !resultRecorded) {
        const double time = *race.finishTime();
        newBest = !bestTime || time < *bestTime;
        if (newBest) { bestTime = time; storageAvailable = lc::saveLocalBest(time, config); }
        resultRecorded = true;
    }
    drawSandbox();
}
}

int main(int argc, char** argv) {
    InitWindow(1360, 690, "LAST CONNECTION - Time Attack");
    const auto stored = lc::loadLocalBest(config);
    bestTime = stored.time;
    storageAvailable = stored.storageAvailable;
#ifdef __EMSCRIPTEN__
    (void)argc;
    (void)argv;
    emscripten_set_main_loop(frame, 0, 1);
#else
    SetTargetFPS(60);
    const bool smoke = argc > 1 && std::strcmp(argv[1], "--smoke") == 0;
    int frames = 0;
    while (!WindowShouldClose() && (!smoke || frames < 120)) {
        frame();
        ++frames;
    }
    if (smoke) TakeScreenshot("sandbox.png");
    CloseWindow();
#endif
    return 0;
}
