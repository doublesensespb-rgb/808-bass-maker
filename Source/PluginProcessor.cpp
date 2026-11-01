#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

static const char* const kModTargetIds[kNumModTargets] = {
    ParamID::click, ParamID::punch, ParamID::drop, ParamID::power, ParamID::length, ParamID::drive, ParamID::fm, ParamID::output,
    ParamID::noise, ParamID::noiseTone, ParamID::noiseTail,
    "lfo1Rate", "lfo2Rate", "lfo3Rate"
};

BassProcessor::BassProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", layout())
{
    for (int t = 0; t < kNumModTargets; ++t)
    {
        modParams[(size_t) t] = apvts.getParameter (kModTargetIds[t]);
        jassert (modParams[(size_t) t] != nullptr);
    }
    pNoiseType = apvts.getRawParameterValue (ParamID::noiseType);
    pDistType = apvts.getRawParameterValue (ParamID::distType);
    for (int l = 0; l < kNumLfos; ++l)
    {
        pLfoSync[(size_t) l] = apvts.getRawParameterValue (ParamID::lfo (l, "Sync"));
        pLfoDiv[(size_t) l]  = apvts.getRawParameterValue (ParamID::lfo (l, "Div"));
        pLfoMode[(size_t) l] = apvts.getRawParameterValue (ParamID::lfo (l, "Mode"));
        pLfoRise[(size_t) l] = apvts.getRawParameterValue (ParamID::lfo (l, "Rise"));
        pLfoOn[(size_t) l]   = apvts.getRawParameterValue (ParamID::lfo (l, "On"));
        setLfoPoints (l, lfoPreset (1));   // triangle
    }
    loadFactory (0);
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
    add (ParamID::glide,  "Glide (legacy)", 0.0f);
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f,
                  AudioParameterFloatAttributes().withLabel ("dB")));
    add (ParamID::fm,        "FM",         d.fm);
    add (ParamID::noise,     "Noise",      d.noise);
    add (ParamID::noiseTone, "Noise Tone", d.noiseTone);
    add (ParamID::noiseTail, "Noise Tail", d.noiseTail);
    StringArray nn;
    for (int i = 0; i < bass::kNumNoises; ++i) nn.add (bass::noiseNames()[i]);
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::noiseType, 1 }, "Noise Type", nn, 0));
    StringArray dn;
    for (int i = 0; i < bass::kNumDists; ++i) dn.add (bass::distNames()[i]);
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::distType, 1 }, "Drive Type", dn, 0));

    auto secFmt = [] (float v, int) { return v < 0.01f ? String ("OFF") : String (v, 2) + " s"; };
    auto hzFmt  = [] (float v, int) { return String (v, v < 10.0f ? 2 : 1) + " Hz"; };
    auto skewed = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };
    for (int l = 0; l < kNumLfos; ++l)
    {
        const String n = "LFO " + String (l + 1) + " ";
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::lfo (l, "Rate"), 1 }, n + "Rate", skewed (0.02f, 30.0f, 2.0f), 1.0f,
                      AudioParameterFloatAttributes().withStringFromValueFunction (hzFmt)));
        ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamID::lfo (l, "Sync"), 1 }, n + "Sync", true));
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::lfo (l, "Div"), 1 }, n + "Division", divisionNames(), 4));
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::lfo (l, "Mode"), 1 }, n + "Mode", StringArray { "FREE", "RETRIG", "ENV" }, 1));
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::lfo (l, "Rise"), 1 }, n + "Rise", skewed (0.0f, 8.0f, 1.5f), 0.0f,
                      AudioParameterFloatAttributes().withStringFromValueFunction (secFmt)));
        ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamID::lfo (l, "On"), 1 }, n + "On", true));
    }
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
    resetLfos();
}

void BassProcessor::resetLfos()
{
    for (int l = 0; l < kNumLfos; ++l) { lfoPhase[(size_t) l] = 0.0; lfoValue[(size_t) l] = lfoShapeAt (l, 0.0f); lfoDone[(size_t) l] = false; }
}

// ===========================================================================
// Params

bass::Params BassProcessor::paramsFrom (const float* v) const
{
    bass::Params p;
    p.click = v[mtClick];  p.punch = v[mtPunch]; p.drop = v[mtDrop]; p.power = v[mtPower];
    p.length = v[mtLength]; p.drive = v[mtDrive]; p.fm = v[mtFm]; p.output = v[mtOutput];
    p.noise = v[mtNoise]; p.noiseTone = v[mtNoiseTone]; p.noiseTail = v[mtNoiseTail];
    p.noiseType = (int) pNoiseType->load();
    p.distType = (int) pDistType->load();
    p.glide = 0.0f;   // GLIDE removed in 0.11
    return p;
}

bass::Params BassProcessor::currentParams() const
{
    float v[kNumModTargets];
    for (int t = 0; t < kNumModTargets; ++t) v[t] = apvts.getRawParameterValue (kModTargetIds[t])->load();
    return paramsFrom (v);
}

bass::Params BassProcessor::modulatedParams() const
{
    float v[kNumModTargets];
    for (int t = 0; t < kNumModTargets; ++t)
    {
        const bool modded = [&] { for (int l = 0; l < kNumLfos; ++l) if (hasModulation (t, l)) return true; return false; }();
        v[t] = modded ? modParams[(size_t) t]->convertFrom0to1 (modNorm[(size_t) t].load())
                      : apvts.getRawParameterValue (kModTargetIds[t])->load();
    }
    return paramsFrom (v);
}

// ===========================================================================
// Audio

void BassProcessor::handle (const MidiMessage& m)
{
    if (m.isNoteOn())
    {
        // envelopes retrigger on every note, so RETRIG / ENV LFOs restart with every note too
        sinceTrigger = 0.0;
        for (int l = 0; l < kNumLfos; ++l)
            if ((int) pLfoMode[(size_t) l]->load() >= 1) { lfoPhase[(size_t) l] = 0.0; lfoDone[(size_t) l] = false; }
        engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    }
    else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    else if (m.isPitchWheel()) engine.setPitchBend ((m.getPitchWheelValue() - 8192) / 8192.0 * 2.0);
}

void BassProcessor::renderRange (int from, int to)
{
    while (from < to)
    {
        const int len = jmin (kChunk, to - from);
        float v[kNumModTargets];
        computeModulated (v);
        engine.setParams (paramsFrom (v));
        engine.render (mono.get() + from, len);
        advanceLfos (from + len, len, v);
        from += len;
    }
}

void BassProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    if (n > monoSize) { mono.allocate ((size_t) n, true); monoSize = n; }

    hostBpm = 120.0; hostPpq = -1.0; hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())         hostBpm = jlimit (20.0, 999.0, *b);
            if (auto q = pos->getPpqPosition()) hostPpq = *q;
            hostPlaying = pos->getIsPlaying();
        }

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = jlimit (0, n, meta.samplePosition);
        if (at > pos) { renderRange (pos, at); pos = at; }
        handle (meta.getMessage());
    }
    if (pos < n) renderRange (pos, n);
    engine.publishViz();

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, mono.get(), n);
}

// ===========================================================================
// LFO shapes (from GRAIN FORM)

const StringArray& BassProcessor::divisionNames()
{
    static const StringArray s { "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                                 "1/2T", "1/4T", "1/8T", "1/16T", "1/2D", "1/4D", "1/8D" };
    return s;
}

double BassProcessor::divisionBeats (int i)
{
    static const double b[] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
                                4.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 3.0, 1.5, 0.75 };
    return b[jlimit (0, (int) std::size (b) - 1, i)];
}

float BassProcessor::segmentShape (float t, float curve)
{
    if (std::abs (curve) < 0.001f) return t;
    const float k = curve * 6.0f;
    return (std::exp (k * t) - 1.0f) / (std::exp (k) - 1.0f);
}

const StringArray& BassProcessor::lfoPresetNames()
{
    static const StringArray s { "HILL", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "PUMP" };
    return s;
}

std::array<LfoPoint, kLfoPoints> BassProcessor::lfoPreset (int i)
{
    switch (i)
    {
        case 1:  return { LfoPoint { 0, 0, 0 }, { 0.5f, 1, 0 }, { 0.5f, 1, 0 }, { 1, 0, 0 } };
        case 2:  return { LfoPoint { 0, 0, 0 }, { 0.333f, 0.333f, 0 }, { 0.667f, 0.667f, 0 }, { 1, 1, 0 } };
        case 3:  return { LfoPoint { 0, 1, 0 }, { 0.333f, 0.667f, 0 }, { 0.667f, 0.333f, 0 }, { 1, 0, 0 } };
        case 4:  return { LfoPoint { 0, 1, 0 }, { 0.5f, 1, 0 }, { 0.5f, 0, 0 }, { 1, 0, 0 } };
        case 5:  return { LfoPoint { 0, 0, -0.75f }, { 0.4f, 0.8f, -0.3f }, { 0.75f, 0.95f, 0 }, { 1, 1, 0 } };
        default: return { LfoPoint { 0, 0, -0.45f }, { 0.4f, 1, 0 }, { 0.6f, 1, 0.45f }, { 1, 0, 0 } };
    }
}

std::array<LfoPoint, kLfoPoints> BassProcessor::getLfoPoints (int l) const
{
    const ScopedLock sl (lfoLock);
    return lfoPoints[(size_t) l];
}

void BassProcessor::setLfoPoints (int l, std::array<LfoPoint, kLfoPoints> pts)
{
    pts[0].x = 0.0f;
    pts[kLfoPoints - 1].x = 1.0f;
    for (int i = 1; i < kLfoPoints - 1; ++i) pts[(size_t) i].x = jlimit (pts[(size_t) i - 1].x, 1.0f, pts[(size_t) i].x);
    for (int i = kLfoPoints - 2; i > 0; --i) pts[(size_t) i].x = jmin (pts[(size_t) i].x, pts[(size_t) i + 1].x);
    for (auto& p : pts) { p.y = jlimit (0.0f, 1.0f, p.y); p.curve = jlimit (-1.0f, 1.0f, p.curve); }
    {
        const ScopedLock sl (lfoLock);
        lfoPoints[(size_t) l] = pts;
    }
    rebuildLfoTable (l);
}

void BassProcessor::rebuildLfoTable (int l)
{
    const auto pts = getLfoPoints (l);
    const int next = 1 - lfoTableIndex[(size_t) l].load();
    auto& t = lfoTables[(size_t) l][(size_t) next];
    size_t seg = 0;
    for (int i = 0; i <= kLfoTable; ++i)
    {
        const float x = (float) i / (float) kLfoTable;
        while (seg + 2 < pts.size() && x > pts[seg + 1].x) ++seg;
        const auto& a = pts[seg];
        const auto& b = pts[seg + 1];
        t[(size_t) i] = (b.x - a.x < 1.0e-5f) ? b.y
                      : a.y + (b.y - a.y) * segmentShape (jlimit (0.0f, 1.0f, (x - a.x) / (b.x - a.x)), a.curve);
    }
    lfoTableIndex[(size_t) l].store (next, std::memory_order_release);
}

float BassProcessor::lfoShapeAt (int l, float phase) const
{
    const auto& t = lfoTables[(size_t) l][(size_t) lfoTableIndex[(size_t) l].load (std::memory_order_acquire)];
    const float p = jlimit (0.0f, 1.0f, phase) * (float) kLfoTable;
    const int i = jmin (kLfoTable - 1, (int) p);
    const float f = p - (float) i;
    return t[(size_t) i] + (t[(size_t) i + 1] - t[(size_t) i]) * f;
}

// ===========================================================================
// Modulation (from GRAIN FORM)

const char* BassProcessor::modTargetId (int t) { return (t >= 0 && t < kNumModTargets) ? kModTargetIds[t] : ""; }

int BassProcessor::modIndexOf (const String& id)
{
    for (int t = 0; t < kNumModTargets; ++t)
        if (id == kModTargetIds[t]) return t;
    return -1;
}

void BassProcessor::setModulation (int target, int lfo, float depth)
{
    if (target < 0 || target >= kNumModTargets || lfo < 0 || lfo >= kNumLfos) return;
    depth = jlimit (-1.0f, 1.0f, depth);
    for (auto& m : mods)
        if (m.target.load() == target && m.lfo.load() == lfo) { m.depth.store (depth); return; }
    for (auto& m : mods)
        if (m.target.load() < 0)
        {
            m.lfo.store (lfo);
            m.depth.store (depth);
            m.target.store (target, std::memory_order_release);
            return;
        }
}

void BassProcessor::removeModulation (int target, int lfo)
{
    for (auto& m : mods)
        if (m.target.load() == target && m.lfo.load() == lfo) m.target.store (-1);
}

void BassProcessor::clearModulations() { for (auto& m : mods) m.target.store (-1); }

bool BassProcessor::hasModulation (int target, int lfo) const
{
    for (auto& m : mods)
        if (m.target.load() == target && m.lfo.load() == lfo) return true;
    return false;
}

float BassProcessor::getModulationDepth (int target, int lfo) const
{
    for (auto& m : mods)
        if (m.target.load() == target && m.lfo.load() == lfo) return m.depth.load();
    return 0.0f;
}

int BassProcessor::countModulations (int lfo) const
{
    int n = 0;
    for (auto& m : mods)
        if (m.target.load() >= 0 && m.lfo.load() == lfo) ++n;
    return n;
}

void BassProcessor::computeModulated (float* v)
{
    float off[kNumModTargets] = {};
    for (auto& m : mods)
    {
        const int t = m.target.load (std::memory_order_acquire);
        if (t < 0 || t >= kNumModTargets) continue;
        const int l = jlimit (0, kNumLfos - 1, m.lfo.load (std::memory_order_relaxed));
        if (pLfoOn[(size_t) l]->load() < 0.5f) continue;
        off[t] += m.depth.load (std::memory_order_relaxed) * lfoValue[(size_t) l];
    }
    for (int t = 0; t < kNumModTargets; ++t)
    {
        auto* p = modParams[(size_t) t];
        const float norm = jlimit (0.0f, 1.0f, p->getValue() + off[t]);
        v[t] = p->convertFrom0to1 (norm);
        modNorm[(size_t) t].store (norm, std::memory_order_relaxed);
    }
}

void BassProcessor::advanceLfos (int endSample, int len, const float* v)
{
    const double sr = getSampleRate() > 0 ? getSampleRate() : 44100.0;
    sinceTrigger += (double) len / sr;
    for (int l = 0; l < kNumLfos; ++l)
    {
        const auto L = (size_t) l;
        const bool sync = pLfoSync[L]->load() > 0.5f;
        const int  mode = (int) pLfoMode[L]->load();
        const double beats = divisionBeats ((int) pLfoDiv[L]->load());
        const double rate = sync ? (hostBpm / 60.0) / beats : (double) v[mtLfo1Rate + l];

        double ph;
        if (mode == 2)
        {
            ph = lfoDone[L] ? 1.0 : lfoPhase[L] + rate * (double) len / sr;
            if (ph >= 1.0) { ph = 1.0; lfoDone[L] = true; }
        }
        else
        {
            if (sync && mode == 0 && hostPlaying && hostPpq >= 0.0)
                ph = (hostPpq + (double) endSample / sr * hostBpm / 60.0) / beats;
            else
                ph = lfoPhase[L] + rate * (double) len / sr;
            ph -= std::floor (ph);
        }
        lfoPhase[L] = ph;

        const float rise = pLfoRise[L]->load();
        const float riseGain = rise > 0.01f ? (float) jmin (1.0, sinceTrigger / (double) rise) : 1.0f;
        lfoValue[L] = lfoShapeAt (l, (float) ph) * riseGain;
        lfoValueOut[L].store (lfoValue[L], std::memory_order_relaxed);
        lfoPhaseOut[L].store ((float) ph, std::memory_order_relaxed);
        lfoRiseOut[L].store (riseGain, std::memory_order_relaxed);
    }
}

// ===========================================================================
// Presets

static void setParam (AudioProcessorValueTreeState& s, const String& id, float v)
{
    if (auto* p = s.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

const String BassProcessor::getProgramName (int i)
{
    const auto& l = bass::factoryPresets();
    return isPositiveAndBelow (i, (int) l.size()) ? String (l[(size_t) i].name) : String();
}

void BassProcessor::loadFactory (int index)
{
    const auto& l = bass::factoryPresets();
    index = (index % (int) l.size() + (int) l.size()) % (int) l.size();
    const auto& p = l[(size_t) index].p;
    setParam (apvts, ParamID::click, p.click);   setParam (apvts, ParamID::punch, p.punch);
    setParam (apvts, ParamID::drop, p.drop);     setParam (apvts, ParamID::power, p.power);
    setParam (apvts, ParamID::length, p.length); setParam (apvts, ParamID::drive, p.drive);
    setParam (apvts, ParamID::glide, 0.0f);      setParam (apvts, ParamID::output, p.output);
    setParam (apvts, ParamID::fm, p.fm);         setParam (apvts, ParamID::noise, p.noise);
    setParam (apvts, ParamID::noiseTone, p.noiseTone); setParam (apvts, ParamID::noiseTail, p.noiseTail);
    setParam (apvts, ParamID::noiseType, (float) p.noiseType);
    setParam (apvts, ParamID::distType, (float) p.distType);
    clearModulations();
    for (int lf = 0; lf < kNumLfos; ++lf)
    {
        setParam (apvts, ParamID::lfo (lf, "Rate"), 1.0f); setParam (apvts, ParamID::lfo (lf, "Sync"), 1.0f);
        setParam (apvts, ParamID::lfo (lf, "Div"), 4.0f);  setParam (apvts, ParamID::lfo (lf, "Mode"), 1.0f);
        setParam (apvts, ParamID::lfo (lf, "Rise"), 0.0f); setParam (apvts, ParamID::lfo (lf, "On"), 1.0f);
        setLfoPoints (lf, lfoPreset (1));
    }
    presetName = l[(size_t) index].name;
}

File BassProcessor::presetFolder()
{
    auto f = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("808 BASS MAKER").getChildFile ("Presets");
    f.createDirectory();
    return f;
}

Array<File> BassProcessor::listUserPresets() const
{
    auto files = presetFolder().findChildFiles (File::findFiles, false, "*.808preset");
    files.sort();
    return files;
}

StringArray BassProcessor::allPresetNames() const
{
    StringArray s;
    for (auto& p : bass::factoryPresets()) s.add (p.name);
    for (auto& f : listUserPresets()) s.add (f.getFileNameWithoutExtension());
    return s;
}

int BassProcessor::currentPresetIndex() const { return allPresetNames().indexOf (presetName); }

void BassProcessor::loadPresetByIndex (int index)
{
    const int nf = (int) bass::factoryPresets().size();
    if (index < nf) { loadFactory (index); return; }
    const auto user = listUserPresets();
    if (isPositiveAndBelow (index - nf, user.size())) loadPreset (user[index - nf]);
}

bool BassProcessor::savePreset (const String& rawName)
{
    const auto name = File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;
    MemoryBlock mb;
    presetName = name;
    getStateInformation (mb);
    if (auto xml = getXmlFromBinary (mb.getData(), (int) mb.getSize()))
        return xml->writeTo (presetFolder().getChildFile (name + ".808preset"));
    return false;
}

bool BassProcessor::loadPreset (const File& f)
{
    auto xml = parseXML (f);
    if (xml == nullptr) return false;
    MemoryBlock mb;
    copyXmlToBinary (*xml, mb);
    setStateInformation (mb.getData(), (int) mb.getSize());
    presetName = f.getFileNameWithoutExtension();
    return true;
}

void BassProcessor::randomTargets (Random& r, std::vector<std::pair<const char*, float>>& out)
{
    // ranges chosen so every roll is a usable 808: clean sub is always there, tails never too short
    out.clear();
    out.push_back ({ ParamID::click,  0.25f + 0.75f * r.nextFloat() });
    out.push_back ({ ParamID::punch,  0.15f + 0.75f * r.nextFloat() });
    out.push_back ({ ParamID::drop,   0.60f * r.nextFloat() * r.nextFloat() });
    out.push_back ({ ParamID::power,  0.45f + 0.50f * r.nextFloat() });
    out.push_back ({ ParamID::length, 0.40f + 0.60f * r.nextFloat() });
    out.push_back ({ ParamID::drive,  r.nextFloat() < 0.2f ? 0.05f * r.nextFloat() : 0.15f + 0.75f * r.nextFloat() });
    out.push_back ({ ParamID::fm,     r.nextFloat() < 0.5f ? 0.0f : 0.1f + 0.6f * r.nextFloat() });
    out.push_back ({ ParamID::noise,  r.nextFloat() < 0.4f ? 0.0f : 0.15f + 0.6f * r.nextFloat() });
    out.push_back ({ ParamID::noiseTone, 0.25f + 0.6f * r.nextFloat() });
    out.push_back ({ ParamID::noiseTail, 0.1f + 0.8f * r.nextFloat() });
    out.push_back ({ ParamID::noiseType, (float) r.nextInt (bass::kNumNoises) });
    out.push_back ({ ParamID::distType, r.nextFloat() < 0.5f ? 0.0f : (float) r.nextInt (bass::kNumDists) });
}

// ===========================================================================
// State

void BassProcessor::getStateInformation (MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("presetName", presetName, nullptr);

    StringArray ms;
    for (auto& m : mods)
        if (const int t = m.target.load(); t >= 0)
            ms.add (String (kModTargetIds[t]) + ":" + String (m.lfo.load()) + ":" + String (m.depth.load(), 4));
    st.setProperty ("mods", ms.joinIntoString (";"), nullptr);
    for (int l = 0; l < kNumLfos; ++l)
    {
        StringArray ps;
        for (auto& p : getLfoPoints (l)) ps.add (String (p.x, 4) + "," + String (p.y, 4) + "," + String (p.curve, 3));
        st.setProperty ("lfoShape" + String (l + 1), ps.joinIntoString (";"), nullptr);
    }
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void BassProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto st = ValueTree::fromXml (*xml);
    const String pn = st.getProperty ("presetName", "THEMATIC 808").toString();
    const auto ms = StringArray::fromTokens (st.getProperty ("mods").toString(), ";", "");
    std::array<String, kNumLfos> shapes;
    for (int l = 0; l < kNumLfos; ++l) shapes[(size_t) l] = st.getProperty ("lfoShape" + String (l + 1)).toString();
    apvts.replaceState (st);
    presetName = pn;

    clearModulations();
    for (auto& entry : ms)
    {
        const auto parts = StringArray::fromTokens (entry, ":", "");
        if (parts.size() == 3) setModulation (modIndexOf (parts[0]), parts[1].getIntValue(), parts[2].getFloatValue());
    }
    for (int l = 0; l < kNumLfos; ++l)
    {
        const auto items = StringArray::fromTokens (shapes[(size_t) l], ";", "");
        if (items.size() != kLfoPoints) { setLfoPoints (l, lfoPreset (1)); continue; }
        std::array<LfoPoint, kLfoPoints> pts {};
        for (int i = 0; i < kLfoPoints; ++i)
        {
            const auto vals = StringArray::fromTokens (items[i], ",", "");
            if (vals.size() == 3) pts[(size_t) i] = { vals[0].getFloatValue(), vals[1].getFloatValue(), vals[2].getFloatValue() };
        }
        setLfoPoints (l, pts);
    }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BassProcessor(); }
