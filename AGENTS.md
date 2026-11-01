# 808 BASS MAKER (repo: 808-bass-maker) — guide for AI agents and developers

Read this whole file before changing anything. Current version: **0.12** (`project(... VERSION 0.1.2)` in CMakeLists.txt).

## 1. Product

808 BASS MAKER, subtitle "HEAVY 808 DESIGNER", is a commercial VST3/AU/Standalone mono synth by Andrey Ulyanov
(artist name Thematic, manufacturer "Thematic"), a producer and game sound designer who uses FL Studio on Windows.
Goal: dial in different heavy 808s fast. "Bass that moves your guts" — power first, but the low end stays clean.

The sound is built on the principle of the owner's Serum 808 patch:

```
sine (OCT -2)
 + 3 pitch envelopes: CLICK (very fast, Serum LFO 4), PUNCH (~1/4, exp, LFO 3), DROP (slow linear over the tail, LFO 2)
 + linear volume tail = LENGTH (Serum LFO 1 -> A Vol, ENV 1 is just a gate: A 0.5 ms, R 15 ms)
 + NOISE (8 built-in types) mixed INTO the drive, breathing with the sine crests and the tail (Serum LFO 5 -> Noise):
   the drive crushes it into crunch instead of hiss
 + FM: carrier + modulator at 2x = ODD harmonics only, band-limited to 160-1600 Hz (stylish squeak)
 -> drive whose amount falls with the tail (Serum LFO 1 -> Dist Drive); types ASYM (Serum) TUBE HARD FOLD (GRAIN FORM) RECTIFY
 -> clean sub below 90 Hz (LR4 split, mono sine) + dirt above it
 -> EQ high cut 9 kHz -> DC block -> output clipper (POWER pushes into it)
Mono, last-note priority, LEGATO off (envelopes retrigger), no glide (GLIDE knob removed in 0.11, param kept), pitch bend +/-2.
3 LFOs exactly like GRAIN FORM: 4-point drawable shapes, drag a tab onto any knob, chips = depth, lamps, REMOVE LINKS,
SHAPE presets, MODE FREE/RETRIG/ENV (default RETRIG, restarts on every note), DIVISION + SYNC, RATE, RISE. Mod every 32 samples.
Everything runs at 4x oversampling, decimated by two half-band FIR stages.
```

POWER is a macro: more drive, more punch depth, more sub, harder clipper, and on screen: thicker wave, more air/dust.

## 2. Owner's rules (important)

- The owner does not code. The agent does everything: code, build, check, deliver files. No step-by-step instructions.
- Before creating or generating anything, ask him to confirm. When he sends change requests, FIRST agree on the changes
  in text, THEN implement. For visual changes, render a preview locally (section 5) and show it BEFORE a CI build.
- CI builds only on his command ("собирай", "делай сборку", "начинай собирать").
- Version naming: 0.1, then 0.11, 0.12 ... Deliver files with the version in the name: `808-BASS-MAKER-0.11-Windows.zip`,
  `808-BASS-MAKER-0.11-macOS.zip`. Bump `VERSION` in CMakeLists.txt (0.11 -> 0.1.1) and the "v0.x" text in the editor.
- Always give links (to builds, tools, sources). He writes Russian, often with typos. Reply in Russian.
- Design (since 0.12): GRAIN FORM style. Graphite bg, rounded panels, quiet grey flat knobs (disc + one light pointer,
  NO value ring, no highlights/3D), name above, value in a dark box below; light wide spaced title on the left.
  Accent: copper `0xffd48c5c`. The "air" on the screen is pearl white: the bass BLOWS; DRIVE heats it
  (pearl -> peach -> copper -> fire, `Colours808::heat`), NOISE AMOUNT mixes in sand grains (`Colours808::sand`).
  Attack: one pressure arc + flash (owner chose this over soft ripples). NOISE also draws a grainy SAND WAVE
  around the wave; sand grains never take the heat colour.
  ALL UI text is English (no Russian in the plugin).
- **Never change** `PLUGIN_MANUFACTURER_CODE Thmt`, `PLUGIN_CODE B808`, `BUNDLE_ID com.thematic.808bassmaker`, `PRODUCT_NAME`.
- **Never rename or remove parameter IDs** (`ParamID` in PluginProcessor.h). If a feature goes away, keep the parameter and ignore it.

## 3. Code map

| File | What it is |
|---|---|
| `Source/BassEngine.h` | Header-only DSP: parameter mapping (`pitchShape`, `ampShape`, `driveGain`...), `Engine` (voice, oversampling, drive, sub split, clipper), viz atomics, factory presets |
| `Source/PluginProcessor.*` | Parameters, MIDI, mod matrix + LFOs (ported from GRAIN FORM), factory + user presets (`Documents/808 BASS MAKER/Presets/*.808preset`), RANDOM ranges (`randomTargets`), state (mods + LFO shapes) |
| `Source/PluginEditor.*` | UI 900x650: `FlatLook`, `Knob` (+ `ModChip`, LFO drop target), `WaveScreen` (wave + air + dust from the real audio, noise sparks), `Meter`, `PresetBar` (< NAME > SAVE), `NoisePicker`, `LfoTab`, `LfoEditor`, `BassEditor` |
| `tools/Snapshot.cpp` | Renders the editor to PNG while a note plays (`ui-idle/impact/tail/gut.png`; `frames` mode = animation strip) |
| `test/engine_test.cpp` | `g++ -std=c++17 -O2 -ISource test/engine_test.cpp -o et && ./et` |
| `.github/workflows/build.yml` | CI: Windows + macOS, publishes zips + PNGs to the `builds` branch (manual start only) |
| `tools/build-windows.bat` | Local one-click Windows build (VS 2022 + CMake) |

The screen draws the wave from the same mapping functions as the DSP, on a stretched time axis (t = u^2) with the
pitch softened, so CLICK/PUNCH are visible and the sine stays readable. Dust: a burst on every note (`vizHits`),
60% from the attack point, 40% from all along the wave; continuous air is emitted in proportion to the live output level.

Bottom row: NOISE panel (type < >, AMOUNT, TONE, TAIL) and LFO panel. Main row: CLICK PUNCH DROP POWER LENGTH DRIVE FM.
RANDOM (big orange button, top right) animates every knob to a random but usable 808; LFO links stay.

## 4. Build pipeline

`gh workflow run build.yml --ref main` (or GitHub -> Actions -> Build -> Run workflow), only on the owner's command.
Windows job (VST3 + Standalone + Snapshot PNGs), mac job (universal VST3 + AU + Standalone, ad-hoc signed), publish job
(force-pushes to `builds`). JUCE 8.0.10 is cloned by CI. ~15 min.
Fetch: `git clone --depth 1 --branch builds https://github.com/doublesensespb-rgb/808-bass-maker`, look at the PNGs,
rename zips with the version, send them.

## 5. Fast local UI previews on Linux

```
ln -s /path/to/JUCE JUCE        # JUCE 8.0.10
CXXFLAGS="-DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0 -DJUCE_USE_XCURSOR=0 -DJUCE_USE_XRENDER=0 -DJUCE_ALSA=0 -DJUCE_JACK=0" \
  cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Snapshot -j3      # more jobs can run out of memory
Xvfb :99 & DISPLAY=:99 ./build/Snapshot_artefacts/Release/Snapshot outdir
```

## 6. History

v0.1: first build. Engine from the Serum patch, 7 knobs (CLICK PUNCH DROP POWER LENGTH DRIVE GLIDE), 6 factory presets,
DICE, wave screen with white air/dust, SUB/OUT meters, click on the screen plays C2.

v0.11: NOISE module (WHITE PINK BROWN VINYL TAPE HUM DIRT DUST) into the drive, FM odd harmonics knob, 3 LFOs from
GRAIN FORM, preset SAVE + user presets, DICE -> big RANDOM, GLIDE knob removed, 9 factory presets.

v0.12: GRAIN FORM look, drive types (distType param), heat colour from DRIVE, sand grains from NOISE, pressure arc on attack, sand wave.

## 7. Ideas not started

EQ, tone SHAPE, drive types TUBE/HARD/FOLD/GUT,
tempo-synced LENGTH, resizable UI, installer, notarization, licence keys.
