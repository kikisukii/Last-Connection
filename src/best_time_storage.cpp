#include "best_time_storage.hpp"
#include "time_attack.hpp"
#include <array>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
EM_JS(int, readBestRecord, (char* buffer, int capacity), {
    try {
        const value = localStorage.getItem('last-connection-best-v1');
        if (value === null) return 0;
        if (lengthBytesUTF8(value) >= capacity) return 0;
        stringToUTF8(value, buffer, capacity);
        return 1;
    } catch (_) { return -1; }
});
EM_JS(int, writeBestRecord, (const char* record), {
    try { localStorage.setItem('last-connection-best-v1', UTF8ToString(record)); return 1; }
    catch (_) { return 0; }
});
#else
#include <raylib.h>
#include <filesystem>
#include <fstream>
#endif

namespace lc {
StoredBest loadLocalBest(const GameplayConfig& config) {
    std::array<char, 4097> buffer{};
#ifdef __EMSCRIPTEN__
    const int result = readBestRecord(buffer.data(), static_cast<int>(buffer.size()));
    if (result <= 0) return {{}, result == 0};
    return {decodeBestTime(buffer.data(), config), true};
#else
    const auto path = std::filesystem::path(GetApplicationDirectory()) / "last_connection.best";
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) return {{}, false};
    if (!exists) return {};
    std::ifstream file(path, std::ios::binary);
    if (!file) return {{}, false};
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (file.bad()) return {{}, false};
    return {decodeBestTime(std::string_view(buffer.data(), static_cast<std::size_t>(file.gcount())), config), true};
#endif
}
bool saveLocalBest(double seconds, const GameplayConfig& config) {
    const auto record = encodeBestTime(seconds, config);
#ifdef __EMSCRIPTEN__
    return writeBestRecord(record.c_str()) != 0;
#else
    const auto path = std::filesystem::path(GetApplicationDirectory()) / "last_connection.best";
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file << record;
    file.close();
    return !file.fail();
#endif
}
}
