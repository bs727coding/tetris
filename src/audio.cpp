#include "audio.hpp"

#include "config.hpp"
#include "music.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <sstream>
#include <string>

namespace {

constexpr int kRate = 48000;
constexpr float kTwoPi = 6.28318531f;

float midiHz(float note) { return 440.0f * std::exp2((note - 69.0f) / 12.0f); }

// PolyBLEP smooths the discontinuities of square/saw waves so they do not alias harshly
float polyBlep(float t, float dt)
{
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

float pulseWave(float phase, float dt, float duty)
{
    float v = phase < duty ? 1.0f : -1.0f;
    v += polyBlep(phase, dt);
    float fall = phase - duty;
    if (fall < 0.0f)
        fall += 1.0f;
    return v - polyBlep(fall, dt);
}

float sawWave(float phase, float dt) { return 2.0f * phase - 1.0f - polyBlep(phase, dt); }
float triWave(float phase) { return 4.0f * std::fabs(phase - 0.5f) - 1.0f; }

struct NoiseGen {
    std::uint32_t s = 0x2545F491u;
    float next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return float(s >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }
};

// ---------------------------------------------------------------------------
//  Sound effects: rendered once at startup into raylib Sounds
// ---------------------------------------------------------------------------
enum class Osc { Sine, Square, Pulse25, Pulse12, Triangle, Saw };

float g_lastPeak = 0.0f, g_lastRms = 0.0f;  // levels of the most recently built effect

class SfxBuilder {
public:
    explicit SfxBuilder(float seconds) : buf_(std::size_t(seconds * kRate) + 1, 0.0f) {}

    // A tone gliding exponentially from f0 to f1, with attack and exponential decay
    SfxBuilder& tone(float start, float dur, float f0, float f1, Osc osc, float vol, float decay = 0.0f,
                     float vibrato = 0.0f)
    {
        const int first = int(start * kRate);
        const int count = int(dur * kRate);
        float phase = 0.0f;
        for (int i = 0; i < count && first + i < int(buf_.size()); ++i) {
            const float t = float(i) / kRate;
            float f = f0 * std::pow(f1 / f0, t / dur);
            if (vibrato > 0.0f)
                f *= 1.0f + vibrato * std::sin(kTwoPi * 6.0f * t);
            const float dt = f / kRate;
            float v = 0.0f;
            switch (osc) {
                case Osc::Sine:     v = std::sin(kTwoPi * phase); break;
                case Osc::Square:   v = pulseWave(phase, dt, 0.5f); break;
                case Osc::Pulse25:  v = pulseWave(phase, dt, 0.25f); break;
                case Osc::Pulse12:  v = pulseWave(phase, dt, 0.125f); break;
                case Osc::Triangle: v = triWave(phase); break;
                case Osc::Saw:      v = sawWave(phase, dt); break;
            }
            phase += dt;
            if (phase >= 1.0f)
                phase -= 1.0f;
            float env = std::min(1.0f, t / 0.002f) * std::min(1.0f, (dur - t) / 0.005f);
            if (decay > 0.0f)
                env *= std::exp(-t / decay);
            buf_[std::size_t(first + i)] += vol * env * v;
        }
        return *this;
    }

    SfxBuilder& note(float start, float dur, float midi, Osc osc, float vol, float decay = 0.0f, float vibrato = 0.0f)
    {
        const float f = midiHz(midi);
        return tone(start, dur, f, f, osc, vol, decay, vibrato);
    }

    SfxBuilder& arp(std::initializer_list<float> notes, float start, float step, float dur, Osc osc, float vol,
                    float decay)
    {
        for (float n : notes) {
            note(start, dur, n, osc, vol, decay);
            start += step;
        }
        return *this;
    }

    // Low-passed noise whose cutoff sweeps from c0 to c1 Hz
    SfxBuilder& noise(float start, float dur, float vol, float decay, float c0, float c1)
    {
        NoiseGen rng;
        float lp = 0.0f;
        const int first = int(start * kRate);
        const int count = int(dur * kRate);
        for (int i = 0; i < count && first + i < int(buf_.size()); ++i) {
            const float t = float(i) / kRate;
            const float fc = c0 * std::pow(c1 / c0, t / dur);
            lp += (1.0f - std::exp(-kTwoPi * fc / kRate)) * (rng.next() - lp);
            float env = std::min(1.0f, t / 0.002f) * std::min(1.0f, (dur - t) / 0.005f);
            if (decay > 0.0f)
                env *= std::exp(-t / decay);
            buf_[std::size_t(first + i)] += vol * env * lp;
        }
        return *this;
    }

    Sound build() const
    {
        std::vector<float> data(buf_.size());
        double sumSq = 0.0;
        float maxAbs = 0.0f;
        for (std::size_t i = 0; i < buf_.size(); ++i) {
            data[i] = std::tanh(buf_[i]);  // gentle limiter
            maxAbs = std::max(maxAbs, std::fabs(data[i]));
            sumSq += double(data[i]) * data[i];
        }
        g_lastPeak = maxAbs;
        g_lastRms = data.empty() ? 0.0f : float(std::sqrt(sumSq / double(data.size())));
        Wave w{};
        w.frameCount = unsigned(data.size());
        w.sampleRate = kRate;
        w.sampleSize = 32;
        w.channels = 1;
        w.data = data.data();
        return LoadSoundFromWave(w);  // copies the samples
    }

private:
    std::vector<float> buf_;
};

Sound makeSfx(Sfx id)
{
    switch (id) {
        case Sfx::Move:
            return SfxBuilder(0.05f).tone(0, 0.035f, 1100, 950, Osc::Pulse25, 0.13f, 0.012f).build();
        case Sfx::Rotate:
            return SfxBuilder(0.08f)
                .tone(0, 0.06f, 620, 980, Osc::Pulse25, 0.12f, 0.03f)
                .tone(0, 0.06f, 1240, 1960, Osc::Sine, 0.05f, 0.02f)
                .build();
        case Sfx::Hold:
            return SfxBuilder(0.2f)
                .tone(0, 0.16f, 330, 880, Osc::Triangle, 0.35f, 0.07f)
                .noise(0, 0.14f, 0.25f, 0.05f, 800, 6000)
                .build();
        case Sfx::SoftDrop:
            return SfxBuilder(0.03f).tone(0, 0.022f, 260, 220, Osc::Triangle, 0.14f, 0.008f).build();
        case Sfx::HardDrop:
            return SfxBuilder(0.3f)
                .tone(0, 0.24f, 210, 42, Osc::Sine, 0.9f, 0.1f)
                .tone(0, 0.05f, 420, 140, Osc::Square, 0.12f, 0.02f)
                .noise(0, 0.14f, 0.6f, 0.045f, 5000, 400)
                .build();
        case Sfx::Lock:
            return SfxBuilder(0.09f)
                .tone(0, 0.06f, 260, 170, Osc::Square, 0.11f, 0.02f)
                .noise(0, 0.04f, 0.25f, 0.012f, 6000, 2000)
                .build();
        case Sfx::Clear1:
            return SfxBuilder(0.5f)
                .arp({ 76, 79, 84 }, 0, 0.05f, 0.26f, Osc::Pulse25, 0.12f, 0.1f)
                .noise(0, 0.4f, 0.12f, 0.15f, 3000, 12000)
                .build();
        case Sfx::Clear2:
            return SfxBuilder(0.6f)
                .arp({ 76, 79, 84, 88 }, 0, 0.05f, 0.28f, Osc::Pulse25, 0.12f, 0.12f)
                .noise(0, 0.45f, 0.13f, 0.17f, 3000, 12000)
                .build();
        case Sfx::Clear3:
            return SfxBuilder(0.7f)
                .arp({ 76, 79, 84, 88, 91 }, 0, 0.045f, 0.3f, Osc::Pulse25, 0.12f, 0.14f)
                .noise(0, 0.5f, 0.14f, 0.2f, 3000, 13000)
                .build();
        case Sfx::Tetris: {
            SfxBuilder b(1.3f);
            b.tone(0, 0.3f, 220, 1760, Osc::Sine, 0.08f);  // riser
            b.arp({ 72, 76, 79, 84, 88, 91 }, 0, 0.04f, 0.2f, Osc::Pulse25, 0.1f, 0.08f);
            for (float n : { 72.0f, 76.0f, 79.0f, 84.0f })
                b.note(0.24f, 0.95f, n, Osc::Saw, 0.07f, 0.45f, 0.006f);
            b.note(0.24f, 0.95f, 48, Osc::Triangle, 0.3f, 0.4f);
            b.noise(0.2f, 1.0f, 0.16f, 0.35f, 4000, 14000);
            return b.build();
        }
        case Sfx::TSpin:
            return SfxBuilder(0.35f)
                .tone(0, 0.3f, 380, 950, Osc::Triangle, 0.3f, 0.15f, 0.02f)
                .tone(0.05f, 0.25f, 760, 1900, Osc::Pulse12, 0.05f, 0.1f)
                .build();
        case Sfx::TSpinClear: {
            SfxBuilder b(0.95f);
            b.arp({ 69, 73, 76, 81, 85 }, 0, 0.035f, 0.18f, Osc::Pulse25, 0.1f, 0.07f);
            for (float n : { 69.0f, 73.0f, 76.0f, 81.0f })
                b.note(0.18f, 0.65f, n, Osc::Saw, 0.06f, 0.3f, 0.008f);
            b.noise(0.15f, 0.65f, 0.14f, 0.25f, 5000, 14000);
            return b.build();
        }
        case Sfx::Combo:
            return SfxBuilder(0.3f)
                .note(0, 0.25f, 81, Osc::Sine, 0.32f, 0.09f)
                .note(0, 0.25f, 93, Osc::Triangle, 0.08f, 0.06f)
                .build();
        case Sfx::BackToBack:
            return SfxBuilder(0.5f).arp({ 96, 91, 100, 96 }, 0, 0.05f, 0.18f, Osc::Pulse12, 0.07f, 0.07f).build();
        case Sfx::AllClear: {
            SfxBuilder b(1.7f);
            b.arp({ 72, 76, 79, 84, 88, 91, 96 }, 0, 0.06f, 0.3f, Osc::Pulse25, 0.1f, 0.12f);
            for (float n : { 72.0f, 76.0f, 79.0f, 84.0f, 88.0f })
                b.note(0.45f, 1.15f, n, Osc::Saw, 0.05f, 0.5f, 0.006f);
            b.noise(0.4f, 1.15f, 0.14f, 0.4f, 6000, 16000);
            return b.build();
        }
        case Sfx::LevelUp: {
            SfxBuilder b(0.9f);
            b.arp({ 67, 72, 76 }, 0, 0.07f, 0.12f, Osc::Pulse25, 0.12f, 0.05f);
            b.note(0.21f, 0.6f, 79, Osc::Pulse25, 0.12f, 0.25f, 0.006f);
            b.note(0.21f, 0.6f, 84, Osc::Square, 0.06f, 0.25f);
            return b.build();
        }
        case Sfx::Countdown:
            return SfxBuilder(0.18f).note(0, 0.14f, 69, Osc::Square, 0.14f, 0.08f).build();
        case Sfx::Go:
            return SfxBuilder(0.5f)
                .note(0, 0.42f, 81, Osc::Square, 0.14f, 0.2f)
                .note(0, 0.42f, 69, Osc::Pulse25, 0.08f, 0.2f)
                .build();
        case Sfx::GameOver: {
            SfxBuilder b(1.6f);
            float t = 0.0f;
            for (float n : { 72.0f, 67.0f, 64.0f, 60.0f }) {
                b.note(t, 0.3f, n, Osc::Pulse25, 0.12f, 0.18f);
                b.note(t, 0.3f, n - 12.0f, Osc::Triangle, 0.22f, 0.18f);
                t += 0.2f;
            }
            b.tone(0.8f, 0.7f, midiHz(60), midiHz(48), Osc::Triangle, 0.28f, 0.3f);
            return b.build();
        }
        case Sfx::Finish: {
            SfxBuilder b(1.3f);
            b.arp({ 72, 76, 79, 84, 88 }, 0, 0.06f, 0.2f, Osc::Pulse25, 0.12f, 0.1f);
            for (float n : { 72.0f, 79.0f, 84.0f, 88.0f })
                b.note(0.32f, 0.85f, n, Osc::Saw, 0.06f, 0.4f, 0.006f);
            return b.build();
        }
        case Sfx::MenuMove:
            return SfxBuilder(0.04f).tone(0, 0.03f, 1400, 1250, Osc::Triangle, 0.2f, 0.012f).build();
        case Sfx::MenuSelect:
            return SfxBuilder(0.2f)
                .note(0, 0.07f, 76, Osc::Pulse25, 0.12f, 0.04f)
                .note(0.06f, 0.11f, 83, Osc::Pulse25, 0.12f, 0.06f)
                .build();
        case Sfx::MenuBack:
            return SfxBuilder(0.2f)
                .note(0, 0.07f, 76, Osc::Pulse25, 0.1f, 0.04f)
                .note(0.06f, 0.11f, 71, Osc::Pulse25, 0.1f, 0.06f)
                .build();
        case Sfx::NewRecord: {
            SfxBuilder b(1.0f);
            b.arp({ 84, 88, 91, 96, 91, 96, 100 }, 0, 0.06f, 0.2f, Osc::Pulse12, 0.09f, 0.08f);
            b.noise(0.0f, 0.9f, 0.1f, 0.3f, 8000, 16000);
            return b.build();
        }
        case Sfx::Pause:
            return SfxBuilder(0.14f).tone(0, 0.11f, 520, 330, Osc::Triangle, 0.3f, 0.05f).build();
        case Sfx::Count:
            break;
    }
    return Sound{};
}

int voiceCount(Sfx id)
{
    switch (id) {
        case Sfx::Move:
        case Sfx::SoftDrop:
        case Sfx::MenuMove: return 4;
        case Sfx::Rotate:
        case Sfx::Lock:
        case Sfx::HardDrop:
        case Sfx::Combo:    return 3;
        default:            return 2;
    }
}

// ---------------------------------------------------------------------------
//  Music: a tiny 4-channel sequencer rendered on the audio thread
// ---------------------------------------------------------------------------
struct NoteEvent {
    int tick;   // 16th notes from the start of the song
    int len;
    int note;   // MIDI note; for drums 0 kick, 1 snare, 2 closed hat, 3 open hat
    float vel;
};

struct SongData {
    float bpm = 120.0f;
    int length = 0;  // ticks
    float leadDuty = 0.25f, leadVol = 0.16f, harmVol = 0.05f, bassVol = 0.3f;
    std::vector<NoteEvent> ch[4];  // lead, harmony, bass, drums
};

struct Chord {
    const char* name;
    int root;      // bass note
    int tones[3];  // arpeggio notes
};
constexpr Chord kChords[] = {
    { "Am", 45, { 57, 60, 64 } }, { "E", 40, { 56, 59, 64 } }, { "Dm", 38, { 57, 62, 65 } },
    { "C", 36, { 55, 60, 64 } },  { "F", 41, { 57, 60, 65 } }, { "G", 43, { 55, 59, 62 } },
};

const Chord& findChord(const std::string& name)
{
    for (const Chord& c : kChords)
        if (name == c.name)
            return c;
    return kChords[0];
}

// "G#4" -> MIDI note number, or -1 if it is not a note name
int parseNote(const std::string& s)
{
    static constexpr int kPitchClass[7] = { 9, 11, 0, 2, 4, 5, 7 };  // A B C D E F G
    if (s.size() < 2 || s[0] < 'A' || s[0] > 'G')
        return -1;
    int pc = kPitchClass[s[0] - 'A'];
    std::size_t i = 1;
    if (s[i] == '#') { ++pc; ++i; }
    else if (s[i] == 'b') { --pc; ++i; }
    if (i >= s.size() || s[i] < '0' || s[i] > '9')
        return -1;
    return (s[i] - '0' + 1) * 12 + pc;
}

// Appends a melody string; returns its length in ticks
int addMelody(std::vector<NoteEvent>& out, const char* text, int startTick, float vel)
{
    std::string cleaned(text);
    std::replace(cleaned.begin(), cleaned.end(), '|', ' ');  // bar lines are only for readability
    std::istringstream in(cleaned);
    std::string tok;
    int tick = startTick;
    while (in >> tok) {
        const auto colon = tok.find(':');
        if (colon == std::string::npos)
            continue;
        const int len = std::atoi(tok.c_str() + colon + 1);
        const std::string name = tok.substr(0, colon);
        const int note = parseNote(name);
        if (note >= 0)
            out.push_back({ tick, len, note, vel });
        tick += len;
    }
    return tick - startTick;
}

std::vector<std::string> words(const char* text)
{
    std::istringstream in(text);
    std::vector<std::string> out;
    for (std::string w; in >> w;)
        out.push_back(w);
    return out;
}

void sortSong(SongData& s)
{
    for (auto& ch : s.ch)
        std::stable_sort(ch.begin(), ch.end(), [](const NoteEvent& a, const NoteEvent& b) { return a.tick < b.tick; });
}

SongData buildGameSong()
{
    SongData s;
    s.bpm = cfg::kMusicBpm;
    struct Section { const char* melody; const char* chords; bool busy; bool halfTime; };
    const Section sections[] = {
        { music::kKorobeinikiA, music::kKorobeinikiChordsA, false, false },
        { music::kKorobeinikiA, music::kKorobeinikiChordsA, true,  false },
        { music::kKorobeinikiB, music::kKorobeinikiChordsB, false, true  },
        { music::kKorobeinikiB, music::kKorobeinikiChordsB, true,  true  },
    };
    int tick = 0;
    for (const Section& sec : sections) {
        const int len = addMelody(s.ch[0], sec.melody, tick, 1.0f);
        const auto chords = words(sec.chords);
        for (int bar = 0; bar < int(chords.size()); ++bar) {
            const Chord& c = findChord(chords[std::size_t(bar)]);
            const int t0 = tick + bar * 16;
            static constexpr int kArp[4] = { 0, 1, 2, 1 };
            if (sec.busy)
                for (int i = 0; i < 16; ++i)
                    s.ch[1].push_back({ t0 + i, 1, c.tones[kArp[i % 4]] + 12, 0.7f });
            else
                for (int i = 0; i < 8; ++i)
                    s.ch[1].push_back({ t0 + i * 2, 2, c.tones[kArp[i % 4]], 0.9f });
            for (int i = 0; i < 8; ++i)
                s.ch[2].push_back({ t0 + i * 2, 2, c.root + (i % 2 ? 12 : 0), 1.0f });

            const bool fill = bar == int(chords.size()) - 1;
            if (sec.halfTime) {
                s.ch[3].push_back({ t0, 1, 0, 1.0f });
                s.ch[3].push_back({ t0 + 8, 1, 1, 0.9f });
                for (int i = 0; i < 16; i += 4)
                    s.ch[3].push_back({ t0 + i, 1, 2, 0.7f });
                s.ch[3].push_back({ t0 + 14, 1, 3, 0.6f });
            } else {
                s.ch[3].push_back({ t0, 1, 0, 1.0f });
                s.ch[3].push_back({ t0 + 8, 1, 0, 0.9f });
                if (bar % 2 == 1)
                    s.ch[3].push_back({ t0 + 10, 1, 0, 0.7f });
                s.ch[3].push_back({ t0 + 4, 1, 1, 0.9f });
                if (fill) {
                    for (int i = 12; i < 16; ++i)
                        s.ch[3].push_back({ t0 + i, 1, 1, 0.55f + 0.12f * float(i - 12) });
                } else {
                    s.ch[3].push_back({ t0 + 12, 1, 1, 0.9f });
                }
                for (int i = 0; i < (fill ? 12 : 16); i += 2)
                    s.ch[3].push_back({ t0 + i, 1, 2, (i % 4 == 2) ? 0.8f : 0.45f });
            }
        }
        tick += len;
    }
    s.length = tick;
    sortSong(s);
    return s;
}

SongData buildTitleSong()
{
    SongData s;
    s.bpm = 100.0f;
    s.leadDuty = 0.5f;
    s.leadVol = 0.1f;
    s.harmVol = 0.045f;
    s.bassVol = 0.26f;
    const int len = addMelody(s.ch[0], music::kTitleMelody, 0, 1.0f);
    const auto chords = words(music::kTitleChords);
    for (int bar = 0; bar < int(chords.size()); ++bar) {
        const Chord& c = findChord(chords[std::size_t(bar)]);
        const int t0 = bar * 16;
        const int arp[4] = { c.tones[0], c.tones[1], c.tones[2], c.tones[0] + 12 };
        for (int i = 0; i < 16; ++i)
            s.ch[1].push_back({ t0 + i, 1, arp[i % 4] + 12, 0.8f });
        s.ch[2].push_back({ t0, 8, c.root, 1.0f });
        s.ch[2].push_back({ t0 + 8, 8, c.root + 7, 0.9f });
        s.ch[3].push_back({ t0, 1, 0, 0.5f });
        for (int i = 2; i < 16; i += 4)
            s.ch[3].push_back({ t0 + i, 1, 2, 0.35f });
    }
    s.length = len;
    sortSong(s);
    return s;
}

struct ToneVoice {
    bool active = false;
    float phase = 0.0f;
    float freq = 0.0f;
    float vel = 0.0f;
    float env = 0.0f;
    float startEnv = 0.0f;
    float age = 0.0f;
    int gate = 0;  // samples until release
};

struct DrumVoice {
    float t = 10.0f;  // seconds since hit
    float vel = 0.0f;
    float phase = 0.0f;
    float state = 0.0f;
};

class MusicSynth {
public:
    std::atomic<int> song{ 0 };
    std::atomic<float> tempo{ 1.0f };
    std::atomic<float> gain{ 0.0f };
    std::atomic<float> peak{ 0.0f };  // loudest output sample so far (diagnostics)

    SongData songs[3];  // indexed by Song; [0] stays empty

    void render(float* out, unsigned frames)
    {
        const int want = song.load(std::memory_order_relaxed);
        if (want != playing_) {
            playing_ = want;
            tick_ = 0.0;
            for (auto& n : next_) n = 0;
            for (auto& v : tone_) v.gate = 0;
        }
        const SongData& s = songs[std::clamp(playing_, 0, 2)];
        const bool hasSong = s.length > 0;
        const double ticksPerSample = hasSong ? s.bpm * tempo.load(std::memory_order_relaxed) * 4.0 / 60.0 / kRate : 0.0;
        const float samplesPerTick = hasSong ? float(1.0 / ticksPerSample) : 0.0f;
        const float target = gain.load(std::memory_order_relaxed);
        float blockPeak = 0.0f;

        for (unsigned i = 0; i < frames; ++i) {
            level_ += (target - level_) * 0.0008f;
            if (hasSong) {
                for (int c = 0; c < 4; ++c) {
                    const auto& ev = s.ch[c];
                    while (next_[c] < ev.size() && ev[next_[c]].tick <= tick_)
                        trigger(c, ev[next_[c]++], samplesPerTick);
                }
                tick_ += ticksPerSample;
                if (tick_ >= s.length) {
                    tick_ -= s.length;
                    for (auto& n : next_) n = 0;
                }
            }

            float l = 0.0f, r = 0.0f;
            // Lead: pulse wave with a delayed vibrato
            if (float e = envelope(tone_[0], 0.7f, 0.25f, 0.03f); e > 0.0f) {
                ToneVoice& v = tone_[0];
                const float vib = 1.0f + 0.0045f * std::sin(kTwoPi * 5.5f * v.age) * std::clamp((v.age - 0.15f) * 8.0f, 0.0f, 1.0f);
                const float dt = v.freq * vib / kRate;
                const float x = pulseWave(v.phase, dt, s.leadDuty) * e * v.vel * s.leadVol;
                advance(v.phase, dt);
                l += x;
                r += x;
            }
            // Harmony: thin plucky pulse, a little to the right
            if (float e = envelope(tone_[1], 0.4f, 0.08f, 0.02f); e > 0.0f) {
                ToneVoice& v = tone_[1];
                const float dt = v.freq / kRate;
                const float x = pulseWave(v.phase, dt, 0.125f) * e * v.vel * s.harmVol;
                advance(v.phase, dt);
                l += x * 0.8f;
                r += x * 1.2f;
            }
            // Bass: triangle
            if (float e = envelope(tone_[2], 0.85f, 0.3f, 0.02f); e > 0.0f) {
                ToneVoice& v = tone_[2];
                const float dt = v.freq / kRate;
                const float x = triWave(v.phase) * e * v.vel * s.bassVol;
                advance(v.phase, dt);
                l += x;
                r += x;
            }
            // Drums
            constexpr float kStep = 1.0f / kRate;
            if (DrumVoice& d = drum_[0]; d.t < 0.4f) {  // kick: fast pitch drop
                const float f = 48.0f + 120.0f * std::exp(-d.t / 0.028f);
                advance(d.phase, f / kRate);
                const float x = std::sin(kTwoPi * d.phase) * std::exp(-d.t / 0.13f) * d.vel * 0.5f;
                l += x;
                r += x;
                d.t += kStep;
            }
            if (DrumVoice& d = drum_[1]; d.t < 0.35f) {  // snare: noise + body tone
                const float n = noise_.next();
                d.state += 0.45f * (n - d.state);
                const float x = (d.state * std::exp(-d.t / 0.07f) * 0.3f +
                                 std::sin(kTwoPi * 185.0f * d.t) * std::exp(-d.t / 0.035f) * 0.14f) * d.vel;
                l += x;
                r += x;
                d.t += kStep;
            }
            for (int h = 2; h < 4; ++h) {  // closed / open hat: high-passed noise
                DrumVoice& d = drum_[h];
                if (d.t >= 0.3f)
                    continue;
                const float n = noise_.next();
                const float hp = n - d.state;
                d.state = n;
                const float x = hp * std::exp(-d.t / (h == 2 ? 0.016f : 0.08f)) * d.vel * (h == 2 ? 0.07f : 0.05f);
                l += x * 1.2f;
                r += x * 0.8f;
                d.t += kStep;
            }

            out[2 * i] = std::tanh(l * 1.3f) * level_;
            out[2 * i + 1] = std::tanh(r * 1.3f) * level_;
            blockPeak = std::max(blockPeak, std::max(std::fabs(out[2 * i]), std::fabs(out[2 * i + 1])));
        }
        if (blockPeak > peak.load(std::memory_order_relaxed))
            peak.store(blockPeak, std::memory_order_relaxed);
    }

private:
    static void advance(float& phase, float dt)
    {
        phase += dt;
        if (phase >= 1.0f)
            phase -= 1.0f;
    }

    // Attack / decay-to-sustain while the gate is open, exponential release after
    static float envelope(ToneVoice& v, float sustain, float decayTau, float releaseTau)
    {
        if (!v.active)
            return 0.0f;
        const float t = v.age;
        v.age += 1.0f / kRate;
        if (v.gate > 0) {
            --v.gate;
            if (t < 0.004f)
                v.env = v.startEnv + (1.0f - v.startEnv) * (t / 0.004f);
            else
                v.env = sustain + (1.0f - sustain) * std::exp(-(t - 0.004f) / decayTau);
        } else {
            v.env *= std::exp(-1.0f / (releaseTau * kRate));
            if (v.env < 1e-4f)
                v.active = false;
        }
        return v.env;
    }

    void trigger(int ch, const NoteEvent& e, float samplesPerTick)
    {
        if (ch == 3) {
            DrumVoice& d = drum_[e.note & 3];
            d.t = 0.0f;
            d.vel = e.vel;
            d.phase = 0.0f;
            return;
        }
        ToneVoice& v = tone_[ch];
        v.startEnv = v.active ? v.env : 0.0f;
        v.active = true;
        v.freq = midiHz(float(e.note));
        v.vel = e.vel;
        v.age = 0.0f;
        const float articulation = ch == 1 ? 0.6f : 0.9f;
        v.gate = int(float(e.len) * samplesPerTick * articulation);
    }

    int playing_ = -1;
    double tick_ = 0.0;
    std::size_t next_[4]{};
    ToneVoice tone_[3];
    DrumVoice drum_[4];
    float level_ = 0.0f;
    NoiseGen noise_;
};

MusicSynth g_music;

void musicCallback(void* buffer, unsigned int frames)
{
    g_music.render(static_cast<float*>(buffer), frames);
}

} // namespace

bool Audio::init()
{
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (!ready_)
        return false;

    for (int i = 0; i < int(Sfx::Count); ++i) {
        const Sound base = makeSfx(Sfx(i));
        peak_[std::size_t(i)] = g_lastPeak;
        rms_[std::size_t(i)] = g_lastRms;
        voices_[std::size_t(i)].push_back(base);
        for (int v = 1; v < voiceCount(Sfx(i)); ++v)
            voices_[std::size_t(i)].push_back(LoadSoundAlias(base));
    }

    g_music.songs[int(Song::Title)] = buildTitleSong();
    g_music.songs[int(Song::Game)] = buildGameSong();
    stream_ = LoadAudioStream(kRate, 32, 2);
    SetAudioStreamCallback(stream_, musicCallback);
    PlayAudioStream(stream_);
    updateMusicGain();
    return true;
}

void Audio::shutdown()
{
    if (!ready_)
        return;
    StopAudioStream(stream_);
    UnloadAudioStream(stream_);
    for (auto& list : voices_) {
        for (std::size_t v = 1; v < list.size(); ++v)
            UnloadSoundAlias(list[v]);
        if (!list.empty())
            UnloadSound(list[0]);
        list.clear();
    }
    CloseAudioDevice();
    ready_ = false;
}

void Audio::play(Sfx sfx, float pitch, float volume, float pan)
{
    if (!ready_ || !sfxOn_)
        return;
    auto& list = voices_[std::size_t(sfx)];
    if (list.empty())
        return;
    int& next = nextVoice_[std::size_t(sfx)];
    const Sound& s = list[std::size_t(next)];
    next = (next + 1) % int(list.size());
    SetSoundPitch(s, pitch);
    SetSoundVolume(s, std::clamp(volume, 0.0f, 1.5f) * cfg::kSfxVolume);
    SetSoundPan(s, std::clamp(pan, -1.0f, 1.0f));
    PlaySound(s);
}

void Audio::setSong(Song song)
{
    g_music.song.store(int(song), std::memory_order_relaxed);
}

void Audio::setTempo(float scale)
{
    g_music.tempo.store(scale, std::memory_order_relaxed);
}

void Audio::setMusicDuck(float gain)
{
    duck_ = gain;
    updateMusicGain();
}

void Audio::setMusicEnabled(bool on)
{
    musicOn_ = on;
    updateMusicGain();
}

void Audio::updateMusicGain()
{
    g_music.gain.store(musicOn_ ? cfg::kMusicVolume * duck_ : 0.0f, std::memory_order_relaxed);
}

std::string Audio::levelReport() const
{
    static constexpr const char* kNames[int(Sfx::Count)] = {
        "Move", "Rotate", "Hold", "SoftDrop", "HardDrop", "Lock", "Clear1", "Clear2", "Clear3", "Tetris",
        "TSpin", "TSpinClear", "Combo", "BackToBack", "AllClear", "LevelUp", "Countdown", "Go", "GameOver",
        "Finish", "MenuMove", "MenuSelect", "MenuBack", "NewRecord", "Pause",
    };
    std::string out = std::string("audio device: ") + (ready_ ? "ready" : "NOT READY") + "\n";
    char line[96];
    for (int i = 0; i < int(Sfx::Count); ++i) {
        std::snprintf(line, sizeof line, "  sfx %-11s peak %.2f  rms %.3f\n", kNames[i], peak_[std::size_t(i)],
                      rms_[std::size_t(i)]);
        out += line;
    }
    std::snprintf(line, sizeof line, "  music peak output %.3f\n", g_music.peak.load());
    return out + line;
}
