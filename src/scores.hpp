#pragma once
// Per-mode top-10 leaderboards and small preferences, saved as text files in
// %APPDATA%\Tetris (or next to the exe when APPDATA is unavailable).
#include "game.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

inline constexpr int kLeaderboardSize = 10;

struct ScoreEntry {
    std::string name;
    std::int64_t score = 0;
    int lines = 0;
    int level = 1;
    double time = 0;  // seconds
    std::string date; // YYYY-MM-DD
};

struct Prefs {
    bool music = true;
    bool sfx = true;
    bool bloom = true;
    bool fullscreen = false;
    std::string lastName = "PLAYER";
    int lastMode = 0;
    int startLevel = 1;
    int song = 0;         // gameplay music: a song index, or -1 for shuffle
    bool beaten = false;  // Marathon has been beaten at least once
    int eggs = 0;         // easter eggs found (bits, see app.hpp)
};

class ScoreBook {
public:
    void load(const std::filesystem::path& dir);
    void save() const;

    const std::vector<ScoreEntry>& entries(GameMode mode) const { return lists_[std::size_t(mode)]; }
    int rankFor(GameMode mode, const ScoreEntry& e) const;  // 0-based rank, or -1 if it does not place
    int insert(GameMode mode, ScoreEntry e);                // returns rank or -1
    const ScoreEntry* best(GameMode mode) const;

    Prefs prefs;
    void savePrefs() const;

private:
    static bool better(GameMode mode, const ScoreEntry& a, const ScoreEntry& b);
    std::filesystem::path dir_;
    std::array<std::vector<ScoreEntry>, kGameModes> lists_;
};

std::filesystem::path defaultDataDir();
std::string todayString();
