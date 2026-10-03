#pragma once
// Song library for the chiptune sequencer in audio.cpp.
//
//   Korobeiniki   Russian folk song (public domain), the classic A-type theme
//   Minuet in G   Christian Petzold, from Bach's Notebook for Anna Magdalena (public domain)
//   Everything else is an original composition.
//
// A song is a list of sections. Each section has a melody and a chord chart plus
// the patterns the accompaniment channels play over those chords.
//   Melody: "NOTE:LENGTH" tokens with LENGTH in 16th notes, "R" is a rest and "|"
//           only marks bars for readability. "" means no lead in that section.
//   Chords: one token per bar, e.g. "Am", "F#m", "Cmaj7", "G/B", "Dsus4". "F,G"
//           splits a bar evenly between two chords and "x" is a silent bar.
// A melody must be exactly as long as its chord chart (the --autotest report
// lists any section that is not).

#include <cstddef>

namespace music {

enum class Harm  { None, Arp8, Arp16, Up16, Pluck, Stab, Pad };
enum class Bass  { None, Octave8, RootFifth, Quarters, Drive, Pump, Pick };
enum class Drums { None, Light, Rock, Half, Four, Build, Folk, Waltz };

struct Section {
    const char* melody;
    const char* chords;
    Harm harm;
    Bass bass;
    Drums drums;
    int shift = 0;  // transposes the melody (semitones)
};

struct Voice {
    float leadDuty = 0.25f, leadVol = 0.16f;
    float harmDuty = 0.125f, harmVol = 0.05f;
    float bassVol = 0.3f;
    bool synthBass = false;  // filtered saw instead of the NES-style triangle
    float echo = 0.0f;       // dotted-eighth echo on lead and harmony (16-bit flavour)
};

struct SongDef {
    const char* title;
    const char* credit;
    float bpm;
    int barTicks;  // 16 = 4/4, 12 = 3/4
    Voice voice;
    const Section* sections;
    int sectionCount;
    bool secret = false;  // hidden from the song picker until the game has been beaten
};

// ---------------------------------------------------------------------------
//  KOROBEINIKI (A-TYPE)
// ---------------------------------------------------------------------------
inline constexpr const char* kKoroA =
    "E5:4 B4:2 C5:2 D5:4 C5:2 B4:2 | A4:4 A4:2 C5:2 E5:4 D5:2 C5:2 |"
    "B4:6 C5:2 D5:4 E5:4            | C5:4 A4:4 A4:4 R:4             |"
    "R:2 D5:4 F5:2 A5:4 G5:2 F5:2  | E5:6 C5:2 E5:4 D5:2 C5:2       |"
    "B4:4 B4:2 C5:2 D5:4 E5:4      | C5:4 A4:4 A4:4 R:4             |";
inline constexpr const char* kKoroChordsA = "E Am E Am Dm C E Am";

inline constexpr const char* kKoroB =
    "E5:8 C5:8 | D5:8 B4:8 | C5:8 A4:8      | G#4:8 B4:8 |"
    "E5:8 C5:8 | D5:8 B4:8 | C5:4 E5:4 A5:8 | G#5:16     |";
inline constexpr const char* kKoroChordsB = "Am E Am E Am E Am E";

inline constexpr Section kKorobeiniki[] = {
    { kKoroA, kKoroChordsA, Harm::Arp8,  Bass::Octave8, Drums::Rock },
    { kKoroA, kKoroChordsA, Harm::Arp16, Bass::Octave8, Drums::Rock },
    { kKoroB, kKoroChordsB, Harm::Arp8,  Bass::Octave8, Drums::Half },
    { kKoroB, kKoroChordsB, Harm::Arp16, Bass::Octave8, Drums::Half },
};

// ---------------------------------------------------------------------------
//  MINUET IN G (C-TYPE), in 3/4
// ---------------------------------------------------------------------------
inline constexpr const char* kMinuetA =
    "D5:4 G4:2 A4:2 B4:2 C5:2 | D5:4 G4:4 G4:4 | E5:4 C5:2 D5:2 E5:2 F#5:2 | G5:4 G4:4 G4:4 |"
    "C5:4 D5:2 C5:2 B4:2 A4:2 | B4:4 C5:2 B4:2 A4:2 G4:2 | F#4:4 G4:2 A4:2 B4:2 G4:2 | A4:12 |"
    "D5:4 G4:2 A4:2 B4:2 C5:2 | D5:4 G4:4 G4:4 | E5:4 C5:2 D5:2 E5:2 F#5:2 | G5:4 G4:4 G4:4 |"
    "C5:4 D5:2 C5:2 B4:2 A4:2 | B4:4 C5:2 B4:2 A4:2 G4:2 | A4:4 B4:2 A4:2 G4:2 F#4:2 | G4:12 |";
inline constexpr const char* kMinuetChordsA = "G G C G C G D D G G C G Am G D G";

inline constexpr const char* kMinuetB =
    "B5:4 G5:2 A5:2 B5:2 G5:2 | A5:4 D5:2 E5:2 F#5:2 D5:2 | G5:4 E5:2 F#5:2 G5:2 D5:2 | C#5:4 B4:2 C#5:2 A4:4 |"
    "A4:2 B4:2 C#5:2 D5:2 E5:2 F#5:2 | G5:4 F#5:4 E5:4 | F#5:4 A4:4 C#5:4 | D5:12 |"
    "D5:4 G4:2 F#4:2 G4:4 | E5:4 G4:2 F#4:2 G4:4 | D5:4 C5:4 B4:4 | A4:2 G4:2 F#4:2 G4:2 A4:4 |"
    "D4:2 E4:2 F#4:2 G4:2 A4:2 B4:2 | C5:4 B4:4 A4:4 | B4:2 D5:2 G4:4 F#4:4 | G4:12 |";
inline constexpr const char* kMinuetChordsB = "G D Em A A Em A D G C G D D Am G,D G";

inline constexpr Section kMinuet[] = {
    { kMinuetA, kMinuetChordsA, Harm::Arp8,  Bass::Quarters, Drums::Waltz },
    { kMinuetA, kMinuetChordsA, Harm::Arp16, Bass::Quarters, Drums::Waltz },
    { kMinuetB, kMinuetChordsB, Harm::Arp8,  Bass::Quarters, Drums::Waltz },
    { kMinuetB, kMinuetChordsB, Harm::Arp16, Bass::Quarters, Drums::Waltz },
};

// ---------------------------------------------------------------------------
//  AFTERGLOW DRIVE: original 16-bit synth-pop / EDM medley
// ---------------------------------------------------------------------------
inline constexpr const char* kDriveChords = "Bm G D A Bm G D A";

inline constexpr const char* kDriveVerse =
    "R:2 F#4:2 F#4:2 A4:2 B4:4 A4:2 F#4:2 | D5:2 B4:2 A4:2 B4:2 G4:8 |"
    "R:2 F#4:2 F#4:2 A4:2 D5:4 C#5:2 A4:2 | C#5:4 B4:2 A4:2 E4:8 |"
    "R:2 B4:2 B4:2 C#5:2 D5:4 C#5:2 B4:2  | D5:2 E5:2 F#5:2 E5:2 D5:4 B4:4 |"
    "A4:4 F#4:2 A4:2 D5:4 E5:4            | C#5:12 R:4 |";

inline constexpr const char* kDriveBuild =
    "D5:4 D5:4 E5:4 F#5:4 | E5:4 E5:4 F#5:4 A5:4 | F#5:4 F#5:4 A5:4 B5:4 | C#6:8 R:8 |";
inline constexpr const char* kDriveBuildChords = "G A Bm A";

inline constexpr const char* kDriveDrop =
    "F#5:2 F#5:2 R:2 D5:2 F#5:2 A5:2 R:2 F#5:2 | G5:2 F#5:2 D5:2 B4:2 R:4 D5:4 |"
    "F#5:2 F#5:2 R:2 A5:2 B5:2 A5:2 R:2 F#5:2  | E5:2 C#5:2 A4:2 C#5:2 E5:8 |"
    "F#5:2 F#5:2 R:2 D5:2 F#5:2 A5:2 R:2 F#5:2 | G5:2 A5:2 B5:2 A5:2 G5:4 F#5:4 |"
    "D5:2 F#5:2 A5:2 D6:2 C#6:2 A5:2 F#5:2 A5:2 | C#6:4 B5:2 A5:2 E5:8 |";

inline constexpr const char* kDriveBreak = "B4:8 D5:8 | C#5:8 E5:8 | F#5:16 | R:16 |";
inline constexpr const char* kDriveOutro = "F#5:16 | D5:16 | A4:16 | B4:16 |";

inline constexpr Section kAfterglow[] = {
    { "",          "Bm G D A",        Harm::Pad,   Bass::None,  Drums::None },
    { kDriveVerse, kDriveChords,      Harm::Pad,   Bass::Drive, Drums::Light },
    { kDriveBuild, kDriveBuildChords, Harm::Arp16, Bass::Pump,  Drums::Build },
    { kDriveDrop,  kDriveChords,      Harm::Stab,  Bass::Pump,  Drums::Four },
    { kDriveBreak, "G A Bm Bm",       Harm::Pad,   Bass::None,  Drums::None },
    { kDriveVerse, kDriveChords,      Harm::Pluck, Bass::Drive, Drums::Four },
    { kDriveBuild, kDriveBuildChords, Harm::Arp16, Bass::Pump,  Drums::Build },
    { kDriveDrop,  kDriveChords,      Harm::Arp16, Bass::Pump,  Drums::Four },
    { kDriveOutro, "Bm G D A",        Harm::Pad,   Bass::None,  Drums::None },
};

// ---------------------------------------------------------------------------
//  MAPLE HOLLOW: original 8-bit folk-pop medley
// ---------------------------------------------------------------------------
inline constexpr const char* kMapleVerse =
    "R:2 B4:2 B4:2 B4:2 D5:2 B4:2 A4:2 G4:2 | A4:4 A4:2 F#4:2 D4:8 |"
    "R:2 B4:2 B4:2 B4:2 D5:2 E5:2 D5:2 B4:2 | C5:4 B4:2 A4:2 G4:8 |"
    "R:2 D5:2 D5:2 D5:2 E5:2 D5:2 B4:2 G4:2 | A4:2 B4:2 A4:2 F#4:2 D4:8 |"
    "E4:2 G4:2 A4:2 C5:2 B4:2 A4:2 F#4:2 A4:2 | G4:12 R:4 |";
inline constexpr const char* kMapleVerseChords = "G D Em C G D C,D G";

inline constexpr const char* kMapleChorus =
    "B4:2 E5:2 G5:6 F#5:2 E5:4 | E5:4 D5:2 E5:2 G5:8 |"
    "D5:2 G5:2 B5:6 A5:2 G5:4  | F#5:4 E5:2 D5:2 A4:8 |"
    "B4:2 E5:2 G5:6 A5:2 B5:4  | C6:4 B5:2 A5:2 G5:4 E5:4 |"
    "D5:4 G5:4 F#5:4 A5:4      | G5:12 R:4 |";
inline constexpr const char* kMapleChorusChords = "Em C G D Em C G,D G";

inline constexpr const char* kMapleBridge = "E5:8 D5:8 | F#5:8 A5:8 | G5:16 | R:16 |";
inline constexpr const char* kMapleOutro = "B4:8 D5:8 | A4:16 | G4:8 B4:8 | G4:16 |";

inline constexpr Section kMapleHollow[] = {
    { "",           "G D Em C",         Harm::Pluck, Bass::None,      Drums::None },
    { kMapleVerse,  kMapleVerseChords,  Harm::Pluck, Bass::Pick,      Drums::None },
    { kMapleChorus, kMapleChorusChords, Harm::Arp16, Bass::Octave8,   Drums::Folk },
    { kMapleVerse,  kMapleVerseChords,  Harm::Pluck, Bass::Pick,      Drums::Folk },
    { kMapleChorus, kMapleChorusChords, Harm::Arp16, Bass::Octave8,   Drums::Folk },
    { kMapleBridge, "C D Em Em",        Harm::Pad,   Bass::RootFifth, Drums::None },
    { kMapleChorus, kMapleChorusChords, Harm::Stab,  Bass::Octave8,   Drums::Rock },
    { kMapleOutro,  "G D Em C",         Harm::Pluck, Bass::None,      Drums::None },
};

// ---------------------------------------------------------------------------
//  NEON LOBBY: original menu theme
// ---------------------------------------------------------------------------
inline constexpr const char* kLobbyA =
    "E5:6 D5:2 C5:4 E5:4 | F5:6 E5:2 C5:8      | G4:4 C5:4 E5:4 G5:4 | D5:12 R:4 |"
    "E5:6 D5:2 C5:4 A4:4 | C5:6 D5:2 F5:4 A5:4 | G5:4 E5:4 C5:4 E5:4 | D5:8 B4:8 |";
inline constexpr const char* kLobbyChordsA = "Am F C G Am F C G";

inline constexpr const char* kLobbyB =
    "A4:4 D5:4 F5:4 E5:4 | C5:8 R:4 E5:4       | F5:4 E5:2 D5:2 C5:4 A4:4 | B4:12 R:4 |"
    "A4:4 D5:4 F5:4 A5:4 | G5:6 E5:2 C5:8      | B4:4 D5:4 G#5:4 B5:4     | G#5:12 R:4 |";
inline constexpr const char* kLobbyChordsB = "Dm Am F G Dm Am E E";

inline constexpr Section kNeonLobby[] = {
    { "",      "Am F C G",    Harm::Pad,  Bass::RootFifth, Drums::None },
    { kLobbyA, kLobbyChordsA, Harm::Up16, Bass::RootFifth, Drums::Light },
    { kLobbyA, kLobbyChordsA, Harm::Up16, Bass::Octave8,   Drums::Half },
    { kLobbyB, kLobbyChordsB, Harm::Pad,  Bass::RootFifth, Drums::Light },
    { kLobbyA, kLobbyChordsA, Harm::Stab, Bass::Octave8,   Drums::Half },
};

// ---------------------------------------------------------------------------
//  PIXEL SUNRISE: original, bright 8-bit
// ---------------------------------------------------------------------------
inline constexpr const char* kSunA =
    "E5:2 G5:2 C6:4 B5:2 G5:2 E5:4 | D5:2 G5:2 B5:4 A5:2 G5:2 D5:4 |"
    "C5:2 E5:2 A5:4 G5:2 E5:2 C5:4 | F5:4 A5:4 G5:4 F5:4 |"
    "E5:2 G5:2 C6:4 D6:2 C6:2 G5:4 | B5:4 A5:2 G5:2 D5:8 |"
    "A5:4 F5:4 G5:4 B5:4           | C6:12 R:4 |";
inline constexpr const char* kSunChordsA = "C G Am F C G F,G C";

inline constexpr const char* kSunB =
    "F5:4 E5:2 D5:2 A4:4 D5:4 | B4:4 D5:4 G5:6 F5:2 | E5:4 G5:2 E5:2 B4:4 E5:4 | C5:4 E5:2 G5:2 A5:8 |"
    "F5:2 E5:2 D5:2 E5:2 F5:4 A5:4 | G5:4 F5:2 E5:2 D5:4 B4:4 |"
    "C5:2 E5:2 G5:2 C6:2 B5:2 G5:2 E5:2 D5:2 | C5:12 R:4 |";
inline constexpr const char* kSunChordsB = "Dm G Em Am Dm G C C";

inline constexpr Section kPixelSunrise[] = {
    { "",    "C G Am F",  Harm::Pad,   Bass::RootFifth, Drums::Light },
    { kSunA, kSunChordsA, Harm::Arp16, Bass::Octave8,   Drums::Rock },
    { kSunA, kSunChordsA, Harm::Stab,  Bass::Octave8,   Drums::Rock },
    { kSunB, kSunChordsB, Harm::Arp8,  Bass::Octave8,   Drums::Half },
    { kSunA, kSunChordsA, Harm::Arp16, Bass::Octave8,   Drums::Four },
    { "A5:4 G5:4 B5:4 D6:4 | C6:16 |", "F,G C", Harm::Pad, Bass::RootFifth, Drums::None },
};

// ---------------------------------------------------------------------------
//  CLOSER (8-bit cover - The Chainsmokers)
// ---------------------------------------------------------------------------
inline constexpr const char* kCloserChords = "F G Am G";

inline constexpr const char* kCloserVerse =
    "G4:2 G4:2 G4:2 G4:2 G4:2 D4:2 E4:4 | E4:2 E4:2 D4:2 B3:6 R:4 |"
    "G4:2 G4:2 G4:2 G4:2 G4:2 D4:2 E4:4 | E4:2 E4:2 D4:2 B3:4 A3:2 B3:4 |";

inline constexpr const char* kCloserChorusA =
    "G4:2 A4:2 C5:2 C5:2 C5:2 C5:2 B4:4 | B4:2 G4:2 A4:2 A4:2 A4:2 A4:2 G4:2 E4:2 |"
    "E4:2 G4:2 A4:2 C5:2 C5:2 C5:2 B4:4 | B4:2 G4:2 A4:2 A4:2 A4:2 A4:2 G4:2 E4:2 |";

inline constexpr const char* kCloserChorusB =
    "E4:2 G4:2 A4:2 C5:2 C5:2 C5:2 B4:4 | B4:2 G4:2 A4:2 A4:2 A4:2 A4:2 G4:2 E4:2 |"
    "E4:2 G4:2 A4:2 C5:2 C5:2 C5:2 B4:4 | B4:2 G4:2 A4:2 A4:2 A4:2 A4:4 R:2 |";

inline constexpr const char* kCloserDrop =
    "C5:4 C5:2 D5:2 G5:8 | R:2 G5:2 F5:2 E5:2 C5:8 |"
    "C5:4 C5:2 D5:2 G5:8 | R:2 A5:2 G5:2 E5:2 D5:8 |";

inline constexpr Section kCloser[] = {
    { "", kCloserChords, Harm::Pad, Bass::None, Drums::None },
    { kCloserVerse, kCloserChords, Harm::Pluck, Bass::RootFifth, Drums::Half },
    { kCloserVerse, kCloserChords, Harm::Pluck, Bass::Drive, Drums::Half },
    { kCloserChorusA, kCloserChords, Harm::Arp16, Bass::Drive, Drums::Four },
    { kCloserChorusB, kCloserChords, Harm::Arp16, Bass::Drive, Drums::Four },
    { kCloserDrop, kCloserChords, Harm::Stab, Bass::Pump, Drums::Build },
    { kCloserDrop, kCloserChords, Harm::Stab, Bass::Pump, Drums::Rock },
    { kCloserVerse, kCloserChords, Harm::Pluck, Bass::Drive, Drums::Half },
    { kCloserChorusA, kCloserChords, Harm::Arp16, Bass::Drive, Drums::Four },
    { kCloserChorusB, kCloserChords, Harm::Arp16, Bass::Drive, Drums::Four },
    { kCloserDrop, kCloserChords, Harm::Stab, Bass::Pump, Drums::Build },
    { kCloserDrop, kCloserChords, Harm::Stab, Bass::Pump, Drums::Rock },
    { kCloserChorusB, kCloserChords, Harm::Pad, Bass::RootFifth, Drums::Half },
    { kCloserDrop, kCloserChords, Harm::Arp8, Bass::None, Drums::None },
};

// ---------------------------------------------------------------------------
//  STICK SEASON (8-bit cover - Noah Kahan)
// ---------------------------------------------------------------------------
inline constexpr const char* kStickChords = "A E F#m D";
inline constexpr const char* kStickChords8 = "A E F#m D A E F#m D";

inline constexpr const char* kStickVerse =
    "C#5:2 C#5:2 C#5:2 B4:2 A4:2 B4:2 A4:4 | B4:2 B4:2 B4:2 A4:2 G#4:2 A4:2 G#4:4 |"
    "A4:2 A4:2 A4:2 G#4:2 F#4:2 G#4:2 F#4:4 | F#4:8 R:8 |";

inline constexpr const char* kStickChorusA =
    "A4:2 C#5:2 C#5:2 C#5:2 C#5:2 C#5:2 B4:2 A4:2 | B4:2 A4:2 G#4:2 F#4:6 R:4 |"
    "A4:2 C#5:2 C#5:2 C#5:2 C#5:2 C#5:2 B4:2 A4:2 | B4:2 A4:2 G#4:2 F#4:6 R:4 |"
    "A4:2 C#5:2 C#5:2 C#5:2 C#5:2 C#5:2 B4:2 A4:2 | B4:2 A4:2 G#4:2 F#4:6 R:4 |"
    "A4:2 C#5:2 C#5:2 C#5:2 C#5:2 C#5:2 B4:2 A4:2 | B4:2 A4:2 G#4:2 F#4:6 R:4 |";

inline constexpr const char* kStickChorusB =
    "R:2 F#4:2 A4:2 A4:2 A4:2 A4:2 G#4:2 F#4:2 | G#4:2 A4:2 F#4:8 R:4 |"
    "R:2 F#4:2 A4:2 A4:2 A4:2 A4:2 G#4:2 F#4:2 | G#4:2 A4:2 F#4:8 R:4 |"
    "R:2 F#4:2 A4:2 A4:2 A4:4 R:4              | R:2 G#4:2 F#4:2 E4:2 F#4:8 |"
    "R:2 F#4:2 A4:2 A4:2 A4:4 R:4              | R:2 G#4:2 F#4:2 E4:2 F#4:8 |";

inline constexpr const char* kStickBridgeChords = "D E A F#m";
inline constexpr const char* kStickBridge =
    "F#5:4 E5:4 D5:8 | E5:4 D5:4 C#5:8 | C#5:4 B4:4 A4:8 | F#4:16 |";

inline constexpr Section kStickSeason[] = {
    { "", kStickChords, Harm::Pluck, Bass::None, Drums::None },
    { kStickVerse, kStickChords, Harm::Pluck, Bass::Pick, Drums::Light },
    { kStickVerse, kStickChords, Harm::Arp8, Bass::Pick, Drums::Folk },
    { kStickChorusA, kStickChords8, Harm::Arp16, Bass::Octave8, Drums::Folk },
    { kStickChorusB, kStickChords8, Harm::Stab, Bass::Drive, Drums::Rock },
    { kStickVerse, kStickChords, Harm::Pluck, Bass::Pick, Drums::Half },
    { kStickChorusA, kStickChords8, Harm::Arp16, Bass::Octave8, Drums::Folk },
    { kStickChorusB, kStickChords8, Harm::Stab, Bass::Drive, Drums::Rock },
    { kStickBridge, kStickBridgeChords, Harm::Pad, Bass::RootFifth, Drums::Half },
    { kStickBridge, kStickBridgeChords, Harm::Arp16, Bass::Drive, Drums::Four },
    { kStickChorusA, kStickChords8, Harm::Stab, Bass::Octave8, Drums::Rock },
    { kStickChorusB, kStickChords8, Harm::Arp16, Bass::Drive, Drums::Four },
    { "", kStickChords, Harm::Pad, Bass::None, Drums::None },
};

// ---------------------------------------------------------------------------
//  VICTORY LAP: original credits theme (unlocked by beating Marathon)
// ---------------------------------------------------------------------------
inline constexpr const char* kLapA =
    "G4:2 C5:2 E5:2 G5:6 E5:2 G5:2 | F5:2 E5:2 D5:4 B4:4 G4:4 |"
    "A4:2 C5:2 E5:2 A5:6 G5:2 E5:2 | F5:4 A5:4 C6:8 |"
    "C6:4 B5:2 A5:2 G5:4 E5:4      | D5:2 E5:2 F5:2 G5:2 B5:4 D6:4 |"
    "C6:4 A5:4 B5:4 D6:4           | C6:12 R:4 |";
inline constexpr const char* kLapChordsA = "C G Am F C G F,G C";

inline constexpr const char* kLapB =
    "E5:6 D5:2 C5:4 A4:4 | B4:6 C5:2 D5:4 E5:4 | F5:6 E5:2 D5:4 C5:4 | E5:12 R:4 |"
    "D5:4 F5:4 A5:4 D6:4 | B5:6 A5:2 G5:4 E5:4 | A5:4 G5:4 F5:4 A5:4 | G5:4 B5:4 D6:8 |";
inline constexpr const char* kLapChordsB = "Am Em F C Dm Em F G";

inline constexpr Section kVictoryLap[] = {
    { "",                                   "Ab Bb C C",  Harm::Pad,   Bass::RootFifth, Drums::Build },
    { kLapA,                                kLapChordsA,  Harm::Arp16, Bass::Octave8,   Drums::Rock },
    { kLapB,                                kLapChordsB,  Harm::Pad,   Bass::RootFifth, Drums::Half },
    { kLapA,                                kLapChordsA,  Harm::Stab,  Bass::Pump,      Drums::Four },
    { kLapB,                                kLapChordsB,  Harm::Arp8,  Bass::Octave8,   Drums::Rock },
    { kLapA,                                kLapChordsA,  Harm::Arp16, Bass::Octave8,   Drums::Four },
    { "C5:8 Eb5:8 | D5:8 F5:8 | E5:16 | R:16 |", "Ab Bb C C", Harm::Pad, Bass::RootFifth, Drums::None },
};

// ---------------------------------------------------------------------------
//  The library
// ---------------------------------------------------------------------------
template <std::size_t N>
constexpr int count(const Section (&)[N]) { return int(N); }

inline constexpr SongDef kSongs[] = {
    { "KOROBEINIKI", "A-TYPE  /  Russian folk song", 140.0f, 16,
      { 0.25f, 0.16f, 0.125f, 0.05f, 0.30f, false, 0.0f }, kKorobeiniki, count(kKorobeiniki) },
    { "MINUET IN G", "C-TYPE  /  Petzold, via Bach", 150.0f, 12,
      { 0.50f, 0.11f, 0.125f, 0.045f, 0.28f, false, 0.18f }, kMinuet, count(kMinuet) },
    { "AFTERGLOW DRIVE", "Original 16-bit synth-pop medley", 100.0f, 16,
      { 0.25f, 0.13f, 0.25f, 0.045f, 0.24f, true, 0.35f }, kAfterglow, count(kAfterglow) },
    { "MAPLE HOLLOW", "Original 8-bit folk-pop medley", 118.0f, 16,
      { 0.25f, 0.14f, 0.25f, 0.045f, 0.30f, false, 0.2f }, kMapleHollow, count(kMapleHollow) },
    { "NEON LOBBY", "Original menu theme", 100.0f, 16,
      { 0.50f, 0.10f, 0.125f, 0.045f, 0.26f, false, 0.25f }, kNeonLobby, count(kNeonLobby) },
    { "PIXEL SUNRISE", "Original bright 8-bit", 132.0f, 16,
      { 0.25f, 0.12f, 0.125f, 0.05f, 0.30f, false, 0.12f }, kPixelSunrise, count(kPixelSunrise) },
    
    { "CLOSER", "8-bit Cover / The Chainsmokers", 95.0f, 16,
      { 0.25f, 0.15f, 0.25f, 0.05f, 0.35f, true, 0.30f }, kCloser, count(kCloser) },
      
    { "STICK SEASON", "8-bit Cover / Noah Kahan", 118.0f, 16,
      { 0.125f, 0.14f, 0.125f, 0.04f, 0.28f, false, 0.10f }, kStickSeason, count(kStickSeason) },

    // plays under the end credits; keep it last (see kCreditsSong)
    { "VICTORY LAP", "Original credits theme  /  unlocked!", 128.0f, 16,
      { 0.25f, 0.13f, 0.25f, 0.05f, 0.26f, true, 0.25f }, kVictoryLap, count(kVictoryLap), true },
};
inline constexpr int kSongCount = int(sizeof(kSongs) / sizeof(kSongs[0]));
inline constexpr int kCreditsSong = kSongCount - 1;

// The menus play these in turn: the next one starts whenever the menus are
// entered, and each one hands over to the next when it ends.
inline constexpr int kMenuPlaylist[] = { 4, 5, 3, 2, 6, 7 };
inline constexpr int kMenuPlaylistSize = int(sizeof(kMenuPlaylist) / sizeof(kMenuPlaylist[0]));

} // namespace music