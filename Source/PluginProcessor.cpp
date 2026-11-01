#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

BassProcessor::BassProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", layout())
{
    loadPreset (0);
}

AudioProcessorValueTreeState::ParameterLayout BassProcessor::layout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    const bass::Params d;
    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    auto add = [&] (const char* id, const char* name, float def)
    {
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def,
                                                             AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    };
    add (ParamID::click,  "Click",  d.click);
    add (ParamID::punch,  "Punch",  d.punch);
    add (ParamID::drop,   "Drop",   d.drop);
    add (ParamID::power,  "Power",  d.power);
    add (ParamID::length, "Length", d.length);
    add (ParamID::drive,  "Drive",  d.drive);
    add (ParamID::glide,  "Glide",  d.glide);
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f,
                  AudioParameterFloatAttributes().withLabel ("dB")));
    return { ps.begin(), ps.end() };
}

bool BassProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == AudioChannelSet::stereo() || out == AudioChannelSet::mono();
}

void BassProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    mono.allocate ((size_t) jmax (1, samplesPerBlock), true);
    monoSize = jmax (1, samplesPerBlock);
    keyboardState.reset();
}

bass::Params BassProcessor::currentParams() const
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    bass::Params p;
    p.click = v (ParamID::click);   p.punch = v (ParamID::punch); p.drop = v (ParamID::drop);
    p.power = v (ParamID::power);   p.length = v (ParamID::length);
    p.drive = v (ParamID::drive);   p.glide = v (ParamID::glide); p.output = v (ParamID::output);
    return p;
}

void BassProcessor::handle (const MidiMessage& m)
{
    if (m.isNoteOn()) engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    else if (m.isPitchWheel()) engine.setPitchBend ((m.getPitchWheelValue() - 8192) / 8192.0 * 2.0);
}

void BassProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    if (n > monoSize) { mono.allocate ((size_t) n, true); monoSize = n; }

    engine.setParams (currentParams());

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = jlimit (0, n, meta.samplePosition);
        if (at > pos) { engine.render (mono.get() + pos, at - pos); pos = at; }
        handle (meta.getMessage());
    }
    if (pos < n) engine.render (mono.get() + pos, n - pos);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, mono.get(), n);
}

const String BassProcessor::getProgramName (int i)
{
    const auto& l = bass::factoryPresets();
    return isPositiveAndBelow (i, (int) l.size()) ? String (l[(size_t) i].name) : String();
}

static void setParam (AudioProcessorValueTreeState& s, const char* id, float v)
{
    if (auto* p = s.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void BassProcessor::loadPreset (int index)
{
    const auto& l = bass::factoryPresets();
    index = (index % (int) l.size() + (int) l.size()) % (int) l.size();
    const auto& p = l[(size_t) index].p;
    setParam (apvts, ParamID::click, p.click);   setParam (apvts, ParamID::punch, p.punch);
    setParam (apvts, ParamID::drop, p.drop);     setParam (apvts, ParamID::power, p.power);
    setParam (apvts, ParamID::length, p.length); setParam (apvts, ParamID::drive, p.drive);
    setParam (apvts, ParamID::glide, p.glide);   setParam (apvts, ParamID::output, p.output);
    presetIndex = index;
    presetName = l[(size_t) index].name;
}

bass::Params BassProcessor::diceParams (Random& r)
{
    // ranges chosen so every roll is a usable 808: clean sub is always there, tails never too short
    bass::Params p;
    p.click  = 0.25f + 0.75f * r.nextFloat();
    p.punch  = 0.15f + 0.75f * r.nextFloat();
    p.drop   = 0.60f * r.nextFloat() * r.nextFloat();
    p.power  = 0.45f + 0.50f * r.nextFloat();
    p.length = 0.40f + 0.60f * r.nextFloat();
    p.drive  = r.nextFloat() < 0.2f ? 0.05f * r.nextFloat() : 0.15f + 0.75f * r.nextFloat();
    p.glide  = 0.15f + 0.60f * r.nextFloat();
    return p;
}

void BassProcessor::randomize (Random& r)
{
    const auto p = diceParams (r);
    setParam (apvts, ParamID::click, p.click);   setParam (apvts, ParamID::punch, p.punch);
    setParam (apvts, ParamID::drop, p.drop);     setParam (apvts, ParamID::power, p.power);
    setParam (apvts, ParamID::length, p.length); setParam (apvts, ParamID::drive, p.drive);
    setParam (apvts, ParamID::glide, p.glide);
    presetIndex = -1;
    presetName = "DICE #" + String (100 + r.nextInt (900));
}

void BassProcessor::getStateInformation (MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("presetName", presetName, nullptr);
    st.setProperty ("presetIndex", presetIndex, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void BassProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto st = ValueTree::fromXml (*xml);
        if (st.isValid() && st.hasType (apvts.state.getType()))
        {
            presetName = st.getProperty ("presetName", "THEMATIC 808").toString();
            presetIndex = (int) st.getProperty ("presetIndex", 0);
            apvts.replaceState (st);
        }
    }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BassProcessor(); }
