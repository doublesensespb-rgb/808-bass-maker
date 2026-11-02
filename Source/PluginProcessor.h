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
    inline constexpr const char* glide  = "glide";     // legacy: knob removed in 0.11, ignored
    inline constexpr const char* output = "output";
    inline constexpr const char* fm     = "fm";
    inline constexpr const char* noise     = "noise";
    inline constexpr const char* noiseTone = "noiseTone";
    inline constexpr const char* noiseTail = "noiseTail";
    inline constexpr const char* noiseType = "noiseType";
    inline constexpr const char* distType  = "distType";
    inline constexpr const char* pitch     = "pitch";      // since 0.13
    inline constexpr const char* ampOn     = "ampOn";      // ADSR amp (same IDs as GRAIN FORM)
    inline constexpr const char* ampA      = "ampA";
    inline constexpr const char* ampD      = "ampD";
    inline constexpr const char* ampS      = "ampS";
    inline constexpr const char* ampR      = "ampR";
    // LFOs (same scheme as GRAIN FORM): "lfo1Rate", "lfo1Sync", "lfo1Div", "lfo1Mode", "lfo1Rise", "lfo1On"
    inline juce::String lfo (int index, const char* what) { return "lfo" + juce::String (index + 1) + what; }
}

static constexpr int kNumLfos = 3;
static constexpr int kMaxMods = 48;
static constexpr int kLfoTable = 512;
static constexpr int kLfoPoints = 4;
static constexpr int kChunk = 32;   // modulation is applied every 32 samples

enum ModTarget
{
    mtClick, mtPunch, mtDrop, mtPower, mtLength, mtDrive, mtFm, mtOutput,
    mtNoise, mtNoiseTone, mtNoiseTail,
    mtLfo1Rate, mtLfo2Rate, mtLfo3Rate,
    mtPitch,
    kNumModTargets
};

struct LfoPoint { float x = 0, y = 0, curve = 0; };

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
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int i) override { loadFactory (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    bass::Params currentParams() const;       // knob positions
    bass::Params modulatedParams() const;     // what is playing right now (knobs + LFOs), for the screen

    // ---- presets: factory (built in) + user files in Documents/808 BASS MAKER/Presets ----
    static juce::File presetFolder();
    juce::Array<juce::File> listUserPresets() const;
    juce::StringArray allPresetNames() const;   // factory first, then user
    void loadPresetByIndex (int index);          // index into allPresetNames()
    int currentPresetIndex() const;
    void loadFactory (int index);
    bool savePreset (const juce::String& name);
    bool loadPreset (const juce::File&);
    juce::String getPresetName() const { return presetName; }
    void setPresetName (const juce::String& n) { presetName = n; }

    // RANDOM: random but always a working 808 (target values; the UI animates to them)
    static void randomTargets (juce::Random&, std::vector<std::pair<const char*, float>>& out);

    // ---- modulation (same as GRAIN FORM) ----
    static const char* modTargetId (int target);
    static int modIndexOf (const juce::String& paramId);
    void setModulation (int target, int lfo, float depth);
    void removeModulation (int target, int lfo);
    void clearModulations();
    bool hasModulation (int target, int lfo) const;
    float getModulationDepth (int target, int lfo) const;
    int countModulations (int lfo) const;
    bool isLfoOn (int lfo) const { return pLfoOn[(size_t) lfo]->load() > 0.5f; }

    std::array<LfoPoint, kLfoPoints> getLfoPoints (int lfo) const;
    void setLfoPoints (int lfo, std::array<LfoPoint, kLfoPoints> pts);
    float lfoShapeAt (int lfo, float phase) const;
    static std::array<LfoPoint, kLfoPoints> lfoPreset (int index);
    static const juce::StringArray& lfoPresetNames();
    static float segmentShape (float t, float curve);
    static double divisionBeats (int index);
    static const juce::StringArray& divisionNames();

    std::array<std::atomic<float>, kNumModTargets> modNorm {};
    std::array<std::atomic<float>, kNumLfos> lfoValueOut {}, lfoPhaseOut {}, lfoRiseOut {};

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;   // the screen and the snapshot tool play notes through it
    bass::Engine engine;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    void handle (const juce::MidiMessage&);
    void renderRange (int from, int to);
    void computeModulated (float* values);
    void advanceLfos (int endSample, int len, const float* values);
    void rebuildLfoTable (int lfo);
    void resetLfos();
    bass::Params paramsFrom (const float* v) const;

    juce::HeapBlock<float> mono;
    int monoSize = 0;
    juce::String presetName { "THEMATIC 808" };

    struct ModSlot
    {
        std::atomic<int>   target { -1 };
        std::atomic<int>   lfo    { 0 };
        std::atomic<float> depth  { 0.0f };
    };
    std::array<ModSlot, kMaxMods> mods;
    std::array<juce::RangedAudioParameter*, kNumModTargets> modParams {};

    std::array<std::array<LfoPoint, kLfoPoints>, kNumLfos> lfoPoints {};
    std::array<std::array<std::array<float, kLfoTable + 1>, 2>, kNumLfos> lfoTables {};
    std::array<std::atomic<int>, kNumLfos> lfoTableIndex {};
    mutable juce::CriticalSection lfoLock;

    std::array<double, kNumLfos> lfoPhase {};
    std::array<float, kNumLfos>  lfoValue {};
    std::array<bool, kNumLfos>   lfoDone {};
    double sinceTrigger = 1.0e9;
    double hostBpm = 120.0, hostPpq = -1.0;
    bool hostPlaying = false;

    std::atomic<float>* pNoiseType {};
    std::atomic<float>* pDistType {};
    std::atomic<float>* pAmpOn {}; std::atomic<float>* pAmpA {}; std::atomic<float>* pAmpD {}; std::atomic<float>* pAmpS {}; std::atomic<float>* pAmpR {};
    std::array<std::atomic<float>*, kNumLfos> pLfoSync {}, pLfoDiv {}, pLfoMode {}, pLfoRise {}, pLfoOn {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassProcessor)
};
