#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace Colours808
{
    inline const juce::Colour bg       { 0xff151515 };
    inline const juce::Colour screen   { 0xff101010 };
    inline const juce::Colour button   { 0xff232323 };
    inline const juce::Colour track    { 0xff2a2a2a };
    inline const juce::Colour knob     { 0xff1e1e1e };
    inline const juce::Colour knobRim  { 0xff2e2e2e };
    inline const juce::Colour title    { 0xfff1efea };
    inline const juce::Colour text     { 0xffcfcdc8 };
    inline const juce::Colour label    { 0xffb4b2a9 };
    inline const juce::Colour dim      { 0xff8a8883 };
    inline const juce::Colour faint    { 0xff6e6c67 };
    inline const juce::Colour air      { 0xfff4f2ed };
    inline const juce::Colour accent   { 0xfff0541b };
}

class FlatLook final : public juce::LookAndFeel_V4
{
public:
    FlatLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
};

// Flat knob with its name and value under it
class Knob final : public juce::Component
{
public:
    Knob (BassProcessor&, const char* paramId, const juce::String& name, bool big = false);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::String name;
    bool big;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// The wave screen: the 808 shape, and air + dust blown out of it by the real audio
class WaveScreen final : public juce::Component
{
public:
    explicit WaveScreen (BassProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick (float dt);   // advance the animation (called by the editor timer and by the snapshot tool)

private:
    struct Dust { float x, y, vx, vy, life, z; };
    struct Ring { float t, power; };
    void rebuildShape();
    void burst (float strength);
    float xAt (float u) const;
    float yAt (int i, float scale) const;

    BassProcessor& proc;
    static constexpr int NS = 600;
    std::array<float, NS> wave {}, env {};
    std::vector<Dust> dust;
    std::vector<Ring> rings;
    juce::Random rnd { 808 };
    int lastHits = 0;
    float level = 0, power = 0.6f, playU = 1.0f, flash = 0, phaseT = 0, emitAcc = 0;
    double windowSec = 1.5;
    bool previewHeld = false;
};

class Meter final : public juce::Component
{
public:
    void set (float v) { value = juce::jmax (v, value * 0.88f); repaint(); }
    void paint (juce::Graphics&) override;
private:
    float value = 0;
};

class BassEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BassEditor (BassProcessor&);
    ~BassEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick (float dt);   // one animation frame
    WaveScreen& waveScreen() { return screen; }

private:
    void timerCallback() override { tick (1.0f / 60.0f); }
    void stepPreset (int delta);
    void rollDice();

    BassProcessor& proc;
    FlatLook look;
    WaveScreen screen;
    Meter subMeter, outMeter;
    Knob click, punch, drop, power, length, drive, glide;
    juce::TextButton prev { "<" }, next { ">" }, presetBox, dice { "DICE" };
    juce::Random rnd;

    // DICE / preset animation: knobs glide to their new values
    struct Anim { const char* id; float from, to; };
    std::vector<Anim> anim;
    float animT = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassEditor)
};
