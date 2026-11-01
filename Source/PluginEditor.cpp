#include "PluginEditor.h"

using namespace juce;
namespace C = Colours808;

static Font font (float h, float kerning = 0.0f, bool bold = false)
{
    return Font (FontOptions (h, bold ? Font::bold : Font::plain)).withExtraKerningFactor (kerning);
}

// ================================================================ look
FlatLook::FlatLook()
{
    setColour (PopupMenu::backgroundColourId, C::button);
    setColour (PopupMenu::textColourId, C::text);
    setColour (PopupMenu::highlightedBackgroundColourId, C::accent);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
}

void FlatLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const bool big = s.getProperties()["big"];
    const float size = (float) jmin (w, h);
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float sw = big ? 6.0f : 4.0f;
    const float r = size * 0.5f - sw * 0.5f - 1.0f;
    const float ang = start + pos * (end - start);

    Path trackP, valP;
    trackP.addCentredArc (cx, cy, r, r, 0, start, end, true);
    g.setColour (C::track);
    g.strokePath (trackP, PathStrokeType (sw, PathStrokeType::curved, PathStrokeType::rounded));
    if (pos > 0.002f)
    {
        valP.addCentredArc (cx, cy, r, r, 0, start, ang, true);
        g.setColour (C::accent);
        g.strokePath (valP, PathStrokeType (sw, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const float ir = r - (big ? 13.0f : 9.0f);
    g.setColour (C::knob);
    g.fillEllipse (cx - ir, cy - ir, ir * 2, ir * 2);
    g.setColour (C::knobRim);
    g.drawEllipse (cx - ir, cy - ir, ir * 2, ir * 2, 1.0f);

    const float a = ang - MathConstants<float>::halfPi;
    const float r0 = ir * 0.32f, r1 = ir - (big ? 9.0f : 6.0f);
    g.setColour (C::title);
    g.drawLine (cx + std::cos (a) * r0, cy + std::sin (a) * r0, cx + std::cos (a) * r1, cy + std::sin (a) * r1, big ? 3.5f : 2.2f);
}

void FlatLook::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    auto c = b.getProperties()["accent"] ? C::accent : C::button;
    if (over) c = c.brighter (0.12f);
    if (down) c = c.darker (0.1f);
    g.setColour (c);
    g.fillRoundedRectangle (b.getLocalBounds().toFloat(), 6.0f);
}

void FlatLook::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setColour (b.getProperties()["accent"] ? Colours::white : C::text);
    g.setFont (font (12.0f, 0.08f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred, false);
}

// ================================================================ knob
Knob::Knob (BassProcessor& p, const char* paramId, const String& n, bool isBig) : name (n), big (isBig)
{
    slider.setSliderStyle (Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (big ? 260 : 200);
    slider.setVelocityBasedMode (false);
    slider.getProperties().set ("big", big);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramId, slider);
    if (auto* prm = p.apvts.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
}

void Knob::resized()
{
    const int k = big ? 112 : 62;
    slider.setBounds ((getWidth() - k) / 2, 0, k, k);
}

void Knob::paint (Graphics& g)
{
    const int k = slider.getBottom();
    g.setColour (big ? C::title : C::label);
    g.setFont (font (big ? 14.0f : 11.5f, big ? 0.22f : 0.12f));
    g.drawText (name, 0, k + 4, getWidth(), 18, Justification::centred, false);
    g.setColour (C::faint);
    g.setFont (font (11.0f, 0.04f));
    g.drawText (String (roundToInt (slider.getValue() * 100.0)) + "%", 0, k + 21, getWidth(), 16, Justification::centred, false);
}

// ================================================================ meter
void Meter::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (C::button);
    g.fillRoundedRectangle (b, 3.0f);
    const float h = jlimit (0.0f, 1.0f, value) * b.getHeight();
    g.setColour (value > 0.97f ? C::accent : C::air);
    g.fillRoundedRectangle (b.withTop (b.getBottom() - h), 3.0f);
}

// ================================================================ wave screen
WaveScreen::WaveScreen (BassProcessor& p) : proc (p)
{
    dust.reserve (3000);
    lastHits = proc.engine.vizHits.load();
    rebuildShape();
}

float WaveScreen::xAt (float u) const { return 14.0f + u * ((float) getWidth() - 28.0f); }
float WaveScreen::yAt (int i, float scale) const
{
    const float m = getHeight() * 0.5f, amp = (m - 22.0f) * (0.58f + 0.36f * power);
    return m - wave[(size_t) i] * amp * scale;
}

void WaveScreen::rebuildShape()
{
    const auto p = proc.currentParams();
    power = p.power;
    windowSec = bass::tailSeconds (p) * 1.08 + 0.02;

    // the screen uses a stretched time axis (t = u^2) so CLICK and PUNCH are visible, and draws
    // the pitch "softened" so the sine stays readable; shapes come from the same functions as the DSP
    std::array<double, NS> vf {};
    double total = 0;
    for (int i = 0; i < NS; ++i)
    {
        const double u = i / (double) (NS - 1), t = windowSec * u * u;
        vf[(size_t) i] = std::pow (2.0, bass::pitchShape (p, t) / 12.0 * 0.5);
        total += vf[(size_t) i];
    }
    const double k = 2.0 * bass::kPi * 11.5 / total;
    const double q = 1.0 / (1.0 + p.drive * 2.2 * (0.6 + 0.4 * p.power));
    double ph = 0;
    for (int i = 0; i < NS; ++i)
    {
        const double u = i / (double) (NS - 1), t = windowSec * u * u;
        ph += vf[(size_t) i] * k;
        const double s = std::sin (ph);
        const double a = bass::ampShape (p, t);
        wave[(size_t) i] = (float) ((s < 0 ? -1.0 : 1.0) * std::pow (std::abs (s), q) * a);
        env[(size_t) i] = (float) a;
    }
}

void WaveScreen::burst (float strength)
{
    const float W = (float) getWidth(), H = (float) getHeight(), m = H * 0.5f, x0 = xAt (0);
    const int n = (int) ((240.0f + 560.0f * power) * (0.45f + 0.55f * strength));
    for (int i = 0; i < n; ++i)
    {
        Dust d;
        d.z = rnd.nextFloat();
        d.life = 0.8f + rnd.nextFloat() * 1.3f;
        if (rnd.nextFloat() < 0.6f)
        {
            // blast from the attack point, fanned out over the whole screen
            const float spread = rnd.nextFloat() < 0.85f ? 1.25f : 2.4f;
            const float ang = (rnd.nextFloat() - 0.5f) * MathConstants<float>::pi * spread;
            const float sp = (160.0f + std::pow (rnd.nextFloat(), 0.55f) * (900.0f + 1900.0f * power)) * (0.6f + 0.4f * strength);
            d.x = x0 + rnd.nextFloat() * 6.0f;
            d.y = m + (rnd.nextFloat() - 0.5f) * 20.0f;
            d.vx = std::cos (ang) * sp;
            d.vy = std::sin (ang) * sp * 0.75f;
        }
        else
        {
            // the whole wave exhales: dust leaves the wave everywhere, outwards
            const int idx = rnd.nextInt (NS);
            const float s = wave[(size_t) idx] >= 0 ? -1.0f : 1.0f;
            d.x = xAt (idx / (float) (NS - 1));
            d.y = yAt (idx, 1.0f);
            d.vx = (rnd.nextFloat() - 0.3f) * 380.0f * (0.5f + power);
            d.vy = s * (120.0f + rnd.nextFloat() * 700.0f) * (0.45f + 0.75f * power) * (0.3f + std::abs (wave[(size_t) idx]));
        }
        dust.push_back (d);
    }
    if (dust.size() > 2800) dust.erase (dust.begin(), dust.begin() + (long) (dust.size() - 2800));
    rings.push_back ({ 0.0f, power * (0.5f + 0.5f * strength) });
    if (rings.size() > 6) rings.erase (rings.begin());
    flash = 1.0f;
    ignoreUnused (W);
}

void WaveScreen::tick (float dt)
{
    if (getWidth() <= 0) return;
    rebuildShape();
    phaseT += dt;

    auto& e = proc.engine;
    const int hits = e.vizHits.load();
    if (hits != lastHits) { lastHits = hits; burst (e.vizVelocity.load()); }

    const float target = jlimit (0.0f, 1.2f, e.vizLevel.load());
    level = jmax (target, level * std::exp (-dt * 5.0f));

    const double nt = e.vizNoteTime.load();
    playU = nt < windowSec ? (float) std::sqrt (nt / windowSec) : 1.0f;

    // continuous air: depends on what the bass is playing right now
    const auto p = proc.currentParams();
    emitAcc += dt * std::pow (level, 1.3f) * (90.0f + 420.0f * power) * (0.6f + p.drive);
    const float H = (float) getHeight(), W = (float) getWidth();
    while (emitAcc >= 1.0f)
    {
        emitAcc -= 1.0f;
        const int lim = jmax (2, (int) (playU * NS));
        const int idx = rnd.nextInt (lim);
        if (rnd.nextFloat() > std::abs (wave[(size_t) idx]) + 0.04f) continue;
        Dust d;
        const float s = wave[(size_t) idx] >= 0 ? -1.0f : 1.0f;
        d.z = rnd.nextFloat();
        d.life = 0.6f + rnd.nextFloat() * 0.9f;
        d.x = xAt (idx / (float) (NS - 1));
        d.y = yAt (idx, 1.0f);
        d.vx = (rnd.nextFloat() - 0.2f) * 160.0f * (0.4f + level);
        d.vy = s * (60.0f + rnd.nextFloat() * 380.0f) * (0.4f + power) * (0.3f + level);
        dust.push_back (d);
    }

    const float drag = std::exp (-dt * 3.2f);
    for (auto& d : dust)
    {
        d.vx *= drag;
        d.vy = d.vy * drag + 22.0f * dt;
        d.x += d.vx * dt;
        d.y += d.vy * dt;
        d.life -= dt * (0.55f + 0.45f * (1.0f - d.z));
    }
    dust.erase (std::remove_if (dust.begin(), dust.end(), [W, H] (const Dust& d)
                { return d.life <= 0 || d.x < -20 || d.x > W + 20 || d.y < -20 || d.y > H + 20; }), dust.end());
    if (dust.size() > 2800) dust.erase (dust.begin(), dust.begin() + (long) (dust.size() - 2800));

    for (auto& r : rings) r.t += dt;
    rings.erase (std::remove_if (rings.begin(), rings.end(), [] (const Ring& r) { return r.t > 1.1f; }), rings.end());
    flash = jmax (0.0f, flash - dt / 0.18f);
}

void WaveScreen::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float W = b.getWidth(), H = b.getHeight(), m = H * 0.5f, amp = (m - 22.0f) * (0.58f + 0.36f * power);
    g.setColour (C::screen);
    g.fillRoundedRectangle (b, 10.0f);
    g.reduceClipRegion (getLocalBounds());

    // envelope silhouette
    {
        Path fill;
        fill.startNewSubPath (xAt (0), m);
        for (int i = 0; i < NS; i += 2) fill.lineTo (xAt (i / (float) (NS - 1)), m - env[(size_t) i] * amp);
        for (int i = NS - 1; i >= 0; i -= 2) fill.lineTo (xAt (i / (float) (NS - 1)), m + env[(size_t) i] * amp);
        fill.closeSubPath();
        g.setColour (C::air.withAlpha (0.035f));
        g.fillPath (fill);
    }
    g.setColour (Colour (0xff222222));
    g.drawHorizontalLine ((int) m, xAt (0), xAt (1));

    auto wavePath = [this] (float scale, int lim, float wobble)
    {
        Path pth;
        for (int i = 0; i < lim; i += 2)
        {
            const float x = xAt (i / (float) (NS - 1));
            const float y = yAt (i, scale) + (wobble > 0 ? std::sin (i * 0.05f + phaseT * 2.0f) * wobble : 0.0f);
            if (i == 0) pth.startNewSubPath (x, y); else pth.lineTo (x, y);
        }
        return pth;
    };

    // air contours breathing out of the wave
    for (int j = 0; j < 4; ++j)
    {
        const float gph = std::fmod (phaseT * (0.45f + level * 0.9f) + j / 4.0f, 1.0f);
        const float a = (1.0f - gph) * (1.0f - gph) * (0.03f + 0.16f * level * (0.4f + power));
        if (a < 0.004f) continue;
        g.setColour (C::air.withAlpha (a));
        g.strokePath (wavePath (1.0f + gph * (0.4f + 1.6f * power * (0.3f + level)), NS, 5.0f * gph), PathStrokeType (1.0f));
    }
    // fine parallel lines
    for (int j = 3; j >= 1; --j)
    {
        g.setColour (C::air.withAlpha (0.05f + 0.02f * (3 - j) + 0.08f * level));
        g.strokePath (wavePath (1.0f + j * (0.06f + 0.12f * level * power), NS, 0.0f), PathStrokeType (1.0f));
    }

    // pressure rings from the attack
    for (const auto& r : rings)
    {
        const float rad = 10.0f + r.t * (W * (0.6f + 0.7f * r.power));
        const float a = jmax (0.0f, 1.0f - r.t / 1.1f);
        Path arc;
        arc.addCentredArc (xAt (0), m, rad, rad * 0.55f, 0, 0.0f, MathConstants<float>::pi, true);
        g.setColour (C::air.withAlpha (a * a * 0.45f));
        g.strokePath (arc, PathStrokeType (1.0f + 3.0f * r.power * a));
    }

    // dust
    for (const auto& d : dust)
    {
        const float a = jmin (1.0f, d.life) * (0.22f + 0.62f * d.z);
        g.setColour (C::air.withAlpha (a));
        const float sp = std::abs (d.vx) + std::abs (d.vy);
        if (sp > 140.0f)
            g.drawLine (d.x - d.vx * 0.012f, d.y - d.vy * 0.012f, d.x, d.y, 0.6f + d.z * 1.1f);
        else
        {
            const float s = 0.8f + d.z * 1.5f;
            g.fillRect (d.x, d.y, s, s);
        }
    }

    if (flash > 0)
    {
        const float r = 6.0f + 18.0f * power * flash + (1.0f - flash) * 12.0f;
        g.setColour (Colours::white.withAlpha (flash * 0.6f));
        g.fillEllipse (xAt (0) - r, m - r, r * 2, r * 2);
    }

    // the wave itself
    const float thick = 1.8f + 1.4f * power;
    const PathStrokeType st (thick, PathStrokeType::curved, PathStrokeType::rounded);
    g.setColour (Colour (0xff3a3a38));
    g.strokePath (wavePath (1.0f, NS, 0.0f), st);
    const int lim = jlimit (2, NS, (int) (playU * NS));
    g.setColour (C::air);
    g.strokePath (wavePath (1.0f, lim, 0.0f), st);

    if (playU < 1.0f)
    {
        g.setColour (C::accent);
        g.fillRect (xAt (playU) - 1.0f, 10.0f, 2.0f, H - 20.0f);
    }
}

void WaveScreen::mouseDown (const MouseEvent&) { previewHeld = true; proc.keyboardState.noteOn (1, 36, 0.95f); }
void WaveScreen::mouseUp (const MouseEvent&)
{
    if (previewHeld) proc.keyboardState.noteOff (1, 36, 0.0f);
    previewHeld = false;
}

// ================================================================ editor
BassEditor::BassEditor (BassProcessor& p)
    : AudioProcessorEditor (p), proc (p), screen (p),
      click (p, ParamID::click, "CLICK"), punch (p, ParamID::punch, "PUNCH"), drop (p, ParamID::drop, "DROP"),
      power (p, ParamID::power, "POWER", true), length (p, ParamID::length, "LENGTH"),
      drive (p, ParamID::drive, "DRIVE"), glide (p, ParamID::glide, "GLIDE")
{
    setLookAndFeel (&look);
    for (auto* c : std::initializer_list<Component*> { &screen, &subMeter, &outMeter, &click, &punch, &drop, &power, &length, &drive, &glide,
                                                       &prev, &next, &presetBox, &dice })
        addAndMakeVisible (c);

    dice.getProperties().set ("accent", true);
    presetBox.setButtonText (proc.presetName);
    prev.onClick = [this] { stepPreset (-1); };
    next.onClick = [this] { stepPreset (+1); };
    dice.onClick = [this] { rollDice(); };
    presetBox.onClick = [this]
    {
        PopupMenu m;
        const auto& l = bass::factoryPresets();
        for (int i = 0; i < (int) l.size(); ++i) m.addItem (i + 1, l[(size_t) i].name, true, i == proc.presetIndex);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (presetBox), [this] (int r)
        {
            if (r > 0) { proc.presetIndex = r - 2; stepPreset (+1); }
        });
    };

    setSize (900, 570);
    startTimerHz (60);
}

BassEditor::~BassEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BassEditor::stepPreset (int delta)
{
    const auto& l = bass::factoryPresets();
    const int n = (int) l.size();
    const int idx = ((jmax (-1, proc.presetIndex) + delta) % n + n) % n;
    const auto& t = l[(size_t) idx].p;
    anim = { { ParamID::click, 0, t.click }, { ParamID::punch, 0, t.punch }, { ParamID::drop, 0, t.drop },
             { ParamID::power, 0, t.power }, { ParamID::length, 0, t.length }, { ParamID::drive, 0, t.drive },
             { ParamID::glide, 0, t.glide }, { ParamID::output, 0, t.output } };
    for (auto& a : anim) a.from = proc.apvts.getRawParameterValue (a.id)->load();
    for (auto& a : anim) if (auto* prm = proc.apvts.getParameter (a.id)) prm->beginChangeGesture();
    animT = 0.0f;
    proc.presetIndex = idx;
    proc.presetName = l[(size_t) idx].name;
}

void BassEditor::rollDice()
{
    const auto t = BassProcessor::diceParams (rnd);
    anim = { { ParamID::click, 0, t.click }, { ParamID::punch, 0, t.punch }, { ParamID::drop, 0, t.drop },
             { ParamID::power, 0, t.power }, { ParamID::length, 0, t.length }, { ParamID::drive, 0, t.drive },
             { ParamID::glide, 0, t.glide } };
    for (auto& a : anim) a.from = proc.apvts.getRawParameterValue (a.id)->load();
    for (auto& a : anim) if (auto* prm = proc.apvts.getParameter (a.id)) prm->beginChangeGesture();
    animT = 0.0f;
    proc.presetIndex = -1;
    proc.presetName = "DICE #" + String (100 + rnd.nextInt (900));
}

void BassEditor::tick (float dt)
{
    if (animT < 1.0f)
    {
        animT = jmin (1.0f, animT + dt / 0.35f);
        const float k = 1.0f - std::pow (1.0f - animT, 3.0f);
        for (auto& a : anim)
            if (auto* prm = proc.apvts.getParameter (a.id))
            {
                prm->setValueNotifyingHost (prm->convertTo0to1 (a.from + (a.to - a.from) * k));
                if (animT >= 1.0f) prm->endChangeGesture();
            }
    }
    if (presetBox.getButtonText() != proc.presetName) presetBox.setButtonText (proc.presetName);

    screen.tick (dt);
    screen.repaint();
    subMeter.set (proc.engine.vizSub.load() * 0.8f);
    outMeter.set (proc.engine.vizLevel.load());
}

void BassEditor::paint (Graphics& g)
{
    g.fillAll (C::bg);

    g.setColour (C::dim);
    g.setFont (font (11.0f, 0.14f));
    g.drawText ("v0.1", 24, 18, 60, 14, Justification::left, false);

    g.setColour (C::title);
    g.setFont (font (31.0f, 0.06f, true));
    g.drawText ("808 BASS MAKER", 0, 12, getWidth(), 38, Justification::centred, false);
    g.setColour (C::dim);
    g.setFont (font (11.0f, 0.24f));
    g.drawText ("HEAVY 808 DESIGNER", 0, 50, getWidth(), 16, Justification::centred, false);

    g.setFont (font (10.5f, 0.16f));
    g.drawText ("SUB", subMeter.getX() - 14, subMeter.getBottom() + 6, subMeter.getWidth() + 28, 14, Justification::centred, false);
    g.drawText ("OUT", outMeter.getX() - 14, outMeter.getBottom() + 6, outMeter.getWidth() + 28, 14, Justification::centred, false);

    // free space for the next modules
    const int top = 462, h = 84, gap = 12, x0 = 30, w = (getWidth() - 2 * x0 - 2 * gap) / 3;
    for (int i = 0; i < 3; ++i)
    {
        const Rectangle<float> r ((float) (x0 + i * (w + gap)), (float) top, (float) w, (float) h);
        Path p; p.addRoundedRectangle (r, 10.0f);
        Path dashed;
        const float dl[] = { 4.0f, 4.0f };
        PathStrokeType (1.0f).createDashedStroke (dashed, p, dl, 2);
        g.setColour (Colour (0xff2c2c2c));
        g.fillPath (dashed);
        g.setColour (Colour (0xff4a4a48));
        g.setFont (font (11.0f, 0.18f));
        g.drawText ("MODULE", r, Justification::centred, false);
    }
}

void BassEditor::resized()
{
    const int W = getWidth();
    dice.setBounds (W - 24 - 64, 18, 64, 28);
    next.setBounds (dice.getX() - 6 - 28, 18, 28, 28);
    presetBox.setBounds (next.getX() - 6 - 128, 18, 128, 28);
    prev.setBounds (presetBox.getX() - 6 - 28, 18, 28, 28);

    subMeter.setBounds (28, 104, 6, 166);
    outMeter.setBounds (W - 34, 104, 6, 166);
    screen.setBounds (52, 84, W - 104, 196);

    const int top = 300, x0 = 30;
    const float unit = (W - 2 * x0) / 7.5f;
    Knob* ks[] = { &click, &punch, &drop, &power, &length, &drive, &glide };
    float x = (float) x0;
    for (auto* k : ks)
    {
        const float w = k == &power ? unit * 1.5f : unit;
        const int h = k == &power ? 150 : 104;
        const int y = k == &power ? top : top + 22;
        k->setBounds ((int) x, y, (int) w, h);
        x += w;
    }
}

juce::AudioProcessorEditor* BassProcessor::createEditor() { return new BassEditor (*this); }
