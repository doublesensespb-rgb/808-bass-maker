#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "BassEngine.h"

// Parameter IDs. NEVER rename or remove them: saved FL projects would break.
namespace ParamID
{
    inline constexpr const char* click  = "click";
    inline constexpr const char* punch  = "punch";
    inline constexpr const char* drop   = "drop";
    inline constexpr const char* power  = "power";
    inline constexpr const char* length = "length";
    inline constexpr const char* drive  = "drive";
    inline constexpr const char* glide  = "glide";
    inline constexpr const char* output = "output";
}

class BassProcessor final : public juce::AudioProcessor
{
public:
    BassProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "808 BASS MAKER"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return (int) bass::factoryPresets().size(); }
    int getCurrentProgram() override { return juce::jmax (0, presetIndex); }
    void setCurrentProgram (int i) override { loadPreset (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    bass::Params currentParams() const;
    void loadPreset (int index);
    void randomize (juce::Random&);   // DICE: random but always a working 808 (target values; the UI animates to them)
    static bass::Params diceParams (juce::Random&);

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;   // used by the snapshot tool to play notes
    bass::Engine engine;

    juce::String presetName { "THEMATIC 808" };
    int presetIndex = 0;   // -1 = edited / dice

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void handle (const juce::MidiMessage&);

    juce::HeapBlock<float> mono;
    int monoSize = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassProcessor)
};
