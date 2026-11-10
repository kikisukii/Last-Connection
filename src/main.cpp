#include "time_attack.hpp"
#include "best_time_storage.hpp"
#include "frame_clock.hpp"
#include "road_view.hpp"
#include "onboarding.hpp"
#include <string>
#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#include <vector>
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
lc::ui::Flow ui;
int pendingMenuAction = 0;
int pendingLanguage = -1;
Font scFont{}, jpFont{};
constexpr Rectangle languageButtons[]{{250,320,260,60},{550,320,260,60},{850,320,260,60}};
const char* text(const char* key) { return lc::ui::getText(ui.language,key); }
Font uiFont() { return ui.language==lc::ui::Language::Japanese ? jpFont : scFont; }
void uiDraw(const char* value, int x, int y, int size, Color color) {
    DrawTextEx(uiFont(),value,Vector2{static_cast<float>(x),static_cast<float>(y)},static_cast<float>(size),1,color);
}
int uiMeasure(const char* value, int size) {
    return static_cast<int>(std::ceil(MeasureTextEx(uiFont(),value,static_cast<float>(size),1).x));
}
void loadUiFonts() {
    std::string all;
    for(const auto& entry : lc::ui::translations) for(const char* value : entry.values) all+=value;
    for(int c=32;c<127;++c) all+=static_cast<char>(c);
    all+="←→";
    int count=0; int* points=LoadCodepoints(all.c_str(),&count);
    std::vector<int> unique(points,points+count); UnloadCodepoints(points);
    std::sort(unique.begin(),unique.end()); unique.erase(std::unique(unique.begin(),unique.end()),unique.end());
    scFont=LoadFontEx("assets/fonts/LC-UI-SC.otf",40,unique.data(),static_cast<int>(unique.size()));
    jpFont=LoadFontEx("assets/fonts/LC-UI-JP.otf",40,unique.data(),static_cast<int>(unique.size()));
    SetTextureFilter(scFont.texture,TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(jpFont.texture,TEXTURE_FILTER_BILINEAR);
}
const char* publicVehicleName(lc::VehicleType type) {
    return text(std::array{"bike","bus","taxi","express"}[static_cast<unsigned>(type)]);
}
[[maybe_unused]] constexpr Rectangle actionButton{350, 393, 260, 48};
constexpr Rectangle startButton{550, 620, 260, 48};
[[maybe_unused]] constexpr Rectangle helpButton{350, 453, 260, 40};
constexpr Rectangle backButton{850, 620, 160, 48};
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
        uiDraw(game.serviceConsumed(vehicle.id) ? text("used") : publicVehicleName(vehicle.type), static_cast<int>(x - 40), static_cast<int>(y - 15), 16, color);
        uiDraw(TextFormat(text("speed"), vehicle.speed), static_cast<int>(x - 35), static_cast<int>(y + 3), 14, RAYWHITE);
    }
    EndScissorMode();
}

void centered(const char* value, int x, int y, int size, Color color) {
    uiDraw(value,x-uiMeasure(value,size)/2,y,size,color);
}
int wrapped(const char* value, int x, int y, int width, int size, Color color) {
    // UTF-8 tokens: Latin words stay together; CJK breaks only between codepoints.
    std::vector<std::string> tokens;
    std::string word;
    for(const char* cursor=value; *cursor;) {
        int bytes=0; const int cp=GetCodepointNext(cursor,&bytes);
        if(cp>=128 || cp==' ') {
            if(!word.empty()) { tokens.push_back(word); word.clear(); }
            tokens.emplace_back(cursor,bytes);
        } else word.append(cursor,bytes);
        cursor+=bytes;
    }
    if(!word.empty()) tokens.push_back(word);
    std::string line;
    for(const auto& token : tokens) {
        if(!line.empty() && uiMeasure((line+token).c_str(),size)>width) {
            uiDraw(line.c_str(),x,y,size,color); y+=size+8; line.clear();
        }
        if(!line.empty() || token!=" ") line+=token;
    }
    uiDraw(line.c_str(),x,y,size,color);
    return y+size+8;
}
void button(Rectangle box, const char* label, bool primary=true) {
    const bool hover=CheckCollisionPointRec(GetMousePosition(),box);
    DrawRectangleRec(box, primary ? (hover?accent:Color{65,132,112,255}) : Color{29,39,51,255});
    if(!primary) DrawRectangleLinesEx(box,1,accent);
    centered(label,static_cast<int>(box.x+box.width/2),static_cast<int>(box.y+box.height/2-10),20,primary?background:accent);
}
[[maybe_unused]] void drawLanguageSelection() {
    centered(text("title"),680,175,42,RAYWHITE);
    centered(text("select"),680,245,24,accent);
    for(int i=0;i<3;++i) button(languageButtons[i],text(std::array{"zh","en","ja"}[i]));
}
[[maybe_unused]] void drawOnboarding() {
    centered(text("title"),680,24,42,RAYWHITE);
    centered(text("subtitle"),680,78,18,LIGHTGRAY);
    centered(text("how"),680,109,22,accent);
    centered(TextFormat("%s / %s",text("goalLabel"),text("goal")),680,145,20,RAYWHITE);
    for(int x : {40,480,920}) DrawRectangle(x,188,400,312,Color{29,39,51,255});
    uiDraw("← / →",66,207,27,accent);
    int y=wrapped(TextFormat("%s / %s",text("controlsLabel"),text("controls")),66,248,350,20,RAYWHITE);
    y=wrapped(text("center"),66,y+20,350,20,accent);
    y=wrapped(text("run"),66,y,350,20,RAYWHITE);
    y=wrapped(text("sides"),66,y+16,350,20,accent);
    wrapped(text("vehicles"),66,y,350,20,RAYWHITE);
    y=wrapped(text("predict"),502,208,354,21,accent);
    y=wrapped(text("early"),502,y+6,354,19,RAYWHITE);
    y=wrapped(text("aim"),502,y+5,354,19,RAYWHITE);
    y=wrapped(text("boarding"),502,y+16,354,21,accent);
    wrapped(text("automatic"),502,y+5,354,19,RAYWHITE);
    y=wrapped(text("miss"),942,208,354,21,accent);
    y=wrapped(text("recover"),942,y+6,354,19,RAYWHITE);
    y=wrapped(text("temporary"),942,y+16,354,20,accent);
    y=wrapped(text("once"),942,y+5,354,19,RAYWHITE);
    wrapped(text("next"),942,y+5,354,19,RAYWHITE);
    centered(text("deadline"),680,512,21,RAYWHITE);
    centered(text("rank"),680,542,18,LIGHTGRAY);
    centered(text("ranks"),680,568,23,accent);
    centered(text("rankHint"),680,596,16,LIGHTGRAY);
    button(startButton,text(ui.screen==lc::ui::Screen::Help?"again":"start"));
    if(ui.screen==lc::ui::Screen::Help) button(backButton,text("back"),false);
    uiDraw(text(ui.screen==lc::ui::Screen::Help?"againHint":"startHint"),36,644,16,LIGHTGRAY);
}
#ifdef __EMSCRIPTEN__
EM_JS(void, webCopy, (const char* key, const char* value), {
    window.lcMenu.copy[UTF8ToString(key)]=UTF8ToString(value);
    document.querySelectorAll('[data-copy="'+UTF8ToString(key)+'"]').forEach(e=>e.textContent=UTF8ToString(value));
});
EM_JS(void, webMenu, (int screen, const char* time, int rank, const char* best, int fresh), {
    window.lcMenu.show(screen,UTF8ToString(time),String.fromCharCode(rank),UTF8ToString(best),!!fresh);
});
EM_JS(void, webLanguage, (const char* tag), {
    document.documentElement.lang=UTF8ToString(tag);
    canvas.setAttribute('aria-label',window.lcMenu.copy.gameLabel);
});
void updateWebLanguage() {
    for(const auto& item : lc::ui::translations) webCopy(item.key,text(item.key));
    webLanguage(lc::ui::languageTag(ui.language));
}
void syncWebMenu() {
    static int previous=-1;
    const int screen=static_cast<int>(ui.screen);
    if(previous==screen) return;
    previous=screen;
    const auto finish=race.finishTime()?lc::formatRaceTime(*race.finishTime()):"--:--.--";
    const auto best=bestTime?lc::formatRaceTime(*bestTime):"--:--.--";
    webMenu(screen,finish.c_str(),race.finished()?race.rank():' ',best.c_str(),newBest);
}
#endif

void drawSandbox() {
    const auto vehicles = traffic.current();
    BeginDrawing();
    ClearBackground(background);
    if(ui.screen==lc::ui::Screen::Language) {
#ifndef __EMSCRIPTEN__
        drawLanguageSelection();
#endif
        EndDrawing(); return;
    }
    if(ui.screen==lc::ui::Screen::Start || ui.screen==lc::ui::Screen::Help) {
#ifndef __EMSCRIPTEN__
        drawOnboarding();
#endif
        EndDrawing(); return;
    }
    DrawText("LAST CONNECTION", 28, 20, 28, RAYWHITE);
    if(ui.debug) DrawText(TextFormat("V0.3 / MOVEMENT FEEL / SEED %u", static_cast<unsigned>(traffic.seed())), 28, 54, 16, LIGHTGRAY);
    uiDraw(TextFormat("%s %s", text("time"), lc::formatRaceTime(race.elapsed()).c_str()), 600, 18, 22, RAYWHITE);
    uiDraw(TextFormat("%s %s", text("deadlineLabel"), lc::formatRaceTime(config.deadline).c_str()), 600, 49, 18, race.late() ? ORANGE : LIGHTGRAY);
    for (int lane = -1; lane <= 1; ++lane) {
        const float x = roadCenter + lane * lanePixels;
        DrawRectangle(static_cast<int>(x - 65), 112, 130, 358, Color{29, 39, 51, 255});
        uiDraw(text(std::array{"left","middle","right"}[lane+1]), static_cast<int>(x - 35), 86, 16, LIGHTGRAY);
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
        uiDraw(TextFormat("%s / %s",text("airport"),text("finish")), 240, static_cast<int>(finishY - 24), 16, accent);
    }
    const float x = roadCenter + static_cast<float>(player.lateralPosition() / player.config().laneSpacing) * lanePixels;
    DrawCircleV(Vector2{x, playerScreenY}, 14.0f, accent);
    DrawTriangle(Vector2{x, playerScreenY - 26}, Vector2{x - 6, playerScreenY - 17},
                 Vector2{x + 6, playerScreenY - 17}, accent);

    if(ui.debug) {
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
        DrawText(TextFormat("Ride time: %.2f / %.2fs", game.elapsedRideTime(), game.currentRideDuration()), debugX, 250, 16, accent);
        DrawText(TextFormat("Time remaining: %.2fs", game.remainingRideTime()), debugX, 270, 16, accent);
        DrawText(TextFormat("Distance remaining: %.1f / USED", game.remainingRideDistance()), debugX, 290, 15, accent);
    } else {
        DrawText("Current ID: -- / Speed: --", debugX, 230, 16, LIGHTGRAY);
        DrawText("Ride time: -- / --s", debugX, 250, 16, LIGHTGRAY);
        DrawText("Time remaining: --s", debugX, 270, 16, LIGHTGRAY);
        DrawText("Distance remaining: -- / USED: N/A", debugX, 290, 15, LIGHTGRAY);
    }
    if (game.lastConsumedVehicle())
        DrawText(TextFormat("Last used ID %u / Completed: %s", static_cast<unsigned>(*game.lastConsumedVehicle()), game.serviceCompleted(*game.lastConsumedVehicle())?"YES":"NO"), debugX, 312, 15, ORANGE);
    else DrawText("Last used: none", debugX, 312, 15, LIGHTGRAY);
    const auto& attempt = game.transferDebug();
    DrawText(lc::outcomeName(attempt.outcome), debugX, 337, 15, game.attachedVehicle() ? accent : RAYWHITE);
    if (attempt.targetAtStart) {
        const auto& target = *attempt.targetAtStart;
        DrawText(TextFormat("Target ID: %u / %s / Limit: %.0f", static_cast<unsigned>(target.id),
                 lc::vehicleName(target.type), config.vehicleSpecs[static_cast<std::size_t>(target.type)].maxBoardingRelativeSpeed), debugX, 357, 15, vehicleColor(target.type));
        DrawText(TextFormat("Window: %.3f-%.3fs / Penalty: %.1f", attempt.prediction.enterTime,
                 attempt.prediction.exitTime, attempt.penaltyApplied), debugX, 377, 15, LIGHTGRAY);
    } else {
        DrawText(attempt.excludedUsedVehicle?"TARGET NONE / VEHICLE_ALREADY_USED":"No target / bright box = Catch Zone", debugX, 357, 15, LIGHTGRAY);
        DrawText(TextFormat("Last miss penalty: %.1f", attempt.penaltyApplied), debugX, 377, 15, LIGHTGRAY);
    }
    for (int i = 0; i < 2; ++i) {
        const auto lane = i == 0 ? lc::Lane::Left : lc::Lane::Right;
        const auto& info = traffic.laneDebug(lane);
        const int y = 406 + i * 57;
        DrawText(TextFormat("%s: %u active / Pending: %s", lc::laneName(lane), static_cast<unsigned>(info.activeCount),
                 info.pendingVehicle ? lc::vehicleName(*info.pendingVehicle) : "--"), debugX, y, 15, RAYWHITE);
        DrawText(TextFormat("Spawn: %s / Next: %.2fs", info.blocked ? "BLOCKED" : "READY", info.nextSpawnTime), debugX, y + 18, 14, info.blocked ? ORANGE : LIGHTGRAY);
        DrawText(TextFormat("Reason: %s", info.blockReason), debugX, y + 35, 14, LIGHTGRAY);
    }
    DrawText(traffic.encounterDebug().status, debugX, 521, 14, accent);
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
    } else {
        uiDraw(text("moveHint"),600,140,23,accent);
        uiDraw(text("rideHint"),600,185,20,RAYWHITE);
        uiDraw(text("missHint"),600,220,20,RAYWHITE);
    }
    uiDraw(text("debugHint"),1160,665,14,LIGHTGRAY);
    DrawRectangle(28, 488, 530, 7, Color{54,66,80,255});
    DrawRectangle(28, 488, static_cast<int>(530 * race.progress()), 7, accent);
    uiDraw(TextFormat(text("progress"), race.remainingDistance(), race.progress() * 100), 28, 504, 17, RAYWHITE);
    uiDraw(TextFormat("%s %s%s", text("best"), bestTime ? lc::formatRaceTime(*bestTime).c_str() : "--:--.--", storageAvailable ? "" : text("sessionSuffix")), 330, 635, 17, accent);
    if (race.late() && !race.finished()) uiDraw(text("lateHint"), 28, 75, 14, ORANGE);

    if (ui.screen==lc::ui::Screen::Results) {
        DrawRectangle(0, 0, 1360, 690, background);
#ifndef __EMSCRIPTEN__

        const Color resultColor=race.rank()=='F'?ORANGE:accent;
        centered(text(race.rank()=='F'?"missed":"reached"),480,78,32,resultColor);
        centered(TextFormat("%s %c",text("rankLabel"),race.rank()),480,140,44,resultColor);
        centered(text("finishTime"),480,210,22,LIGHTGRAY);
        centered(lc::formatRaceTime(*race.finishTime()).c_str(),480,243,40,RAYWHITE);
        centered(TextFormat("%s  %s",text("bestTime"),lc::formatRaceTime(*bestTime).c_str()),480,303,22,accent);
        centered(text(newBest?"newBest":"tryAgain"),480,342,18,LIGHTGRAY);
        button(actionButton,text("again"));
        button(helpButton,text("how"),false);
        centered(text("againHint"),480,519,18,LIGHTGRAY);
        centered(text(storageAvailable?"saved":"session"),480,554,15,LIGHTGRAY);
#endif
    }
    EndDrawing();
}

void frame() {
    // Consume press events rather than comparing held states. A fast tap can
    // start and end between frames. raylib queues PRESS events, not repeats.
    // Drain the queue now so inputs during a transfer cannot fire on arrival.
    bool left=false, right=false, toggleDebug=false;
    for (int key=GetKeyPressed(); key!=0; key=GetKeyPressed()) {
        left=left || key==KEY_LEFT;
        right=right || key==KEY_RIGHT;
        toggleDebug=toggleDebug || key==KEY_F1;
    }
    bool start=(left!=right) || pendingMenuAction==1;
    bool help=pendingMenuAction==2, back=pendingMenuAction==3;
    pendingMenuAction=0;
#ifndef __EMSCRIPTEN__
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const auto mouse=GetMousePosition();
        const bool tutorial=ui.screen==lc::ui::Screen::Start || ui.screen==lc::ui::Screen::Help;
        start=start || CheckCollisionPointRec(mouse,tutorial?startButton:actionButton);
        help=help || CheckCollisionPointRec(mouse,helpButton);
        back=back || CheckCollisionPointRec(mouse,backButton);
    }
#endif
    if(ui.screen==lc::ui::Screen::Language) {
#ifndef __EMSCRIPTEN__
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            for(int i=0;i<3;++i) if(CheckCollisionPointRec(GetMousePosition(),languageButtons[i])) pendingLanguage=i;
#endif
        if(pendingLanguage>=0 && pendingLanguage<3) {
            ui.selectLanguage(static_cast<lc::ui::Language>(pendingLanguage));
#ifdef __EMSCRIPTEN__
            updateWebLanguage();
#endif
        }
        pendingLanguage=-1;
#ifdef __EMSCRIPTEN__
        syncWebMenu();
#endif
        drawSandbox(); return;
    }
    const auto action=ui.input(start,help,back,toggleDebug);
    if(action!=lc::ui::Action::None) {
        if(action==lc::ui::Action::StartRun || action==lc::ui::Action::RestartRun) {
            if(action==lc::ui::Action::RestartRun) race.restart(newTrafficSeed());
            resultRecorded=false; newBest=false; frameClock=lc::FrameClock(config);
        }
#ifdef __EMSCRIPTEN__
        syncWebMenu();
#endif
        // A menu input is fully consumed: never transfer or advance simulation
        // using the menu frame's delta, including keyboard start/restart.
        drawSandbox(); return;
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
        ui.finish();
    }
#ifdef __EMSCRIPTEN__
    syncWebMenu();
#endif
    drawSandbox();
}
}
#ifdef __EMSCRIPTEN__
extern "C" EMSCRIPTEN_KEEPALIVE void select_language(int language) {
    if(language>=0 && language<3) pendingLanguage=language;
}
extern "C" EMSCRIPTEN_KEEPALIVE void request_menu_action(int action) {
    if(action>=1 && action<=3) pendingMenuAction=action;
}
#endif

int main(int argc, char** argv) {
    InitWindow(1360, 690, "LAST CONNECTION - Time Attack");
    loadUiFonts();
    const auto stored = lc::loadLocalBest(config);
    bestTime = stored.time;
    storageAvailable = stored.storageAvailable;
#ifdef __EMSCRIPTEN__
    (void)argc;
    (void)argv;
    updateWebLanguage();
    syncWebMenu();
    emscripten_set_main_loop(frame, 0, 1);
#else
    SetTargetFPS(60);
    const bool smoke = argc > 1 && std::strcmp(argv[1], "--smoke") == 0;
    // Screenshot-only UI selection; no gameplay/config overrides.
    if(smoke && argc>2) {
        const std::string language=argv[2];
        if(language=="zh" || language=="en" || language=="ja")
            ui.selectLanguage(language=="zh"?lc::ui::Language::Chinese:language=="ja"?lc::ui::Language::Japanese:lc::ui::Language::English);
    }
    int frames = 0;
    while (!WindowShouldClose() && (!smoke || frames < 120)) {
        frame();
        ++frames;
    }
    if (smoke) TakeScreenshot("sandbox.png");
    UnloadFont(scFont); UnloadFont(jpFont);
    CloseWindow();
#endif
    return 0;
}
