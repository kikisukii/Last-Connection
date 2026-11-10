#pragma once
#include "gameplay_config.hpp"
#include <optional>
namespace lc {
struct StoredBest { std::optional<double> time; bool storageAvailable = true; };
StoredBest loadLocalBest(const GameplayConfig& config);
bool saveLocalBest(double seconds, const GameplayConfig& config);
}
