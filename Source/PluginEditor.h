#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace Colours808
{
    // GRAIN FORM style: graphite, quiet grey knobs, copper accent
    inline const juce::Colour bg       { 0xff1f2123 };
    inline const juce::Colour panel    { 0xff28292b };
    inline const juce::Colour screen   { 0xff121212 };
    inline const juce::Colour button   { 0xff3a3d41 };
    inline const juce::Colour field    { 0xff1b1c1e };   // value boxes, preset name
    inline const juce::Colour track    { 0xff2e3033 };
    inline const juce::Colour knob     { 0xff45484c };
    inline const juce::Colour knobRim  { 0xff5c5f63 };
    inline const juce::Colour title    { 0xffece6df };
    inline const juce::Colour text     { 0xffd9d3cc };
    inline const juce::Colour label    { 0xffece6df };
    inline const juce::Colour dim      { 0xff968f88 };
    inline const juce::Colour faint    { 0xff6c6863 };
    inline const juce::Colour air      { 0xfff4f0ea };   // the air: pearl white
    inline const juce::Colour sand     { 0xffd6be9c };   // NOISE grains
    inline const juce::Colour accent   { 0xffd48c5c };   // copper
    inline const juce::Colour accentDeep { 0xff956443 };
    // DRIVE heats the air: pearl -> peach -> copper -> fire
    inline juce::Colour heat (float d)
    {
        d = juce::jlimit (0.0f, 1.0f, d);
        const juce::Colour c0 (0xfff4f0ea), c1 (0xfff2c9a8), c2 (0xffe08a55), c3 (0xffff5a1f);
        return d < 0.33f ? c0.interpolatedWith (c1, d / 0.33f) : d < 0.66f ? c1.interpolatedWith (c2, (d - 0.33f) / 0.33f)
                                                                         : c2.interpolatedWith (c3, (d - 0.66f) / 0.34f);
    }
    // LFO colours: copper, sand, pearl blue
    inline juce::Colour lfo (int i) { return i == 0 ? juce::Colour (0xffe0965f) : i == 1 ? juce::Colour (0xffd6be9c) : juce::Colour (0xffa9c4d6); }
}

class FlatLook final : public juce::LookAndFeel_V4
{
public:
    FlatLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
};

class Knob;

// Small coloured circle next to a modulated knob: drag up/down = depth, double-click = remove
class ModChip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModChip (BassProcessor&, Knob& owner, int lfo);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    const int lfo;
private:
    BassProcessor& proc;
    Knob& owner;
    float startDepth = 0.0f;
};

// Flat knob with its name and value under it. LFOs can be dropped on it.
class Knob final : public juce::Component, public juce::DragAndDropTarget
{
public:
    enum class Size { Small, Normal, Big };
    Knob (BassProcessor&, const juce::String& paramId, const juce::String& name, Size = Size::Normal);
    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

    void refreshMods();
    void tick();

    juce::Slider slider;
    const int target;
private:
    float knobSize() const;
    BassProcessor& proc;
    juce::String name;
    Size size;
    juce::RangedAudioParameter* param = nullptr;
    bool dragHover = false, anyMod = false;
    std::array<std::unique_ptr<ModChip>, kNumLfos> chips;
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
    void tick (float dt);

private:
    struct Dust { float x, y, vx, vy, life, z; bool sand = false; };
    bool rollSand();
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
    float level = 0, power = 0.6f, heatAmt = 0, noiseAmt = 0, playU = 1.0f, flash = 0, phaseT = 0, emitAcc = 0, sparkAcc = 0;
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

// Header preset browser: < NAME > SAVE
class PresetBar final : public juce::Component
{
public:
    explicit PresetBar (BassProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<void (int)> onPick;   // index into allPresetNames()
private:
    void step (int dir);
    void showMenu();
    void askSave();
    BassProcessor& proc;
    juce::TextButton prev { "<" }, next { ">" }, save { "SAVE" };
    juce::Rectangle<int> nameArea;
    std::unique_ptr<juce::AlertWindow> saveWindow;
};

// Choice picker (NOISE type, DRIVE type): < NAME >, click the name for the list
class ChoicePicker final : public juce::Component
{
public:
    ChoicePicker (BassProcessor&, const char* paramId, juce::StringArray names, bool noiseTexture);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    int index() const;
    void setIndex (int);
    BassProcessor& proc;
    juce::String paramId;
    juce::StringArray names;
    bool texture;
    juce::TextButton prev { "<" }, next { ">" };
    juce::Rectangle<int> body;
};

// LFO tab: click the light = on/off, click = select, drag onto any knob = modulate it
class LfoTab final : public juce::Component, public juce::SettableTooltipClient
{
public:
    LfoTab (BassProcessor&, int index);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<void (int)> onSelect;
    bool selected = false;
    const int index;
private:
    BassProcessor& proc;
    bool dragging = false;
};

// 4-point drawable LFO shape: drag points, drag the rings on each segment to bend it
class LfoEditor final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LfoEditor (BassProcessor&);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void setLfo (int l);
    void tick();
    int lfo = 0;
private:
    juce::Rectangle<float> area() const;
    juce::Point<float> toScreen (float x, float y) const;
    juce::Point<float> handlePos (int seg) const;
    int hitTest (juce::Point<float>, bool& isHandle) const;
    BassProcessor& proc;
    std::array<LfoPoint, kLfoPoints> pts {};
    int dragIndex = -1, hoverIndex = -1;
    bool dragHandle = false, hoverHandle = false;
    float dragStartCurve = 0.0f;
    juce::Point<float> dragStart;
};

// ADSR amp tab (as in GRAIN FORM): click the light = envelope on/off (off = the 808 LENGTH tail), click = select
class AmpTab final : public juce::Component, public juce::SettableTooltipClient
{
public:
    AmpTab();
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<void()> onSelect;
    bool selected = false;
    juce::RangedAudioParameter* param = nullptr;
};

// ADSR drawn as a curve (GRAIN FORM): drag the attack peak, the decay/sustain corner and the release end
class AmpEditor final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit AmpEditor (BassProcessor&);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    struct Geo { juce::Point<float> p0, p1, p2, p3, p4; juce::Rectangle<float> a; float maxW = 0, plateau = 0; };
    Geo geometry() const;
    float value (const char* id) const;
    void setValue (const char* id, float v);
    BassProcessor& proc;
    int dragging = -1, hover = -1;
};

class BassEditor final : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit BassEditor (BassProcessor&);
    ~BassEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick (float dt);
    void selectLfo (int i);
    void refreshAllMods();

private:
    void timerCallback() override { tick (1.0f / 60.0f); }
    void randomize();
    void afterPresetLoad();

    using ComboAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    BassProcessor& proc;
    FlatLook look;
    WaveScreen screen;
    Meter subMeter, outMeter;
    PresetBar presetBar;
    juce::TextButton randomButton { "RANDOM" };
    std::vector<std::unique_ptr<Knob>> mainKnobs, noiseKnobs;
    ChoicePicker noisePicker, drivePicker;

    struct LfoControls
    {
        juce::ComboBox preset, div, mode;
        juce::TextButton sync { "SYNC" }, remove { "REMOVE LINKS" };
        std::unique_ptr<Knob> rate, rise;
        std::unique_ptr<ComboAtt> divA, modeA;
        std::unique_ptr<ButtonAtt> syncA;
    };
    std::array<std::unique_ptr<LfoTab>, kNumLfos> lfoTabs;
    std::array<std::unique_ptr<LfoControls>, kNumLfos> lfoControls;
    LfoEditor lfoEditor;
    AmpTab ampTab;
    AmpEditor ampEditor;
    std::vector<std::unique_ptr<Knob>> ampKnobs;
    Knob* powerKnob = nullptr;
    Knob* driveKnob = nullptr;
    int selectedLfo = 0;      // 0..2 = LFO, 3 = ADSR

    juce::Rectangle<int> noisePanel, lfoPanel, topPanel;
    juce::Random rnd;
    std::vector<std::pair<const char*, float>> animTo;
    std::vector<float> animFrom;
    float animT = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassEditor)
};
