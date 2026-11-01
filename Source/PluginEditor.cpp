#include "PluginEditor.h"

using namespace juce;
namespace C = Colours808;

static Font font (float h, float kerning = 0.0f, bool bold = false)
{
    return Font (FontOptions (h, bold ? Font::bold : Font::plain)).withExtraKerningFactor (kerning);
}

static Path arcPath (Point<float> c, float r, float a0, float a1)
{
    Path p;
    p.addCentredArc (c.x, c.y, r, r, 0.0f, jmin (a0, a1), jmax (a0, a1), true);
    return p;
}

// ================================================================ look
FlatLook::FlatLook()
{
    setColour (PopupMenu::backgroundColourId, C::button);
    setColour (PopupMenu::textColourId, C::text);
    setColour (PopupMenu::headerTextColourId, C::dim);
    setColour (PopupMenu::highlightedBackgroundColourId, C::accent);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (ComboBox::textColourId, C::text);
    setColour (ComboBox::arrowColourId, C::dim);
    setColour (TooltipWindow::backgroundColourId, C::button);
    setColour (TooltipWindow::textColourId, C::text);
    setColour (TooltipWindow::outlineColourId, Colours::transparentBlack);
    setColour (AlertWindow::backgroundColourId, C::panel);
    setColour (AlertWindow::textColourId, C::text);
    setColour (AlertWindow::outlineColourId, C::accent.withAlpha (0.4f));
    setColour (TextEditor::backgroundColourId, C::screen);
    setColour (TextEditor::textColourId, C::title);
    setColour (TextEditor::outlineColourId, C::knobRim);
    setColour (TextEditor::focusedOutlineColourId, C::accent);
}

void FlatLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const int sz = (int) s.getProperties()["sz"];   // 0 small, 1 normal, 2 big
    const float size = (float) jmin (w, h);
    const Point<float> c (x + w * 0.5f, y + h * 0.5f);
    const float sw = sz == 2 ? 6.0f : sz == 1 ? 4.0f : 3.0f;
    const float r = size * 0.5f - sw * 0.5f - 1.0f;
    const float ang = start + pos * (end - start);

    g.setColour (C::track);
    g.strokePath (arcPath (c, r, start, end), PathStrokeType (sw, PathStrokeType::curved, PathStrokeType::rounded));
    if (pos > 0.002f)
    {
        g.setColour (C::accent);
        g.strokePath (arcPath (c, r, start, ang), PathStrokeType (sw, PathStrokeType::curved, PathStrokeType::rounded));
    }
    const float ir = r - (sz == 2 ? 13.0f : sz == 1 ? 9.0f : 7.0f);
    g.setColour (C::knob);
    g.fillEllipse (c.x - ir, c.y - ir, ir * 2, ir * 2);
    g.setColour (C::knobRim);
    g.drawEllipse (c.x - ir, c.y - ir, ir * 2, ir * 2, 1.0f);
    const float r0 = ir * 0.32f, r1 = ir - (sz == 2 ? 9.0f : sz == 1 ? 6.0f : 4.0f);
    g.setColour (C::title);
    g.drawLine (Line<float> (c.getPointOnCircumference (r0, ang), c.getPointOnCircumference (r1, ang)), sz == 2 ? 3.5f : sz == 1 ? 2.2f : 1.8f);
}

void FlatLook::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    const bool accent = (bool) b.getProperties()["accent"] || b.getToggleState();
    auto c = accent ? C::accent : C::button;
    if (over) c = c.brighter (0.12f);
    if (down) c = c.darker (0.1f);
    g.setColour (c);
    g.fillRoundedRectangle (b.getLocalBounds().toFloat(), 6.0f);
}

void FlatLook::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const bool accent = (bool) b.getProperties()["accent"] || b.getToggleState();
    g.setColour (accent ? Colours::white : C::text);
    const float fh = b.getProperties().contains ("fh") ? (float) b.getProperties()["fh"] : 11.5f;
    g.setFont (font (fh, 0.1f, fh > 13.0f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred, false);
}

void FlatLook::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&)
{
    g.setColour (C::button);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 6.0f);
    Path a;
    const float ax = w - 12.0f, ay = h * 0.5f;
    a.addTriangle (ax - 3.5f, ay - 1.5f, ax + 3.5f, ay - 1.5f, ax, ay + 2.5f);
    g.setColour (C::dim);
    g.fillPath (a);
}

Font FlatLook::getComboBoxFont (ComboBox&) { return font (11.5f, 0.08f); }
Font FlatLook::getPopupMenuFont() { return font (13.0f, 0.04f); }
void FlatLook::positionComboBoxText (ComboBox& b, Label& l) { l.setBounds (8, 0, b.getWidth() - 24, b.getHeight()); l.setFont (getComboBoxFont (b)); }

// ================================================================ mod chip
ModChip::ModChip (BassProcessor& p, Knob& k, int l) : lfo (l), proc (p), owner (k)
{
    setMouseCursor (MouseCursor::UpDownResizeCursor);
    setRepaintsOnMouseActivity (true);
    setTooltip ("LFO " + String (l + 1) + ". Drag up/down: depth. Double-click: remove. Right-click: invert.");
}

void ModChip::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto col = C::lfo (lfo).withMultipliedAlpha (proc.isLfoOn (lfo) ? 1.0f : 0.35f);
    const float d = proc.getModulationDepth (owner.target, lfo);
    g.setColour (C::screen);
    g.fillEllipse (r);
    g.setColour (col.withAlpha (0.25f));
    g.drawEllipse (r.reduced (1.5f), 1.5f);
    g.setColour (col);
    g.strokePath (arcPath (r.getCentre(), r.getWidth() * 0.5f - 1.5f, 0.0f, d * MathConstants<float>::twoPi * 0.999f), PathStrokeType (2.0f));
    g.setColour (isMouseOver() ? Colours::white : col);
    g.setFont (font (r.getHeight() * 0.6f, 0.0f, true));
    g.drawText (String (lfo + 1), r, Justification::centred, false);
}

void ModChip::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        PopupMenu m;
        m.addItem (1, "Invert");
        m.addItem (2, "Remove");
        Component::SafePointer<ModChip> safe (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [safe] (int res)
        {
            if (safe == nullptr) return;
            auto& c = *safe;
            auto& k = c.owner;
            if (res == 1) c.proc.setModulation (k.target, c.lfo, -c.proc.getModulationDepth (k.target, c.lfo));
            if (res == 2) c.proc.removeModulation (k.target, c.lfo);
            k.refreshMods();
        });
        return;
    }
    startDepth = proc.getModulationDepth (owner.target, lfo);
}

void ModChip::mouseDrag (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const float sens = e.mods.isShiftDown() ? 600.0f : 150.0f;
    proc.setModulation (owner.target, lfo, jlimit (-1.0f, 1.0f, startDepth - (float) e.getDistanceFromDragStartY() / sens));
    repaint();
    owner.repaint();
}

void ModChip::mouseDoubleClick (const MouseEvent&)
{
    proc.removeModulation (owner.target, lfo);
    owner.refreshMods();
}

// ================================================================ knob
Knob::Knob (BassProcessor& p, const String& paramId, const String& n, Size sz)
    : target (BassProcessor::modIndexOf (paramId)), proc (p), name (n), size (sz)
{
    slider.setSliderStyle (Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (sz == Size::Big ? 260 : 200);
    slider.getProperties().set ("sz", sz == Size::Big ? 2 : sz == Size::Normal ? 1 : 0);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.apvts, paramId, slider);
    param = p.apvts.getParameter (paramId);
    if (param != nullptr) slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    if (target >= 0)
        for (int l = 0; l < kNumLfos; ++l)
        {
            chips[(size_t) l] = std::make_unique<ModChip> (proc, *this, l);
            addChildComponent (*chips[(size_t) l]);
        }
}

float Knob::knobSize() const { return size == Size::Big ? 112.0f : size == Size::Normal ? 62.0f : 42.0f; }

void Knob::resized()
{
    const int k = (int) knobSize();
    slider.setBounds ((getWidth() - k) / 2, size == Size::Small ? 4 : 6, k, k);
    refreshMods();
}

void Knob::paint (Graphics& g)
{
    const int k = slider.getBottom();
    const bool big = size == Size::Big, small = size == Size::Small;
    g.setColour (big ? C::title : C::label);
    g.setFont (font (big ? 14.0f : small ? 10.5f : 11.5f, big ? 0.22f : 0.12f));
    g.drawText (name, 0, k + (small ? 2 : 4), getWidth(), 16, Justification::centred, false);
    g.setColour (C::faint);
    g.setFont (font (small ? 10.0f : 11.0f, 0.04f));
    const String v = param != nullptr ? param->getCurrentValueAsText().replace (" %", "%") : String();
    g.drawText (v, 0, k + (small ? 16 : 20), getWidth(), 14, Justification::centred, false);
}

void Knob::paintOverChildren (Graphics& g)
{
    if (target < 0) return;
    const auto sb = slider.getBounds().toFloat();
    const auto c = sb.getCentre();
    const float r = sb.getWidth() * 0.5f;
    const auto rp = slider.getRotaryParameters();
    auto angleOf = [&rp] (float n) { return rp.startAngleRadians + jlimit (0.0f, 1.0f, n) * (rp.endAngleRadians - rp.startAngleRadians); };

    if (dragHover)
    {
        g.setColour (C::accent.withAlpha (0.18f));
        g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c).expanded (4.0f));
        g.setColour (C::accent);
        g.drawEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c).expanded (3.0f), 2.0f);
    }
    if (! anyMod) return;

    const float base = (float) slider.valueToProportionOfLength (slider.getValue());
    const float tw = size == Size::Big ? 3.0f : 2.2f;
    float ringR = r + 3.5f;
    for (int l = 0; l < kNumLfos; ++l)
    {
        if (! proc.hasModulation (target, l)) continue;
        const float depth = proc.getModulationDepth (target, l);
        const auto col = C::lfo (l).withMultipliedAlpha (proc.isLfoOn (l) ? 1.0f : 0.3f);
        g.setColour (col);
        g.strokePath (arcPath (c, ringR, angleOf (base), angleOf (base + depth)), PathStrokeType (tw, PathStrokeType::curved, PathStrokeType::rounded));
        ringR += tw + 1.5f;
    }
    // live modulated value on the value ring
    const float live = proc.modNorm[(size_t) target].load();
    const float sw = size == Size::Big ? 6.0f : size == Size::Normal ? 4.0f : 3.0f;
    const auto p = c.getPointOnCircumference (r - sw * 0.5f - 1.0f, angleOf (live));
    g.setColour (Colours::white);
    g.fillEllipse (Rectangle<float> (sw + 1.5f, sw + 1.5f).withCentre (p));
}

void Knob::refreshMods()
{
    if (target < 0) return;
    anyMod = false;
    const auto sb = slider.getBounds().toFloat();
    const auto c = sb.getCentre();
    const float r = sb.getWidth() * 0.5f;
    const float chip = size == Size::Small ? 13.0f : 15.0f;
    int shown = 0;
    for (int l = 0; l < kNumLfos; ++l)
    {
        auto& ch = chips[(size_t) l];
        const bool on = proc.hasModulation (target, l);
        anyMod |= on;
        ch->setVisible (on);
        if (! on) continue;
        const float a = MathConstants<float>::pi * (0.25f + 0.2f * (float) shown++);
        auto rect = Rectangle<float> (chip, chip).withCentre (c.getPointOnCircumference (r + chip * 0.75f, a)).toNearestInt();
        ch->setBounds (rect.constrainedWithin (getLocalBounds()));
        ch->repaint();
    }
    repaint();
}

void Knob::tick() { if (anyMod) repaint(); }

bool Knob::isInterestedInDragSource (const SourceDetails& s) { return target >= 0 && s.description.toString().startsWith ("lfo:"); }
void Knob::itemDragEnter (const SourceDetails&) { dragHover = true; repaint(); }
void Knob::itemDragExit (const SourceDetails&)  { dragHover = false; repaint(); }

void Knob::itemDropped (const SourceDetails& s)
{
    dragHover = false;
    const int l = s.description.toString().fromFirstOccurrenceOf ("lfo:", false, false).getIntValue();
    if (! proc.hasModulation (target, l)) proc.setModulation (target, l, 0.35f);
    refreshMods();
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
    dust.reserve (3200);
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
    const auto p = proc.modulatedParams();
    power = p.power;
    windowSec = bass::tailSeconds (p) * 1.08 + 0.02;

    // stretched time axis (t = u^2) so CLICK and PUNCH are visible; pitch drawn softened so the sine stays readable
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
        const double a = bass::ampShape (p, t);
        double s = std::sin (ph) + 0.35 * p.fm * bass::fmPart (ph, bass::fmIndex (p, a));   // FM squeak = ripples on the sine
        s = jlimit (-1.0, 1.0, s);
        wave[(size_t) i] = (float) ((s < 0 ? -1.0 : 1.0) * std::pow (std::abs (s), q) * a);
        env[(size_t) i] = (float) a;
    }
}

void WaveScreen::burst (float strength)
{
    const float m = getHeight() * 0.5f, x0 = xAt (0);
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
    rings.push_back ({ 0.0f, power * (0.5f + 0.5f * strength) });
    if (rings.size() > 6) rings.erase (rings.begin());
    flash = 1.0f;
}

void WaveScreen::tick (float dt)
{
    if (getWidth() <= 0) return;
    rebuildShape();
    phaseT += dt;

    auto& e = proc.engine;
    const int hits = e.vizHits.load();
    if (hits != lastHits) { lastHits = hits; burst (e.vizVelocity.load()); }

    level = jmax (jlimit (0.0f, 1.2f, e.vizLevel.load()), level * std::exp (-dt * 5.0f));
    const double nt = e.vizNoteTime.load();
    playU = nt < windowSec ? (float) std::sqrt (nt / windowSec) : 1.0f;

    const auto p = proc.modulatedParams();
    const float H = (float) getHeight(), W = (float) getWidth();
    const int lim = jmax (2, (int) (playU * NS));

    // continuous air: depends on what the bass is playing right now
    emitAcc += dt * std::pow (level, 1.3f) * (90.0f + 420.0f * power) * (0.6f + p.drive);
    while (emitAcc >= 1.0f)
    {
        emitAcc -= 1.0f;
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
    // NOISE: fine fast sparks crackling off the crests
    sparkAcc += dt * p.noise * std::pow (level, 1.1f) * 520.0f;
    while (sparkAcc >= 1.0f)
    {
        sparkAcc -= 1.0f;
        const int idx = rnd.nextInt (lim);
        Dust d;
        const float ang = rnd.nextFloat() * MathConstants<float>::twoPi;
        const float sp = 200.0f + rnd.nextFloat() * 600.0f;
        d.z = 0.6f + 0.4f * rnd.nextFloat();
        d.life = 0.25f + rnd.nextFloat() * 0.35f;
        d.x = xAt (idx / (float) (NS - 1));
        d.y = yAt (idx, 1.0f);
        d.vx = std::cos (ang) * sp;
        d.vy = std::sin (ang) * sp;
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
    if (dust.size() > 3000) dust.erase (dust.begin(), dust.begin() + (long) (dust.size() - 3000));

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

    for (int j = 0; j < 4; ++j)
    {
        const float gph = std::fmod (phaseT * (0.45f + level * 0.9f) + j / 4.0f, 1.0f);
        const float a = (1.0f - gph) * (1.0f - gph) * (0.03f + 0.16f * level * (0.4f + power));
        if (a < 0.004f) continue;
        g.setColour (C::air.withAlpha (a));
        g.strokePath (wavePath (1.0f + gph * (0.4f + 1.6f * power * (0.3f + level)), NS, 5.0f * gph), PathStrokeType (1.0f));
    }
    for (int j = 3; j >= 1; --j)
    {
        g.setColour (C::air.withAlpha (0.05f + 0.02f * (3 - j) + 0.08f * level));
        g.strokePath (wavePath (1.0f + j * (0.06f + 0.12f * level * power), NS, 0.0f), PathStrokeType (1.0f));
    }

    for (const auto& r : rings)
    {
        const float rad = 10.0f + r.t * (W * (0.6f + 0.7f * r.power));
        const float a = jmax (0.0f, 1.0f - r.t / 1.1f);
        Path arc;
        arc.addCentredArc (xAt (0), m, rad, rad * 0.55f, 0, 0.0f, MathConstants<float>::pi, true);
        g.setColour (C::air.withAlpha (a * a * 0.45f));
        g.strokePath (arc, PathStrokeType (1.0f + 3.0f * r.power * a));
    }

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

    const PathStrokeType st (1.8f + 1.4f * power, PathStrokeType::curved, PathStrokeType::rounded);
    g.setColour (Colour (0xff3a3a38));
    g.strokePath (wavePath (1.0f, NS, 0.0f), st);
    g.setColour (C::air);
    g.strokePath (wavePath (1.0f, jlimit (2, NS, (int) (playU * NS)), 0.0f), st);

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

// ================================================================ preset bar
PresetBar::PresetBar (BassProcessor& p) : proc (p)
{
    for (auto* b : { &prev, &next, &save }) addAndMakeVisible (*b);
    prev.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    save.onClick = [this] { askSave(); };
    save.setTooltip ("Save the current sound as a preset");
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    save.setBounds (r.removeFromRight (58));
    r.removeFromRight (6);
    next.setBounds (r.removeFromRight (28));
    prev.setBounds (r.removeFromLeft (28));
    nameArea = r.reduced (4, 0);
}

void PresetBar::paint (Graphics& g)
{
    auto r = nameArea.toFloat();
    g.setColour (C::button);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (C::title);
    g.setFont (font (12.0f, 0.1f));
    g.drawText (proc.getPresetName().toUpperCase(), r.reduced (12.0f, 0.0f), Justification::centred, true);
}

void PresetBar::mouseDown (const MouseEvent& e) { if (nameArea.contains (e.getPosition())) showMenu(); }

void PresetBar::step (int dir)
{
    const auto names = proc.allPresetNames();
    const int n = names.size();
    const int cur = proc.currentPresetIndex();
    const int idx = cur < 0 ? (dir > 0 ? 0 : n - 1) : ((cur + dir) % n + n) % n;
    if (onPick) onPick (idx);
    repaint();
}

void PresetBar::showMenu()
{
    const auto names = proc.allPresetNames();
    const int nf = (int) bass::factoryPresets().size();
    PopupMenu m;
    m.addSectionHeader ("FACTORY");
    for (int i = 0; i < nf; ++i) m.addItem (100 + i, names[i], true, names[i] == proc.getPresetName());
    m.addSectionHeader ("YOURS");
    if (names.size() == nf) m.addItem (1, "(no saved presets yet)", false);
    for (int i = nf; i < names.size(); ++i) m.addItem (100 + i, names[i], true, names[i] == proc.getPresetName());
    m.addSeparator();
    m.addItem (2, "Save as...");
    m.addItem (3, "Open presets folder");
    Component::SafePointer<PresetBar> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()), [safe] (int res)
    {
        if (safe == nullptr || res == 0) return;
        if (res == 2) { safe->askSave(); return; }
        if (res == 3) { BassProcessor::presetFolder().startAsProcess(); return; }
        if (res >= 100 && safe->onPick) safe->onPick (res - 100);
        safe->repaint();
    });
}

void PresetBar::askSave()
{
    saveWindow = std::make_unique<AlertWindow> ("Save preset", "Name:", MessageBoxIconType::NoIcon, this);
    const bool factory = [&] { for (auto& p : bass::factoryPresets()) if (proc.getPresetName() == p.name) return true; return false; }();
    saveWindow->addTextEditor ("name", factory || proc.getPresetName().startsWith ("RANDOM") ? String ("MY 808") : proc.getPresetName());
    saveWindow->addButton ("SAVE", 1, KeyPress (KeyPress::returnKey));
    saveWindow->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));
    Component::SafePointer<PresetBar> safe (this);
    saveWindow->enterModalState (true, ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr) return;
        if (result == 1) safe->proc.savePreset (safe->saveWindow->getTextEditorContents ("name"));   // same name = update it
        safe->saveWindow.reset();
        safe->repaint();
    }), false);
}

// ================================================================ noise picker
NoisePicker::NoisePicker (BassProcessor& p) : proc (p)
{
    for (auto* b : { &prev, &next }) addAndMakeVisible (*b);
    prev.onClick = [this] { setIndex (index() - 1); };
    next.onClick = [this] { setIndex (index() + 1); };
    setMouseCursor (MouseCursor::PointingHandCursor);
}

int NoisePicker::index() const { return (int) proc.apvts.getRawParameterValue (ParamID::noiseType)->load(); }

void NoisePicker::setIndex (int i)
{
    i = ((i % bass::kNumNoises) + bass::kNumNoises) % bass::kNumNoises;
    if (auto* prm = proc.apvts.getParameter (ParamID::noiseType))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
        prm->endChangeGesture();
    }
    repaint();
}

void NoisePicker::resized()
{
    auto r = getLocalBounds();
    prev.setBounds (r.removeFromLeft (28));
    next.setBounds (r.removeFromRight (28));
    body = r.reduced (4, 0);
}

void NoisePicker::paint (Graphics& g)
{
    auto r = body.toFloat();
    g.setColour (C::screen);
    g.fillRoundedRectangle (r, 6.0f);
    // tiny noise texture (deterministic per type)
    Random rr (index() * 77 + 5);
    g.setColour (C::air.withAlpha (0.13f));
    for (float x = r.getX() + 6; x < r.getRight() - 6; x += 2.0f)
    {
        const float h = (index() == 7 || index() == 3) ? (rr.nextFloat() < 0.08f ? r.getHeight() * 0.35f : 1.5f) : rr.nextFloat() * r.getHeight() * 0.32f;
        g.fillRect (x, r.getCentreY() - h, 1.0f, h * 2.0f);
    }
    g.setColour (C::title);
    g.setFont (font (12.0f, 0.14f));
    g.drawText (bass::noiseNames()[jlimit (0, bass::kNumNoises - 1, index())], r, Justification::centred, false);
}

void NoisePicker::mouseDown (const MouseEvent& e)
{
    if (! body.contains (e.getPosition())) return;
    PopupMenu m;
    for (int i = 0; i < bass::kNumNoises; ++i) m.addItem (i + 1, bass::noiseNames()[i], true, i == index());
    Component::SafePointer<NoisePicker> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [safe] (int r) { if (safe != nullptr && r > 0) safe->setIndex (r - 1); });
}

// ================================================================ LFO tab
LfoTab::LfoTab (BassProcessor& p, int i) : index (i), proc (p)
{
    setMouseCursor (MouseCursor::DraggingHandCursor);
    setTooltip ("Click the light: this LFO on/off everywhere (links are kept).\nDrag onto any knob to modulate it.");
}

void LfoTab::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto col = C::lfo (index);
    g.setColour (selected ? C::button.brighter (0.08f) : C::button);
    g.fillRoundedRectangle (r, 6.0f);
    if (selected)
    {
        g.setColour (col);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.2f);
    }
    const bool on = proc.isLfoOn (index);
    const auto led = Rectangle<float> (8.0f, 8.0f).withCentre ({ r.getX() + 12.0f, r.getCentreY() });
    if (on)
    {
        g.setColour (col.withAlpha (0.45f + 0.55f * proc.lfoValueOut[(size_t) index].load()));
        g.fillEllipse (led);
    }
    else
    {
        g.setColour (col.withAlpha (0.45f));
        g.drawEllipse (led.reduced (0.5f), 1.2f);
    }
    g.setColour ((selected ? Colours::white : C::text).withAlpha (on ? 1.0f : 0.45f));
    g.setFont (font (11.5f, 0.1f, true));
    g.drawText ("LFO " + String (index + 1), r.withTrimmedLeft (22.0f), Justification::centredLeft, false);
    const int n = proc.countModulations (index);
    if (n > 0)
    {
        g.setColour (col);
        g.setFont (font (10.5f, 0.0f, true));
        g.drawText (String (n), r.withTrimmedRight (8.0f), Justification::centredRight, false);
    }
}

void LfoTab::mouseMove (const MouseEvent& e) { setMouseCursor (e.position.x < 22.0f ? MouseCursor::NormalCursor : MouseCursor::DraggingHandCursor); }

void LfoTab::mouseDown (const MouseEvent& e)
{
    dragging = false;
    if (e.position.x < 22.0f)
        if (auto* prm = proc.apvts.getParameter (ParamID::lfo (index, "On")))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
            prm->endChangeGesture();
            repaint();
        }
    if (onSelect) onSelect (index);
}

void LfoTab::mouseDrag (const MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 5) return;
    if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragging = true;
        c->startDragging ("lfo:" + String (index), this);
    }
}

void LfoTab::mouseUp (const MouseEvent&) { dragging = false; }

// ================================================================ LFO editor
LfoEditor::LfoEditor (BassProcessor& p) : proc (p)
{
    pts = proc.getLfoPoints (0);
    setTooltip ("Drag the points. Drag the small rings to bend a segment, double-click a ring to straighten it.");
}

void LfoEditor::setLfo (int l)
{
    lfo = l;
    pts = proc.getLfoPoints (l);
    dragIndex = hoverIndex = -1;
    repaint();
}

void LfoEditor::tick()
{
    if (dragIndex < 0)
    {
        const auto now = proc.getLfoPoints (lfo);
        bool same = true;
        for (int i = 0; i < kLfoPoints; ++i)
            same &= now[(size_t) i].x == pts[(size_t) i].x && now[(size_t) i].y == pts[(size_t) i].y && now[(size_t) i].curve == pts[(size_t) i].curve;
        if (! same) pts = now;
    }
    repaint();
}

Rectangle<float> LfoEditor::area() const { return getLocalBounds().toFloat().reduced (10.0f, 10.0f).withTrimmedTop (6.0f); }

Point<float> LfoEditor::toScreen (float x, float y) const
{
    const auto a = area();
    return { a.getX() + x * a.getWidth(), a.getBottom() - y * a.getHeight() };
}

Point<float> LfoEditor::handlePos (int seg) const
{
    const auto& a = pts[(size_t) seg];
    const auto& b = pts[(size_t) seg + 1];
    return toScreen ((a.x + b.x) * 0.5f, a.y + (b.y - a.y) * BassProcessor::segmentShape (0.5f, a.curve));
}

int LfoEditor::hitTest (Point<float> m, bool& isHandle) const
{
    isHandle = false;
    int best = -1;
    float bestD = 11.0f;
    for (int i = 0; i < kLfoPoints; ++i)
    {
        const float d = toScreen (pts[(size_t) i].x, pts[(size_t) i].y).getDistanceFrom (m);
        if (d < bestD) { bestD = d; best = i; }
    }
    if (best >= 0) return best;
    for (int s = 0; s < kLfoPoints - 1; ++s)
    {
        const float d = handlePos (s).getDistanceFrom (m);
        if (d < bestD) { bestD = d; best = s; isHandle = true; }
    }
    return best;
}

void LfoEditor::mouseMove (const MouseEvent& e)
{
    bool h = false;
    const int i = hitTest (e.position, h);
    if (i != hoverIndex || h != hoverHandle)
    {
        hoverIndex = i; hoverHandle = h;
        setMouseCursor (i < 0 ? MouseCursor::NormalCursor : h ? MouseCursor::UpDownResizeCursor : MouseCursor::DraggingHandCursor);
    }
}

void LfoEditor::mouseExit (const MouseEvent&) { hoverIndex = -1; }

void LfoEditor::mouseDown (const MouseEvent& e)
{
    pts = proc.getLfoPoints (lfo);
    dragIndex = hitTest (e.position, dragHandle);
    dragStart = e.position;
    if (dragIndex >= 0 && dragHandle) dragStartCurve = pts[(size_t) dragIndex].curve;
}

void LfoEditor::mouseDrag (const MouseEvent& e)
{
    if (dragIndex < 0) return;
    const auto a = area();
    if (dragHandle)
    {
        auto& p0 = pts[(size_t) dragIndex];
        const auto& p1 = pts[(size_t) dragIndex + 1];
        const float up = (dragStart.y - e.position.y) / (a.getHeight() * 0.6f);
        p0.curve = jlimit (-1.0f, 1.0f, dragStartCurve + (p1.y >= p0.y ? -up : up));
    }
    else
    {
        auto& p = pts[(size_t) dragIndex];
        p.y = jlimit (0.0f, 1.0f, (a.getBottom() - e.position.y) / a.getHeight());
        if (dragIndex > 0 && dragIndex < kLfoPoints - 1)
            p.x = jlimit (pts[(size_t) dragIndex - 1].x, pts[(size_t) dragIndex + 1].x, (e.position.x - a.getX()) / a.getWidth());
    }
    proc.setLfoPoints (lfo, pts);
    repaint();
}

void LfoEditor::mouseUp (const MouseEvent&) { dragIndex = -1; }

void LfoEditor::mouseDoubleClick (const MouseEvent& e)
{
    bool h = false;
    const int i = hitTest (e.position, h);
    if (i >= 0 && h) { pts[(size_t) i].curve = 0.0f; proc.setLfoPoints (lfo, pts); }
}

void LfoEditor::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (C::screen);
    g.fillRoundedRectangle (r, 8.0f);
    const auto a = area();
    const auto col = C::lfo (lfo);

    g.setColour (Colours::white.withAlpha (0.05f));
    for (int i = 1; i < 4; ++i) g.drawVerticalLine ((int) (a.getX() + a.getWidth() * (float) i / 4.0f), a.getY(), a.getBottom());
    g.drawHorizontalLine ((int) a.getCentreY(), a.getX(), a.getRight());

    Path p, fill;
    const int n = jmax (8, (int) a.getWidth());
    size_t seg = 0;
    for (int i = 0; i <= n; ++i)
    {
        const float x = (float) i / (float) n;
        while (seg + 2 < pts.size() && x > pts[seg + 1].x) ++seg;
        const auto& p0 = pts[seg];
        const auto& p1 = pts[seg + 1];
        const float y = (p1.x - p0.x < 1.0e-5f) ? p1.y
                      : p0.y + (p1.y - p0.y) * BassProcessor::segmentShape (jlimit (0.0f, 1.0f, (x - p0.x) / (p1.x - p0.x)), p0.curve);
        const auto q = toScreen (x, y);
        if (i == 0) { p.startNewSubPath (q); fill.startNewSubPath (a.getX(), a.getBottom()); }
        p.lineTo (q);
        fill.lineTo (q);
    }
    fill.lineTo (a.getRight(), a.getBottom());
    fill.closeSubPath();
    g.setColour (col.withAlpha (0.10f));
    g.fillPath (fill);
    g.setColour (col);
    g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));

    for (int s = 0; s < kLfoPoints - 1; ++s)
    {
        if (pts[(size_t) s + 1].x - pts[(size_t) s].x < 0.02f) continue;
        const auto hp = handlePos (s);
        const bool hot = (hoverIndex == s && hoverHandle) || (dragIndex == s && dragHandle);
        g.setColour (C::screen);
        g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (hp));
        g.setColour (col.withAlpha (hot ? 1.0f : 0.6f));
        g.drawEllipse (Rectangle<float> (hot ? 10.0f : 8.0f, hot ? 10.0f : 8.0f).withCentre (hp), 1.3f);
    }
    for (int i = 0; i < kLfoPoints; ++i)
    {
        const auto q = toScreen (pts[(size_t) i].x, pts[(size_t) i].y);
        const bool hot = (hoverIndex == i && ! hoverHandle) || (dragIndex == i && ! dragHandle);
        g.setColour (col.withAlpha (0.3f));
        g.fillEllipse (Rectangle<float> (hot ? 16.0f : 12.0f, hot ? 16.0f : 12.0f).withCentre (q));
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre (q));
    }

    const float ph = proc.lfoPhaseOut[(size_t) lfo].load();
    const float val = proc.lfoValueOut[(size_t) lfo].load();
    const float rise = proc.lfoRiseOut[(size_t) lfo].load();
    const auto q = toScreen (ph, rise > 0.001f ? val / rise : val);
    g.setColour (Colours::white.withAlpha (0.15f));
    g.drawVerticalLine ((int) q.x, a.getY(), a.getBottom());
    g.setColour (Colours::white.withAlpha (0.4f + 0.6f * rise));
    g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (q));

    const bool sync = proc.apvts.getRawParameterValue (ParamID::lfo (lfo, "Sync"))->load() > 0.5f;
    const String rate = sync ? BassProcessor::divisionNames()[(int) proc.apvts.getRawParameterValue (ParamID::lfo (lfo, "Div"))->load()]
                             : String (proc.apvts.getRawParameterValue (ParamID::lfo (lfo, "Rate"))->load(), 2) + " Hz";
    g.setColour (C::dim);
    g.setFont (font (10.0f, 0.06f));
    g.drawText (rate, r.reduced (10.0f, 4.0f), Justification::topRight, false);
}

// ================================================================ editor
BassEditor::BassEditor (BassProcessor& p)
    : AudioProcessorEditor (p), proc (p), screen (p), presetBar (p), noisePicker (p), lfoEditor (p)
{
    setLookAndFeel (&look);
    auto& s = proc.apvts;
    for (auto* c : std::initializer_list<Component*> { &screen, &subMeter, &outMeter, &presetBar, &randomButton, &noisePicker, &lfoEditor })
        addAndMakeVisible (c);

    for (auto [id, nm] : { std::pair<const char*, const char*> { ParamID::click, "CLICK" }, { ParamID::punch, "PUNCH" }, { ParamID::drop, "DROP" },
                           { ParamID::power, "POWER" }, { ParamID::length, "LENGTH" }, { ParamID::drive, "DRIVE" }, { ParamID::fm, "FM" } })
    {
        mainKnobs.push_back (std::make_unique<Knob> (proc, id, nm, String (id) == ParamID::power ? Knob::Size::Big : Knob::Size::Normal));
        addAndMakeVisible (*mainKnobs.back());
    }
    mainKnobs.back()->slider.setTooltip ("FM: odd harmonics in the low-mids, a stylish squeak on top of the bass");
    for (auto [id, nm] : { std::pair<const char*, const char*> { ParamID::noise, "AMOUNT" }, { ParamID::noiseTone, "TONE" }, { ParamID::noiseTail, "TAIL" } })
    {
        noiseKnobs.push_back (std::make_unique<Knob> (proc, id, nm, Knob::Size::Small));
        addAndMakeVisible (*noiseKnobs.back());
    }

    randomButton.getProperties().set ("accent", true);
    randomButton.getProperties().set ("fh", 15.0f);
    randomButton.setTooltip ("A new random 808 (always a working one)");
    randomButton.onClick = [this] { randomize(); };
    presetBar.onPick = [this] (int i) { proc.loadPresetByIndex (i); afterPresetLoad(); };

    for (int l = 0; l < kNumLfos; ++l)
    {
        lfoTabs[(size_t) l] = std::make_unique<LfoTab> (proc, l);
        lfoTabs[(size_t) l]->onSelect = [this] (int i) { selectLfo (i); };
        addAndMakeVisible (*lfoTabs[(size_t) l]);

        auto c = std::make_unique<LfoControls>();
        c->preset.addItemList (BassProcessor::lfoPresetNames(), 1);
        c->preset.setTextWhenNothingSelected ("SHAPE...");
        c->preset.onChange = [this, l]
        {
            auto& box = lfoControls[(size_t) l]->preset;
            if (box.getSelectedItemIndex() < 0) return;
            proc.setLfoPoints (l, BassProcessor::lfoPreset (box.getSelectedItemIndex()));
            lfoEditor.setLfo (l);
            box.setSelectedId (0, dontSendNotification);
        };
        c->div.addItemList (BassProcessor::divisionNames(), 1);
        c->mode.addItemList ({ "FREE", "RETRIG", "ENV" }, 1);
        c->mode.setTooltip ("FREE: runs on its own.  RETRIG: restarts with each note.  ENV: runs once per note and holds.");
        c->sync.setClickingTogglesState (true);
        c->sync.setTooltip ("Lock the LFO to the project tempo");
        c->remove.setTooltip ("Removes this LFO from every knob it modulates");
        c->remove.onClick = [this, l]
        {
            for (int t = 0; t < kNumModTargets; ++t)
                if (proc.hasModulation (t, l)) proc.removeModulation (t, l);
            refreshAllMods();
        };
        c->rate = std::make_unique<Knob> (proc, ParamID::lfo (l, "Rate"), "RATE", Knob::Size::Small);
        c->rise = std::make_unique<Knob> (proc, ParamID::lfo (l, "Rise"), "RISE", Knob::Size::Small);
        c->rise->slider.setTooltip ("RISE: the LFO fades in over this time after a note starts");
        c->divA  = std::make_unique<ComboAtt> (s, ParamID::lfo (l, "Div"), c->div);
        c->modeA = std::make_unique<ComboAtt> (s, ParamID::lfo (l, "Mode"), c->mode);
        c->syncA = std::make_unique<ButtonAtt> (s, ParamID::lfo (l, "Sync"), c->sync);
        for (Component* comp : { (Component*) &c->preset, (Component*) &c->div, (Component*) &c->mode, (Component*) &c->sync,
                                 (Component*) &c->remove, (Component*) c->rate.get(), (Component*) c->rise.get() })
            addChildComponent (comp);
        lfoControls[(size_t) l] = std::move (c);
    }
    selectLfo (0);

    setSize (900, 650);
    startTimerHz (60);
}

BassEditor::~BassEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BassEditor::selectLfo (int i)
{
    selectedLfo = i;
    for (int l = 0; l < kNumLfos; ++l)
    {
        lfoTabs[(size_t) l]->selected = (l == i);
        lfoTabs[(size_t) l]->repaint();
        auto& c = *lfoControls[(size_t) l];
        for (Component* comp : { (Component*) &c.preset, (Component*) &c.div, (Component*) &c.mode, (Component*) &c.sync,
                                 (Component*) &c.remove, (Component*) c.rate.get(), (Component*) c.rise.get() })
            comp->setVisible (l == i);
    }
    lfoEditor.setLfo (i);
}

void BassEditor::refreshAllMods()
{
    for (auto& k : mainKnobs) k->refreshMods();
    for (auto& k : noiseKnobs) k->refreshMods();
    for (auto& c : lfoControls) { c->rate->refreshMods(); c->rise->refreshMods(); }
}

void BassEditor::afterPresetLoad()
{
    animT = 1.0f;
    lfoEditor.setLfo (selectedLfo);
    refreshAllMods();
    presetBar.repaint();
}

void BassEditor::randomize()
{
    BassProcessor::randomTargets (rnd, animTo);
    animFrom.clear();
    for (auto& [id, v] : animTo)
    {
        auto* prm = proc.apvts.getParameter (id);
        animFrom.push_back (proc.apvts.getRawParameterValue (id)->load());
        if (prm != nullptr) prm->beginChangeGesture();
    }
    animT = 0.0f;
    proc.setPresetName ("RANDOM #" + String (100 + rnd.nextInt (900)));
    presetBar.repaint();
}

void BassEditor::tick (float dt)
{
    if (animT < 1.0f)
    {
        animT = jmin (1.0f, animT + dt / 0.35f);
        const float k = 1.0f - std::pow (1.0f - animT, 3.0f);
        for (size_t i = 0; i < animTo.size(); ++i)
            if (auto* prm = proc.apvts.getParameter (animTo[i].first))
            {
                const bool isChoice = String (animTo[i].first) == ParamID::noiseType;
                const float v = isChoice ? (animT >= 1.0f ? animTo[i].second : animFrom[i]) : animFrom[i] + (animTo[i].second - animFrom[i]) * k;
                prm->setValueNotifyingHost (prm->convertTo0to1 (v));
                if (animT >= 1.0f) prm->endChangeGesture();
            }
    }

    screen.tick (dt);
    screen.repaint();
    subMeter.set (proc.engine.vizSub.load() * 0.8f);
    outMeter.set (proc.engine.vizLevel.load());
    for (auto& k : mainKnobs) k->tick();
    for (auto& k : noiseKnobs) k->tick();
    for (auto& c : lfoControls) { c->rate->tick(); c->rise->tick(); }
    for (auto& t : lfoTabs) t->repaint();
    lfoEditor.tick();
    noisePicker.repaint();
}

void BassEditor::paint (Graphics& g)
{
    g.fillAll (C::bg);

    g.setColour (C::title);
    g.setFont (font (31.0f, 0.06f, true));
    g.drawText ("808 BASS MAKER", 0, 12, getWidth(), 38, Justification::centred, false);
    g.setColour (C::dim);
    g.setFont (font (11.0f, 0.24f));
    g.drawText ("HEAVY 808 DESIGNER", 0, 50, getWidth(), 16, Justification::centred, false);
    g.setColour (C::faint);
    g.setFont (font (10.0f, 0.14f));
    g.drawText ("v0.11", presetBar.getX(), presetBar.getBottom() + 6, 60, 12, Justification::left, false);

    g.setColour (C::dim);
    g.setFont (font (10.5f, 0.16f));
    g.drawText ("SUB", subMeter.getX() - 14, subMeter.getBottom() + 6, subMeter.getWidth() + 28, 14, Justification::centred, false);
    g.drawText ("OUT", outMeter.getX() - 14, outMeter.getBottom() + 6, outMeter.getWidth() + 28, 14, Justification::centred, false);

    for (auto r : { noisePanel, lfoPanel })
    {
        g.setColour (C::panel);
        g.fillRoundedRectangle (r.toFloat(), 12.0f);
    }
    g.setColour (C::label);
    g.setFont (font (12.0f, 0.2f, true));
    g.drawText ("NOISE", noisePanel.getX() + 16, noisePanel.getY() + 10, 120, 16, Justification::left, false);

    // LFO column labels
    const int x0 = lfoPanel.getX(), y0 = lfoPanel.getY();
    g.setColour (C::dim);
    g.setFont (font (10.0f, 0.14f));
    g.drawText ("SHAPE",    x0 + 306, y0 + 12, 90, 14, Justification::left, false);
    g.drawText ("MODE",     x0 + 306, y0 + 68, 90, 14, Justification::left, false);
    g.drawText ("DIVISION", x0 + 404, y0 + 12, 90, 14, Justification::left, false);
    g.drawText ("TEMPO",    x0 + 404, y0 + 68, 90, 14, Justification::left, false);
    g.setColour (C::faint);
    g.setFont (font (9.5f, 0.1f));
    g.drawFittedText ("DRAG A TAB\nONTO A KNOB", x0 + 12, y0 + 136, 80, 30, Justification::centred, 2);
}

void BassEditor::resized()
{
    const int W = getWidth();
    presetBar.setBounds (24, 20, 252, 30);
    randomButton.setBounds (W - 24 - 150, 16, 150, 40);

    subMeter.setBounds (28, 98, 6, 150);
    outMeter.setBounds (W - 34, 98, 6, 150);
    screen.setBounds (52, 80, W - 104, 182);

    const int top = 276, x0 = 30;
    const float unit = (W - 2 * x0) / 7.5f;
    float x = (float) x0;
    for (auto& k : mainKnobs)
    {
        const bool big = k.get() == mainKnobs[3].get();
        const float w = big ? unit * 1.5f : unit;
        k->setBounds ((int) x, big ? top : top + 22, (int) w, big ? 156 : 112);
        x += w;
    }

    noisePanel = { 24, 446, 262, 184 };
    lfoPanel = { 298, 446, W - 24 - 298, 184 };

    noisePicker.setBounds (noisePanel.getX() + 14, noisePanel.getY() + 36, noisePanel.getWidth() - 28, 30);
    const int nkw = (noisePanel.getWidth() - 20) / 3;
    for (int i = 0; i < 3; ++i)
        noiseKnobs[(size_t) i]->setBounds (noisePanel.getX() + 10 + i * nkw, noisePanel.getY() + 80, nkw, 96);

    const int lx = lfoPanel.getX(), ly = lfoPanel.getY();
    for (int l = 0; l < kNumLfos; ++l) lfoTabs[(size_t) l]->setBounds (lx + 12, ly + 14 + l * 40, 80, 32);
    lfoEditor.setBounds (lx + 100, ly + 14, 196, 156);
    for (auto& cp : lfoControls)
    {
        auto& c = *cp;
        c.preset.setBounds (lx + 306, ly + 28, 90, 28);
        c.mode.setBounds (lx + 306, ly + 84, 90, 28);
        c.div.setBounds (lx + 404, ly + 28, 90, 28);
        c.sync.setBounds (lx + 404, ly + 84, 90, 28);
        c.remove.setBounds (lx + 306, ly + 134, 188, 28);
        c.rate->setBounds (lx + 500, ly + 8, lfoPanel.getWidth() - 506, 84);
        c.rise->setBounds (lx + 500, ly + 92, lfoPanel.getWidth() - 506, 84);
    }
    refreshAllMods();
}

juce::AudioProcessorEditor* BassProcessor::createEditor() { return new BassEditor (*this); }
