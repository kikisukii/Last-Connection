#pragma once
#include <array>

namespace lc::ui {
// Presentation only. No gameplay configuration or simulation state lives here.
enum class Screen { Start, Playing, Results, Help };
enum class Action { None, StartRun, RestartRun, MenuOnly };
struct Flow {
    Screen screen = Screen::Start;
    bool debug = false;
    Action input(bool start, bool help, bool back, bool toggleDebug) {
        if (screen == Screen::Playing) {
            if (toggleDebug) debug = !debug;
            return Action::None;
        }
        if (screen == Screen::Results && help) { screen = Screen::Help; return Action::MenuOnly; }
        if (screen == Screen::Help && back) { screen = Screen::Results; return Action::MenuOnly; }
        if (!start) return Action::MenuOnly;
        const bool first = screen == Screen::Start;
        screen = Screen::Playing;
        return first ? Action::StartRun : Action::RestartRun;
    }
    void finish() { screen = Screen::Results; }
};
struct Copy { const char* key; const char* text; };
inline constexpr std::array copy{
    Copy{"title", "LAST CONNECTION"},
    Copy{"subtitle", "A TWO-BUTTON TIME ATTACK"},
    Copy{"how", "HOW TO PLAY"},
    Copy{"goal", "REACH THE AIRPORT AS FAST AS YOU CAN."},
    Copy{"controls", "CHANGE TO AN ADJACENT LANE"},
    Copy{"center", "CENTER"},
    Copy{"run", "Run at normal speed."},
    Copy{"sides", "LEFT / RIGHT"},
    Copy{"vehicles", "Ride passing vehicles to gain speed."},
    Copy{"predict", "PREDICT THE INTERSECTION"},
    Copy{"early", "Don't wait until you're next to a vehicle."},
    Copy{"aim", "Move early and aim for where it will be."},
    Copy{"boarding", "BOARDING"},
    Copy{"automatic", "If your timing and speed are right, you'll board automatically."},
    Copy{"miss", "MISS?"},
    Copy{"recover", "Keep running. You'll lose speed, but you can recover."},
    Copy{"temporary", "EVERY RIDE IS TEMPORARY."},
    Copy{"once", "Each vehicle can only be used once."},
    Copy{"next", "Look for your next connection."},
    Copy{"deadline", "REACH THE AIRPORT BEFORE THE DEADLINE."},
    Copy{"rank", "Finish faster to improve your rank."},
    Copy{"ranks", "S  A  B  C  D  F"},
    Copy{"rankHint", "S = fastest    F = missed flight"}
};
}
