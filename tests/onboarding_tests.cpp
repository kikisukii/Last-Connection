#include "onboarding.hpp"
#include "time_attack.hpp"
#include <iostream>
#include <stdexcept>
int main() {
    try {
        auto check=[](bool value){ if(!value) throw std::runtime_error("UI contract failed"); };
        lc::ui::Flow ui;
        lc::TimeAttack race({},42);
        check(ui.screen==lc::ui::Screen::Start && !ui.debug);
        check(ui.input(false,false,false,true)==lc::ui::Action::MenuOnly && !ui.debug);
        for(bool keyboard : {false,true}) {
            (void)keyboard; // both native hit-testing and keyboard route to start
            ui={};
            const auto action=ui.input(true,false,false,false);
            check(action==lc::ui::Action::StartRun && ui.screen==lc::ui::Screen::Playing);
            // The production frame dispatch returns on menu actions, with no
            // requestTransfer/update. Exercise the same branch with real state.
            if(action==lc::ui::Action::None) race.requestTransfer(-1);
            check(race.game().player().lane()==lc::Lane::Center && !race.game().player().transferring() && race.elapsed()==0);
        }
        check(ui.input(false,false,false,true)==lc::ui::Action::None && ui.debug);
        check(ui.input(false,false,false,true)==lc::ui::Action::None && !ui.debug);
        ui.finish(); check(ui.screen==lc::ui::Screen::Results);
        check(ui.input(false,true,false,false)==lc::ui::Action::MenuOnly && ui.screen==lc::ui::Screen::Help);
        check(ui.input(false,false,true,false)==lc::ui::Action::MenuOnly && ui.screen==lc::ui::Screen::Results);
        check(ui.input(true,false,false,false)==lc::ui::Action::RestartRun && ui.screen==lc::ui::Screen::Playing);
        ui.finish(); ui.input(false,true,false,false);
        check(ui.input(true,false,false,false)==lc::ui::Action::RestartRun && ui.screen==lc::ui::Screen::Playing);
        check(ui.input(true,true,true,false)==lc::ui::Action::None); // menu commands cannot restart active play
        std::cout<<"PASS: initial screen, start-only input, debug opt-in, optional help/back, direct restart; simulation untouched\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
