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

    // a heavy preset
    p.loadPreset (2);
    for (int i = 0; i < 30; ++i) frame (p, ed);
    p.keyboardState.noteOn (1, 33, 1.0f);
    for (int i = 0; i < 9; ++i) frame (p, ed);
    save (ed, outDir, "ui-gut.png");
    p.keyboardState.noteOff (1, 33, 0.0f);

    base.reset();
    return 0;
}
