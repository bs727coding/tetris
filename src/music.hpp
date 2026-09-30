#pragma once
// Song data for the chiptune sequencer in audio.cpp.
//   Game theme : an arrangement of "Korobeiniki" (Russian folk song, public domain)
//   Title theme: an original loop
// Melodies are "NOTE:LENGTH" tokens with LENGTH in 16th notes, "R" is a rest and
// "|" only marks bars for readability. Chords are listed one per bar.

namespace music {

inline constexpr const char* kKorobeinikiA =
    "E5:4 B4:2 C5:2 D5:4 C5:2 B4:2 | A4:4 A4:2 C5:2 E5:4 D5:2 C5:2 |"
    "B4:6 C5:2 D5:4 E5:4           | C5:4 A4:4 A4:4 R:4             |"
    "R:2 D5:4 F5:2 A5:4 G5:2 F5:2  | E5:6 C5:2 E5:4 D5:2 C5:2       |"
    "B4:4 B4:2 C5:2 D5:4 E5:4      | C5:4 A4:4 A4:4 R:4             |";
inline constexpr const char* kKorobeinikiChordsA = "E Am E Am Dm C E Am";

inline constexpr const char* kKorobeinikiB =
    "E5:8 C5:8 | D5:8 B4:8 | C5:8 A4:8      | G#4:8 B4:8 |"
    "E5:8 C5:8 | D5:8 B4:8 | C5:4 E5:4 A5:8 | G#5:16     |";
inline constexpr const char* kKorobeinikiChordsB = "Am E Am E Am E Am E";

inline constexpr const char* kTitleMelody =
    "E5:6 D5:2 C5:4 E5:4 | F5:6 E5:2 C5:8      | G4:4 C5:4 E5:4 G5:4 | D5:12 R:4 |"
    "E5:6 D5:2 C5:4 A4:4 | C5:6 D5:2 F5:4 A5:4 | G5:4 E5:4 C5:4 E5:4 | D5:8 B4:8 |";
inline constexpr const char* kTitleChords = "Am F C G Am F C G";

} // namespace music
