#pragma once
// Synthesized sound effects and chiptune music; no audio files are needed.
#include "raylib.h"

#include <array>
#include <string>
#include <vector>

enum class Sfx : int {
    Move, Rotate, Hold, SoftDrop, HardDrop, Lock,
    Clear1, Clear2, Clear3, Tetris, TSpin, TSpinClear, Combo, BackToBack, AllClear,
    LevelUp, Countdown, Go, GameOver, Finish,
    MenuMove, MenuSelect, MenuBack, NewRecord, Pause,
    Count
};

enum class Song : int { None = 0, Title = 1, Game = 2 };

class Audio {
public:
    bool init();
    void shutdown();

    void play(Sfx sfx, float pitch = 1.0f, float volume = 1.0f, float pan = 0.0f);

    void setSong(Song song);
    void setTempo(float scale);     // 1 = the song's normal tempo
    void setMusicDuck(float gain);  // e.g. quieter while paused
    void setMusicEnabled(bool on);
    void setSfxEnabled(bool on) { sfxOn_ = on; }
    bool musicEnabled() const { return musicOn_; }
    bool sfxEnabled() const { return sfxOn_; }

    std::string levelReport() const;  // peak/RMS of every effect + music peak (used by --autotest)

private:
    void updateMusicGain();

    bool ready_ = false;
    bool musicOn_ = true;
    bool sfxOn_ = true;
    float duck_ = 1.0f;
    AudioStream stream_{};
    std::array<std::vector<Sound>, int(Sfx::Count)> voices_{};  // [0] owns the data, the rest are aliases
    std::array<int, int(Sfx::Count)> nextVoice_{};
    std::array<float, int(Sfx::Count)> peak_{}, rms_{};
};
