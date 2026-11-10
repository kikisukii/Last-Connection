#pragma once
#include "game.hpp"
#include <vector>
namespace lc {
enum class OpportunityKind { Immediate, Prepared };
struct EncounterCandidate {
    VehicleType type;
    Lane lane;
    double position;
    double transferStart;
    double catchTime;
    double firstVisibleTime;
    bool serviceContinuation;
    OpportunityKind kind = OpportunityKind::Immediate;
    std::optional<std::size_t> prerequisiteId = {};
};
// Lightweight forecasts of an ordinary return-to-center then one transfer.
// They suggest traffic; they never issue input or automatically board a car.
std::vector<EncounterCandidate> planEncounters(const Game& game, std::span<const Vehicle> active);
bool hasUpcomingEncounter(const Game& game, std::span<const Vehicle> active);
std::vector<EncounterCandidate> planPreparedEncounters(const Game& game, std::span<const Vehicle> active);
bool isUpcomingCandidate(const Game& game, const Vehicle& vehicle);
bool isPreparedCandidate(const Game& game, const Vehicle& target, std::span<const Vehicle> active);
}
