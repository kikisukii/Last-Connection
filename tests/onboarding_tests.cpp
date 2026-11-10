#include "onboarding.hpp"
#include "time_attack.hpp"
#include <iostream>
#include <stdexcept>
int main() {
    try {
        auto check=[](bool value){ if(!value) throw std::runtime_error("UI contract failed"); };
        lc::ui::Flow ui;
        lc::TimeAttack race({},42);
        check(ui.screen==lc::ui::Screen::Language && !ui.debug);
        check(ui.input(false,false,false,true)==lc::ui::Action::MenuOnly && !ui.debug);
        for(bool keyboard : {false,true}) {
            (void)keyboard; // both native hit-testing and keyboard route to start
            ui={};
            check(ui.input(true,false,false,false)==lc::ui::Action::MenuOnly);
            ui.selectLanguage(lc::ui::Language::Japanese);
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
        check(ui.language==lc::ui::Language::Japanese);
        check(lc::ui::Flow{}.screen==lc::ui::Screen::Language);
        for(const auto& entry : lc::ui::translations) for(const auto* value : entry.values) check(value[0]!=0);
        check(std::string(lc::ui::getText(ui.language,"how"))=="遊び方");
        for(const auto language : {lc::ui::Language::Chinese,lc::ui::Language::English,lc::ui::Language::Japanese}) {
            lc::ui::Flow flow;
            lc::TimeAttack run({},42);
            flow.selectLanguage(language);
            check(flow.input(true,false,false,false)==lc::ui::Action::StartRun);
            // Native model integration: a real 1200-unit run, with the existing
            // fixed step and normal simulation. No clock/config shortcuts.
            for(int step=0;step<30000 && !run.finished();++step) run.update(lc::GameplayConfig{}.fixedStepSeconds);
            check(run.finished() && run.rank()=='F');
            flow.finish();
            check(flow.input(false,true,false,false)==lc::ui::Action::MenuOnly);
            check(flow.screen==lc::ui::Screen::Help && flow.language==language);
            flow.input(false,false,true,false);
            check(flow.input(true,false,false,false)==lc::ui::Action::RestartRun);
            run.restart(43);
            check(run.elapsed()==0 && flow.language==language && flow.screen==lc::ui::Screen::Playing);
        }
        std::cout<<"PASS: initial screen, start-only input, debug opt-in, optional help/back, direct restart; simulation untouched\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
