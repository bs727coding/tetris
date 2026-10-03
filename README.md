# Tetris

A fast, neon-styled falling-block puzzle game for Windows, written in C++20 on top of
[raylib](https://www.raylib.com/). Modern guideline rules (hold, ghost piece, 7-bag, SRS wall
kicks, T-spins, combos, back-to-back), four game modes, per-mode leaderboards, bloom, particles,
and synthesized chiptune music and sound effects. The only asset files are Windows' own fonts.

It builds to a single self-contained, native ARM64 `tetris.exe` (about 2.4 MB) that only needs
Windows' own DLLs.

## Build and run

### In VS Code
1. Open this folder in VS Code (restart VS Code once after the toolchain install so it sees the new PATH).
2. **Ctrl+Shift+B** builds the release version (`build/release/tetris.exe`).
3. **F5** builds the debug version and starts it under VS Code's C++ debugger (clang writes PDB debug info for it).
4. *Terminal → Run Task…* also offers **Run**, **Run Tests**, **Clean** and **Clean All**.

### From a terminal
```bash
mingw32-make -j
```
```bash
mingw32-make run
```
```bash
mingw32-make test
```

| Target | What it does |
|---|---|
| `mingw32-make` | Release build → `build/release/tetris.exe` |
| `mingw32-make CONFIG=debug` | Debug build → `build/debug/tetris.exe` + `tetris.pdb` |
| `mingw32-make run` | Build and launch |
| `mingw32-make test` | Build and run the rules-engine tests |
| `mingw32-make clean` | Delete the current configuration's build output |
| `mingw32-make distclean` | Delete everything in `build/`, including the compiled raylib |
| `mingw32-make icon` | Regenerate `assets/tetris.ico` from `tools/make_icon.cpp` (only needed when changing the icon) |

The first build also compiles raylib from `third_party/raylib` (about 20 seconds); after that only
changed files rebuild.

**Toolchain:** [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw) (clang 22, native ARM64), installed with
`winget install MartinStorsjo.LLVM-MinGW.UCRT`. It provides `clang++`, `mingw32-make` and `lldb`.

## Controls

| Action | Keys |
|---|---|
| Move | ← → |
| Soft drop | ↓ |
| Hard drop | Space |
| Rotate clockwise | ↑ or X |
| Rotate counter-clockwise | Z or Ctrl |
| Rotate 180° | A |
| Hold | C or Shift |
| Pause | Esc or P |
| Music on/off | M |
| Next menu song (title screen) | Tab |
| Pick the game music (mode select) | Q / E |
| Sound effects on/off | N |
| Glow (bloom) on/off | B |
| Fullscreen | F11 or Alt+Enter |
| FPS counter | F3 |

Menus also work with the mouse. The game pauses itself when its window loses focus.

## Music

Six chiptune songs, all synthesized live (no audio files):

| Song | Style |
|---|---|
| **Korobeiniki** | The classic A-type theme (Russian folk song) |
| **Minuet in G** | A C-type style waltz (Petzold, from Bach's Anna Magdalena notebook) |
| **Afterglow Drive** | Original 16-bit synth-pop medley: verse, build, drop, breakdown |
| **Maple Hollow** | Original 8-bit folk-pop medley: verses, choruses, bridge |
| **Neon Lobby** | Original menu theme |
| **Pixel Sunrise** | Original, bright and fast |

The menus play a rotating playlist (Neon Lobby, Pixel Sunrise, Maple Hollow, Afterglow Drive). A new
track starts each time you return to the title screen or reach the results screen, and each track
hands over to the next when it ends. Tab on the title screen skips ahead.

The mode select screen has a **Game music** picker (Q / E, or click or scroll it) that previews each
song. Your choice, or **Shuffle**, plays during the game and is remembered.

New songs go in `src/music.hpp` as note strings and chord charts. `tetris.exe --autotest <dir>`
checks that every melody fits its chords and lists the songs in its report.

## Modes

| Mode | Goal | Ranked by |
|---|---|---|
| **Marathon** | Clear 200 lines to beat the game. The level rises every 10 lines; pick a start level from 1 to 15 | Score |
| **Sprint** | Clear 40 lines | Time (completed runs only) |
| **Ultra** | Two minutes | Score |
| **Zen** | Slow, steady gravity and no game over. Topping out just clears the board. End the session from the pause menu | Score |

## Beating the game

Clearing **200 lines in Marathon** beats the game. Speed tops out at level 20, so a run from level 1
finishes with ten lines at full speed. A higher start level reaches full speed sooner and spends
longer there, so it's the harder way to win. Winning ends the run with fireworks and rolls the
**end credits**: your run's stats, the song list and thanks. The credits hand over to the usual
results and name entry. Hold Space to fast-forward them or press Esc to skip.

Your first win also unlocks the credits theme, **Victory Lap**, in the Game Music picker and adds
"CHAMPION" to the title screen. Set `kMarathonLines` in `src/config.hpp` to 0 for an endless Marathon.

## Secrets

There are 7 easter eggs. The end credits show how many you've found.

<details>
<summary>Spoilers</summary>

| Secret | How |
|---|---|
| Party mode | Konami code on the title screen: ↑ ↑ ↓ ↓ ← → ← → B A. Rainbow colours, and every line clear throws confetti. Enter it again to turn it off |
| Hello | Type `CLAUDE` on the title screen |
| The tune | Type `TETRIS` on the title screen, or click the logo letters T-E-T-R-I-S in order |
| Secret credits | Type `CREDITS` on the title screen |
| Birthday | Launch the game on June 6, the day Tetris was first released in 1984 |
| Still there? | Leave the game paused for 45 seconds |
| Champion | Beat Marathon |

</details>

## Scoring (× current level)

| Clear | Points | | Clear | Points |
|---|---|---|---|---|
| Single | 100 | | T-spin (no lines) | 400 |
| Double | 300 | | T-spin single | 800 |
| Triple | 500 | | T-spin double | 1200 |
| Tetris | 800 | | T-spin triple | 1600 |
| T-spin mini | 100 | | T-spin mini single | 200 |

* **Back-to-back:** consecutive tetrises and T-spin clears earn ×1.5.
* **Combo:** each consecutive clearing piece adds 50 × combo count.
* **Perfect clear:** emptying the board adds 800 / 1200 / 1800 / 2000 (3200 for a back-to-back tetris).
* **Drops:** soft drop scores 1 per row, hard drop 2 per row.

Leaderboards (top 10 per mode) and preferences are saved in `%APPDATA%\Tetris\`.

## Tuning

Every tunable lives in [`src/config.hpp`](src/config.hpp). Edit it and rebuild. It covers:
* **Handling:** DAS 133 ms and ARR 33 ms by default, plus soft-drop speed, lock delay and move-reset limit.
* **Mode rules:** Sprint length, Ultra time and the level curve.
* **Presentation:** glow strength, screen shake, smooth falling, volumes, music tempo and the piece colours.

## How it is put together

```
src/
  main.cpp       window, main loop, hotkeys, --autotest
  config.hpp     tunables
  tetromino.*    piece shapes, SRS rotation + kick tables
  game.*         the rules engine (no graphics): gravity, lock delay, T-spins, scoring, modes
  input.*        keyboard -> actions with DAS/ARR
  ai.*           placement bot for the title-screen demo and automated testing
  render.*       virtual 1280x720 canvas, font cache, block tiles, bloom, backdrop
  fx.*           particles, popups, shake, flashes, row-collapse springs
  audio.*        sound-effect synthesizer + 4-channel chiptune sequencer
  music.hpp      song library: melodies, chord charts, arrangement patterns, menu playlist
  play.cpp       gameplay screen
  screens.cpp    title, mode select, results, leaderboards, controls
  scores.*       leaderboards and preferences on disk
tests/test_rules.cpp   headless engine tests (SRS kicks, T-spins, scoring, lock delay, modes)
assets/tetris.ico      application icon (16-256 px), embedded into the exe via assets/tetris.rc
tools/make_icon.cpp    draws the icon procedurally and writes the .ico
third_party/raylib/    raylib 6.0 source (zlib license)
```

The icon resource is named `GLFW_ICON`, so GLFW also uses it for the game window's title bar and taskbar button.

**Rendering:**
* Everything is laid out on a 1280×720 virtual canvas and scaled to the window. Fonts are rasterized at the real on-screen size, so text stays sharp at any window size.
* Block tiles are shaded per pixel at startup (signed-distance rounded squares with bevel, gloss and rim), then mipmapped.
* Bloom renders the glowing elements into a half-resolution buffer and blurs it at two scales.
* The soft backdrop is drawn at quarter resolution and upscaled.

On a Snapdragon X Elite this runs at a locked 60 fps using well under 1 ms of CPU per frame.

## Developer options

* `tetris.exe --autotest <folder>`: plays through every screen with the AI, saves screenshots and writes
  `report.txt` (frame timings, audio levels) into the folder. It uses its own throwaway scores file.
* `tetris.exe --autotest <folder> --gputime`: includes GPU time in the frame timings (slower; for profiling).

## Troubleshooting

* **`mingw32-make` not found:** restart VS Code or the terminal after installing LLVM-MinGW. The VS Code
  tasks add the toolchain to PATH themselves.
* **No window / OpenGL errors:** raylib can use other backends. Try
  `mingw32-make distclean` then `mingw32-make RAYLIB_PLATFORM=PLATFORM_DESKTOP_WIN32`.
* **The game stops while minimized:** this is intentional. raylib sleeps until the window is restored, which saves battery.

## Credits

* [raylib](https://github.com/raysan5/raylib) by Ramon Santamaria and contributors (zlib license).
* *Korobeiniki* (a 19th-century Russian folk song) and the *Minuet in G* (Christian Petzold, c. 1725) are in
  the public domain. The songs marked "Original" were written for this game.
* Tetris is a trademark of The Tetris Company. This is a personal, non-commercial fan project.
