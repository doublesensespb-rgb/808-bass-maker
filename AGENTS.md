# 808 BASS MAKER (repo: 808-bass-maker) — guide for AI agents and developers

Read this whole file before changing anything. Current version: **0.13** (`project(BassMaker VERSION 0.1.3)` in
CMakeLists.txt, "v0.13" text in `BassEditor::paint`). Owner's guide in Russian: `README-RU.md`.

## 1. Product

808 BASS MAKER, subtitle "HEAVY 808 DESIGNER", is a commercial VST3 / AU / Standalone **mono 808 synth** by
Andrey Ulyanov (artist name Thematic, manufacturer "Thematic"), producer and game sound designer, FL Studio on Windows.
Goal: dial in different heavy 808s fast. "Bass that moves your guts": power first, but the low end stays clean.
Sister project: GRAIN FORM (https://github.com/doublesensespb-rgb/granulum) — the LFO system, presets, CI and the look come from it.

The sound follows the principle of the owner's Serum 808 patch:

```
sine (OCT -2)
 + 3 pitch envelopes: CLICK (very fast, Serum LFO 4), PUNCH (~1/4, exp, LFO 3), DROP (slow linear over the tail, LFO 2)
 + linear volume tail = LENGTH (Serum LFO 1 -> A Vol; ENV 1 is only a gate: A 0.5 ms, R 15 ms)
 + NOISE (8 built-in generated types) mixed INTO the drive, louder on the sine crests, follows the tail (Serum LFO 5 -> Noise):
   the drive crushes it into crunch instead of hiss
 + FM: modulator at 2x the carrier = ODD harmonics only (3,5,7,9), band-limited to 160-1600 Hz (the "squeak")
 -> DRIVE, amount falls with the tail (Serum LFO 1 -> Dist Drive); types ASYM (Serum) / TUBE / HARD / FOLD (GRAIN FORM) / RECTIFY
 -> clean sub below 90 Hz (LR4 split, mono sine) + dirt above it
 -> high cut 9 kHz -> DC block -> output clipper (POWER pushes into it)
Mono, last-note priority, LEGATO off (envelopes retrigger on every note), no glide, pitch bend +/-2.
Since 0.13 it plays TWO OCTAVES below the played key (`kBaseOctaveShift`); global PITCH knob -24..+24 st on top.
ADSR amp (as in GRAIN FORM, same param IDs ampOn/ampA/ampD/ampS/ampR): off (default) = the LENGTH tail; on = volume
follows the ADSR while the key is held (LENGTH still sets the DROP time). Old states without `pitch` get PITCH +24.
Everything runs at 4x oversampling, decimated by two half-band FIR stages.
```

POWER is a macro: more drive, more punch depth, more sub, harder clipper; on screen a thicker wave and a bigger blast.

3 LFOs exactly like GRAIN FORM: 4-point drawable shapes with bendable segments, drag an LFO tab onto any knob,
chips on the knob = depth (drag) / double-click remove / right-click invert, lamp on the tab = LFO on/off (links kept),
REMOVE LINKS, SHAPE presets, MODE FREE / RETRIG / ENV (default RETRIG = restarts on every note), DIVISION + SYNC, RATE, RISE.
Modulation is applied every 32 samples.

Presets: 9 factory presets (in code) + user presets = XML files `Documents/808 BASS MAKER/Presets/*.808preset`
(SAVE in the header; same name = overwrite). State and presets store the knobs, LFO links and LFO shapes.
RANDOM (big copper button): animates every knob to random but always usable values (`randomTargets`); LFO links stay.

## 2. Owner's rules (important)

- The owner does not code. The agent does everything itself: code, preview, build, check, deliver files.
  No step-by-step instructions for him unless he asks.
- Before creating or generating anything, ask him to confirm.
- When he sends change requests: FIRST agree on the changes in text, THEN implement.
- Visual changes: render a preview picture locally (section 5) and show it BEFORE any CI build.
- **NEVER start a CI build without his explicit command** ("собирай", "делай сборку", "начинай собирать").
  Docs-only commits get `[skip ci]` in the message.
- He writes Russian, often with typos / wrong keyboard layout. **Reply in Russian.** All UI text in the plugin is English.
- **Always give links** (repo, builds, tools, sources).
- Version naming: 0.1, 0.11, 0.12, 0.13 ... (then 1.0 for the release). Files with the version in the name:
  `808-BASS-MAKER-0.13-Windows.zip`, `808-BASS-MAKER-0.13-macOS.zip`.
  Bump `VERSION` in CMakeLists.txt (0.13 -> 0.1.3) and the "v0.13" text in `BassEditor::paint`.
- Design (since 0.12) = GRAIN FORM style: graphite background, rounded panels, quiet grey flat knobs (disc + one light
  pointer line, NO value ring, no highlights / 3D / gradients), name above the knob, value in a dark box below,
  light wide spaced title on the left. Accent: copper `0xffd48c5c`. Palette in `Colours808` (PluginEditor.h).
- Screen logic: pearl-white air — the bass BLOWS like a big sound system. DRIVE heats it (pearl -> peach -> copper -> fire,
  `Colours808::heat`). NOISE AMOUNT adds sand grains + a grainy sand wave (`Colours808::sand`, never heat-tinted).
  Dust is FINE (0.6-1.4 px, thousands of grains, like noise). **No arc / rings on the attack** (owner removed it):
  only a small flash + the dust blast.

### Never change (saved FL projects and presets would break)
- `PLUGIN_MANUFACTURER_CODE Thmt`, `PLUGIN_CODE B808`, `BUNDLE_ID com.thematic.808bassmaker`, `PRODUCT_NAME "808 BASS MAKER"`.
- Parameter IDs (`ParamID` in `Source/PluginProcessor.h`, plus `lfoNRate/Sync/Div/Mode/Rise/On`). Never rename or remove.
  If a feature goes away, keep the parameter and ignore it (example: `glide`, knob removed in 0.11, engine gets 0).
- Choice parameters (`noiseType`, `distType`, LFO `Div`/`Mode`): only APPEND new entries at the end.
- Preset format `.808preset`: only add new properties, never rename old ones.

## 3. Code map

| File | What it is |
|---|---|
| `Source/BassEngine.h` | Header-only DSP, no JUCE: `Params`, mapping functions shared with the screen (`pitchShape`, `ampShape`, `tailSeconds`, `driveGain`, `fmPart`...), `driveShape` (5 drive types), `NoiseGen` (8 noises), `Engine` (mono voice, 4x oversampling, sub split, clipper, viz atomics, `publishViz`), `factoryPresets()` |
| `Source/PluginProcessor.*` | Parameters, MIDI, mod matrix + 3 LFOs (ported from GRAIN FORM: `computeModulated`, `advanceLfos`, LFO tables), factory + user presets, `randomTargets`, state save/load (mods + LFO shapes) |
| `Source/PluginEditor.*` | UI 900x720: `Colours808`, `FlatLook`, `Knob` (+ `ModChip`, LFO drop target), `WaveScreen` (wave, glow, dust, sand), `Meter`, `PresetBar`, `ChoicePicker` (noise type, drive type), `AmpTab` + `AmpEditor` (ADSR from GRAIN FORM), `LfoTab`, `LfoEditor`, `BassEditor` |
| `tools/Snapshot.cpp` | Renders the editor to PNG while notes play: `ui-idle`, `ui-impact`, `ui-tail`, `ui-sand`, `ui-gut` (`frames` mode = animation strip) |
| `test/engine_test.cpp` | Plain C++ engine tests (presets, pitch, release, noises, drive types, FM odd harmonics) |
| `.github/workflows/build.yml` | CI: Windows + macOS builds, publishes zips + PNG previews to the `builds` branch. Manual start only |
| `tools/build-windows.bat` | One-click local Windows build (Visual Studio 2022 + CMake), result in `out/` |
| `README-RU.md` | Owner's guide in Russian |

Main row: PITCH CLICK PUNCH DROP POWER LENGTH DRIVE (+ drive type picker) FM. Bottom: NOISE panel (type, AMOUNT, TONE, TAIL)
and MODULATION panel: tabs ADSR amp / LFO 1-3, a WIDE rectangular screen (top = ADSR tab, bottom = LFO 3 tab; owner's
request), LFO controls (SHAPE, MODE, DIVISION, SYNC, REMOVE LINKS, RATE, RISE) or ADSR knobs (ATTACK DECAY SUSTAIN RELEASE).

Screen (`WaveScreen`): the wave is drawn from the same mapping functions as the DSP (modulated values), on a stretched
time axis (t = u^2) with softened pitch so CLICK/PUNCH are visible and the sine stays readable. Dust follows the knobs:
POWER = blast size/force, PUNCH = amount, CLICK = speed, LENGTH = how long it hangs, DROP = how fast it settles,
DRIVE = heat colour + more air, NOISE = sand grains + sand wave, FM = fine shimmer, velocity = blast strength;
continuous air is emitted in proportion to the live output level. Click on the screen plays C2.

## 4. Build pipeline

`gh workflow run build.yml --ref main` (or GitHub -> Actions -> Build -> Run workflow) — ONLY on the owner's command.
1. `windows` (windows-latest, MSVC): VST3 + Standalone + Snapshot PNGs -> `808-BASS-MAKER-Windows.zip`.
2. `mac` (macos-latest, Xcode): universal arm64+x86_64 VST3 + AU + Standalone, ad-hoc codesign, README.txt -> `808-BASS-MAKER-macOS.zip`.
3. `publish`: force-pushes zips + PNGs to the `builds` branch.
JUCE 8.0.10 is cloned by CI (not committed). A build takes ~15 min.

Workflow loop:
1. Agree on changes with the owner (text). Work on a branch if not approved yet.
2. Edit code, run the engine tests: `g++ -std=c++17 -O2 -ISource test/engine_test.cpp -o et && ./et`.
3. Visual change -> render previews with Snapshot (section 5), show them, get approval.
4. On his command: bump VERSION + "v0.x" text, merge to `main`, push, start the workflow, wait (`gh run list -L 1`).
5. Fetch: `git clone --depth 1 --branch builds https://github.com/doublesensespb-rgb/808-bass-maker`.
6. LOOK at the PNGs (rendered on Windows by CI), fix anything broken.
7. Rename zips with the version, send them with a short summary.

Install, Windows: replace `808 BASS MAKER.vst3` in `C:\Program Files\Common Files\VST3`, rescan in FL Studio.
Mac: `README.txt` inside the mac zip (copy to `/Library/Audio/Plug-Ins/...`, remove quarantine with `xattr`).

## 5. Fast local UI previews on Linux (no CI)

```
ln -s /path/to/JUCE JUCE        # JUCE 8.0.10
CXXFLAGS="-DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0 -DJUCE_USE_XCURSOR=0 -DJUCE_USE_XRENDER=0 -DJUCE_ALSA=0 -DJUCE_JACK=0" \
  cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Snapshot -j3      # more jobs can run out of memory
Xvfb :99 & DISPLAY=:99 ./build/Snapshot_artefacts/Release/Snapshot outdir
```
The CXXFLAGS switch off optional X11 extensions whose headers may be missing. Remove the JUCE symlink before `git add`.

## 6. History

- **0.1**: first build. Engine from the Serum patch, CLICK PUNCH DROP POWER LENGTH DRIVE GLIDE, 6 factory presets, DICE,
  dark flat UI with white air/dust screen, SUB/OUT meters.
- **0.11**: NOISE module (WHITE PINK BROWN VINYL TAPE HUM DIRT DUST) into the drive, FM odd-harmonics knob, 3 LFOs from
  GRAIN FORM, preset SAVE + user presets, DICE -> big RANDOM, GLIDE knob removed, 9 factory presets.
- **0.12**: GRAIN FORM look (graphite, grey knobs, copper), drive types ASYM / TUBE / HARD / FOLD / RECTIFY (`distType`),
  DRIVE heats the screen, NOISE sand grains + sand wave, fine dense dust driven by the knobs, attack arc removed.
- **0.13**: plays two octaves lower by default, global PITCH knob (LFO-able), ADSR amp from GRAIN FORM, wide LFO/ADSR screen.

## 7. Ideas not started / release to-do

EQ / tone SHAPE module, more factory presets (20-50), user preset folders / tags, resizable UI, tempo-synced LENGTH,
Windows installer (Inno Setup), Apple notarization (owner's Apple Developer account, $99/yr), pluginval / auval in CI,
copy protection / licence keys (owner undecided), EULA, JUCE licence check before selling (free Starter tier up to
$20k/yr revenue).
