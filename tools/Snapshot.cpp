// Renders the editor to PNGs while a note plays (CI previews + local design checks).
// Usage: Snapshot <outDir> [frames]   (frames mode also writes an animation strip f000.png...)
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>

using namespace juce;

static void save (Component& c, const File& dir, const String& name)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    PNGImageFormat png;
    auto f = dir.getChildFile (name);
    f.deleteFile();
    FileOutputStream os (f);
    png.writeImageToStream (img, os);
    std::cout << "wrote " << f.getFullPathName() << std::endl;
}

// advance audio and UI together by one 60 fps frame
static void frame (BassProcessor& p, BassEditor& ed)
{
    AudioBuffer<float> buf (2, 800);
    MidiBuffer midi;
    buf.clear();
    p.processBlock (buf, midi);
    ed.tick (1.0f / 60.0f);
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? String (argv[1]) : File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();
    const bool frames = argc > 2 && String (argv[2]) == "frames";

    BassProcessor p;
    p.setPlayConfigDetails (0, 2, 48000.0, 800);
    p.prepareToPlay (48000.0, 800);
    std::unique_ptr<AudioProcessorEditor> base (p.createEditor());
    auto& ed = *dynamic_cast<BassEditor*> (base.get());

    for (int i = 0; i < 10; ++i) frame (p, ed);
    save (ed, outDir, "ui-idle.png");

    // hit: capture the blast, then the tail
    p.keyboardState.noteOn (1, 36, 1.0f);
    for (int i = 0; i < 70; ++i)
    {
        frame (p, ed);
        if (i == 7) save (ed, outDir, "ui-impact.png");
        if (i == 40) save (ed, outDir, "ui-tail.png");
        if (frames && i % 2 == 0) save (ed, outDir, "f" + String (i).paddedLeft ('0', 3) + ".png");
    }
    p.keyboardState.noteOff (1, 36, 0.0f);

    // NOISE on: sand wave + sand grains
    auto setP = [&p] (const char* id, float v) { if (auto* x = p.apvts.getParameter (id)) x->setValueNotifyingHost (x->convertTo0to1 (v)); };
    setP (ParamID::noise, 0.8f); setP (ParamID::noiseType, 3.0f); setP (ParamID::noiseTail, 0.7f);
    for (int i = 0; i < 30; ++i) frame (p, ed);
    p.keyboardState.noteOn (1, 36, 1.0f);
    for (int i = 0; i < 12; ++i) frame (p, ed);
    save (ed, outDir, "ui-sand.png");
    p.keyboardState.noteOff (1, 36, 0.0f);

    // a heavy preset with noise crunch, FM and an LFO on DRIVE and FM
    p.loadFactory (7);   // CRUNCH
    if (auto* d = p.apvts.getParameter (ParamID::distType)) d->setValueNotifyingHost (d->convertTo0to1 (3.0f));   // FOLD
    if (auto* d = p.apvts.getParameter (ParamID::drive)) d->setValueNotifyingHost (0.85f);
    p.setModulation (mtDrive, 0, 0.35f);
    p.setModulation (mtFm, 1, -0.3f);
    p.setModulation (mtNoiseTone, 0, 0.25f);
    ed.selectLfo (0);
    ed.refreshAllMods();
    for (int i = 0; i < 30; ++i) frame (p, ed);
    p.keyboardState.noteOn (1, 33, 1.0f);
    for (int i = 0; i < 9; ++i) frame (p, ed);
    save (ed, outDir, "ui-gut.png");
    p.keyboardState.noteOff (1, 33, 0.0f);

    // ADSR amp switched on and selected, a held note in its sustain; PITCH +5
    p.loadFactory (0);
    p.clearModulations();
    ed.refreshAllMods();
    if (auto* x = p.apvts.getParameter (ParamID::ampOn)) x->setValueNotifyingHost (1.0f);
    if (auto* x = p.apvts.getParameter (ParamID::pitch)) x->setValueNotifyingHost (x->convertTo0to1 (5.0f));
    ed.selectLfo (3);
    for (int i = 0; i < 20; ++i) frame (p, ed);
    p.keyboardState.noteOn (1, 60, 1.0f);
    for (int i = 0; i < 40; ++i) frame (p, ed);
    save (ed, outDir, "ui-adsr.png");
    p.keyboardState.noteOff (1, 60, 0.0f);

    base.reset();
    return 0;
}
