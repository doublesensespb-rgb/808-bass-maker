# 808 BASS MAKER (repo: 808-bass-maker) — guide for AI agents and developers

Read this whole file before changing anything. Current version: **0.1** (`project(... VERSION 0.1.0)` in CMakeLists.txt).

## 1. Product

808 BASS MAKER, subtitle "HEAVY 808 DESIGNER", is a commercial VST3/AU/Standalone mono synth by Andrey Ulyanov
(artist name Thematic, manufacturer "Thematic"), a producer and game sound designer who uses FL Studio on Windows.
Goal: dial in different heavy 808s fast. "Bass that moves your guts" — power first, but the low end stays clean.

The sound is built on the principle of the owner's Serum 808 patch:

```
sine (OCT -2)
 + 3 pitch envelopes: CLICK (very fast, Serum LFO 4), PUNCH (~1/4, exp, LFO 3), DROP (slow linear over the tail, LFO 2)
 + linear volume tail = LENGTH (Serum LFO 1 -> A Vol, ENV 1 is just a gate: A 0.5 ms, R 15 ms)
 + short high-passed noise transient (Serum LFO 5 -> Noise level)
 -> ASYM distortion whose drive falls with the tail (Serum LFO 1 -> Dist Drive)
 -> clean sub below 90 Hz (LR4 split, mono sine) + dirt above it
 -> EQ high cut 9 kHz -> DC block -> output clipper (POWER pushes into it)
Mono, last-note priority, LEGATO off (envelopes retrigger), GLIDE only on overlapping notes, pitch bend +/-2.
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
- Design: dark graphite, flat. Knobs: flat dark disc, thin orange value arc, white pointer, NO highlights/3D/gradients.
  One accent colour: orange `0xfff0541b`. The "air" on the screen is white: the bass BLOWS (like big sound systems), it does not burn.
  ALL UI text is English (no Russian in the plugin).
- **Never change** `PLUGIN_MANUFACTURER_CODE Thmt`, `PLUGIN_CODE B808`, `BUNDLE_ID com.thematic.808bassmaker`, `PRODUCT_NAME`.
- **Never rename or remove parameter IDs** (`ParamID` in PluginProcessor.h). If a feature goes away, keep the parameter and ignore it.

## 3. Code map

| File | What it is |
|---|---|
| `Source/BassEngine.h` | Header-only DSP: parameter mapping (`pitchShape`, `ampShape`, `driveGain`...), `Engine` (voice, oversampling, drive, sub split, clipper), viz atomics, factory presets |
| `Source/PluginProcessor.*` | Parameters, sample-accurate MIDI, presets, DICE ranges (`diceParams`), state save/load |
| `Source/PluginEditor.*` | UI 900x570: `FlatLook`, `Knob`, `WaveScreen` (wave + air contours + dust bursts that react to the real audio), `Meter`, `BassEditor` |
| `tools/Snapshot.cpp` | Renders the editor to PNG while a note plays (`ui-idle/impact/tail/gut.png`; `frames` mode = animation strip) |
| `test/engine_test.cpp` | `g++ -std=c++17 -O2 -ISource test/engine_test.cpp -o et && ./et` |
| `.github/workflows/build.yml` | CI: Windows + macOS, publishes zips + PNGs to the `builds` branch (manual start only) |
| `tools/build-windows.bat` | Local one-click Windows build (VS 2022 + CMake) |

The screen draws the wave from the same mapping functions as the DSP, on a stretched time axis (t = u^2) with the
pitch softened, so CLICK/PUNCH are visible and the sine stays readable. Dust: a burst on every note (`vizHits`),
60% from the attack point, 40% from all along the wave; continuous air is emitted in proportion to the live output level.

Three dashed `MODULE` slots under the knobs are reserved for future blocks (owner's request).

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

## 7. Ideas not started

Modules for the 3 slots (TRANSIENT type, EQ, LFO, tone SHAPE, drive types TUBE/HARD/FOLD/GUT), user presets SAVE,
tempo-synced LENGTH, resizable UI, installer, notarization, licence keys.
