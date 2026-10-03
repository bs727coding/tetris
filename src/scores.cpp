#include "scores.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

std::string sanitizeName(std::string name)
{
    std::string out;
    for (char c : name)
        if (c >= 32 && c < 127 && c != '|')
            out += c;
    if (out.size() > 12)
        out.resize(12);
    return out.empty() ? "PLAYER" : out;
}

} // namespace

fs::path defaultDataDir()
{
#ifdef _WIN32
    if (const wchar_t* appData = _wgetenv(L"APPDATA"); appData && *appData)
        return fs::path(appData) / L"Tetris";
#endif
    std::error_code ec;
    return fs::current_path(ec) / "Tetris";
}

std::string todayString()
{
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[16];
    std::strftime(buf, sizeof buf, "%Y-%m-%d", &tm);
    return buf;
}

bool ScoreBook::better(GameMode mode, const ScoreEntry& a, const ScoreEntry& b)
{
    if (mode == GameMode::Sprint)
        return a.time < b.time;  // fastest 40 lines wins
    return a.score > b.score;
}

void ScoreBook::load(const fs::path& dir)
{
    dir_ = dir;
    for (auto& l : lists_)
        l.clear();

    std::ifstream in(dir_ / "scores.txt");
    std::string line;
    while (std::getline(in, line)) {
        // mode|score|lines|level|time|date|name
        std::stringstream ss(line);
        std::string field[7];
        int n = 0;
        while (n < 7 && std::getline(ss, field[n], '|'))
            ++n;
        if (n != 7)
            continue;
        try {
            const int mode = std::stoi(field[0]);
            if (mode < 0 || mode >= kGameModes)
                continue;
            ScoreEntry e;
            e.score = std::stoll(field[1]);
            e.lines = std::stoi(field[2]);
            e.level = std::stoi(field[3]);
            e.time = std::stod(field[4]);
            e.date = field[5];
            e.name = sanitizeName(field[6]);
            lists_[std::size_t(mode)].push_back(e);
        } catch (...) {
            // ignore malformed lines
        }
    }
    for (int m = 0; m < kGameModes; ++m) {
        auto& l = lists_[std::size_t(m)];
        std::stable_sort(l.begin(), l.end(), [m](const ScoreEntry& a, const ScoreEntry& b) {
            return better(GameMode(m), a, b);
        });
        if (l.size() > kLeaderboardSize)
            l.resize(kLeaderboardSize);
    }

    std::ifstream pin(dir_ / "prefs.txt");
    while (std::getline(pin, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        try {
            if (key == "music")           prefs.music = value == "1";
            else if (key == "sfx")        prefs.sfx = value == "1";
            else if (key == "bloom")      prefs.bloom = value == "1";
            else if (key == "fullscreen") prefs.fullscreen = value == "1";
            else if (key == "name")       prefs.lastName = sanitizeName(value);
            else if (key == "mode")       prefs.lastMode = std::clamp(std::stoi(value), 0, kGameModes - 1);
            else if (key == "startLevel") prefs.startLevel = std::clamp(std::stoi(value), 1, 15);
            else if (key == "song")       prefs.song = std::max(-1, std::stoi(value));
            else if (key == "beaten")     prefs.beaten = value == "1";
            else if (key == "eggs")       prefs.eggs = std::stoi(value);
        } catch (...) {
        }
    }
}

void ScoreBook::save() const
{
    std::error_code ec;
    fs::create_directories(dir_, ec);
    std::ofstream out(dir_ / "scores.txt", std::ios::trunc);
    for (int m = 0; m < kGameModes; ++m)
        for (const ScoreEntry& e : lists_[std::size_t(m)])
            out << m << '|' << e.score << '|' << e.lines << '|' << e.level << '|' << e.time << '|' << e.date
                << '|' << e.name << '\n';
}

void ScoreBook::savePrefs() const
{
    std::error_code ec;
    fs::create_directories(dir_, ec);
    std::ofstream out(dir_ / "prefs.txt", std::ios::trunc);
    out << "music=" << (prefs.music ? 1 : 0) << '\n'
        << "sfx=" << (prefs.sfx ? 1 : 0) << '\n'
        << "bloom=" << (prefs.bloom ? 1 : 0) << '\n'
        << "fullscreen=" << (prefs.fullscreen ? 1 : 0) << '\n'
        << "name=" << prefs.lastName << '\n'
        << "mode=" << prefs.lastMode << '\n'
        << "startLevel=" << prefs.startLevel << '\n'
        << "song=" << prefs.song << '\n'
        << "beaten=" << (prefs.beaten ? 1 : 0) << '\n'
        << "eggs=" << prefs.eggs << '\n';
}

int ScoreBook::rankFor(GameMode mode, const ScoreEntry& e) const
{
    const auto& l = lists_[std::size_t(mode)];
    int rank = 0;
    while (rank < int(l.size()) && !better(mode, e, l[std::size_t(rank)]))
        ++rank;
    return rank < kLeaderboardSize ? rank : -1;
}

int ScoreBook::insert(GameMode mode, ScoreEntry e)
{
    const int rank = rankFor(mode, e);
    if (rank < 0)
        return -1;
    e.name = sanitizeName(e.name);
    auto& l = lists_[std::size_t(mode)];
    l.insert(l.begin() + rank, e);
    if (l.size() > kLeaderboardSize)
        l.resize(kLeaderboardSize);
    save();
    return rank;
}

const ScoreEntry* ScoreBook::best(GameMode mode) const
{
    const auto& l = lists_[std::size_t(mode)];
    return l.empty() ? nullptr : &l.front();
}
