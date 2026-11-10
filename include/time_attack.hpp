#pragma once
#include "game.hpp"
#include "live_traffic.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace lc {
void validateRaceConfig(const GameplayConfig& config);
char rankForTime(double seconds, const GameplayConfig& config);
std::string formatRaceTime(double seconds);
// A local record is valid only for the same gameplay parameters/clock policy.
std::string scoreRulesKey(const GameplayConfig& config);
std::string encodeBestTime(double seconds, const GameplayConfig& config);
std::optional<double> decodeBestTime(std::string_view data, const GameplayConfig& config);

class TimeAttack {
public:
    TimeAttack(GameplayConfig config, std::uint32_t seed);
    bool requestTransfer(int direction);
    void update(double seconds);
    void restart(std::uint32_t seed); // caller resets its frame accumulator too
    const Game& game() const { return game_; }
    const LiveTraffic& traffic() const { return traffic_; }
    bool finished() const { return finishTime_.has_value(); }
    bool late() const { return elapsed() > config_.deadline + 1e-9; }
    double elapsed() const { return finishTime_.value_or(game_.player().elapsedSeconds()); }
    double remainingDistance() const;
    double progress() const;
    std::optional<double> finishTime() const { return finishTime_; }
    char rank() const { return finished() ? rankForTime(*finishTime_, config_) : '-'; }
private:
    GameplayConfig config_;
    Game game_;
    LiveTraffic traffic_;
    std::optional<double> finishTime_;
};
}
