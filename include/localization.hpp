#pragma once
#include <array>
#include <string_view>

namespace lc::ui {
enum class Language { Chinese, English, Japanese };
struct Translation { const char* key; std::array<const char*, 3> values; };
inline constexpr Translation translations[] = {
#define LC_TEXT(key, zh, en, ja) {key, {zh, en, ja}},
#include "localization_data.inc"
#undef LC_TEXT
};
inline const char* getText(Language language, std::string_view key) {
    for (const auto& entry : translations)
        if (key == entry.key) return entry.values[static_cast<unsigned>(language)];
    return "[MISSING TEXT]";
}
inline const char* languageTag(Language language) {
    return std::array{"zh-CN", "en", "ja"}[static_cast<unsigned>(language)];
}
}
