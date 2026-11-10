#pragma once
#include "localization.hpp"

namespace lc::ui {
// Presentation only. No gameplay configuration or simulation state lives here.
enum class Screen { Start, Playing, Results, Help, Language };
enum class Action { None, StartRun, RestartRun, MenuOnly };
struct Flow {
    Screen screen = Screen::Language;
    Language language = Language::English;
    void selectLanguage(Language selected) {
        if (screen == Screen::Language) { language = selected; screen = Screen::Start; }
    }
    bool debug = false;
    Action input(bool start, bool help, bool back, bool toggleDebug) {
        if (screen == Screen::Language) return Action::MenuOnly;
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
}
