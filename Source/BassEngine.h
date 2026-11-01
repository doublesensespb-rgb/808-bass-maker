// 808 BASS MAKER - header-only DSP engine (no JUCE), so it can be unit-tested with plain g++.
//
// Built on the principle of Thematic's Serum 808 patch:
//   sine (OCT -2) + three pitch envelopes of different speed (fast CLICK, 1/4-ish PUNCH, slow DROP over the tail)
//   + linear volume tail (LENGTH) + short noise transient + ASYM distortion whose drive falls with the tail
//   + EQ high cut. POWER is a macro: more drive, more sub, more punch, harder output clipper.
// The sub (below ~90 Hz) is kept as a clean mono sine, the dirt lives above it, so the low end never collapses.
// Everything runs at 4x oversampling and is decimated with two half-band FIR stages.
#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace bass
{
constexpr double kPi = 3.14159265358979323846;

struct Params
{
    float click  = 0.75f;   // fast pitch spike (0..1)
    float punch  = 0.50f;   // body pitch envelope (0..1)
    float drop   = 0.30f;   // slow pitch slide over the tail (0..1)
    float power  = 0.65f;   // macro (0..1)
    float length = 0.70f;   // tail length (0..1)
    float drive  = 0.40f;   // asym distortion (0..1)
    float glide  = 0.35f;   // portamento for overlapping notes (0..1)
    float output = 0.0f;    // output gain, dB
};

// ---- parameter mapping (shared by the DSP and the screen, so the picture matches the sound) ----
inline double clickSemis (const Params& p) { return p.click * 36.0; }
inline double clickTau   (const Params&)   { return 0.0045; }
inline double punchSemis (const Params& p) { return p.punch * 19.0; }
inline double punchTau   (const Params& p) { return 0.030 + 0.030 * p.punch; }
inline double dropSemis  (const Params& p) { return p.drop * 7.0; }
inline double tailSeconds (const Params& p) { return 0.15 * std::pow (4.0 / 0.15, (double) p.length); }
inline double glideSeconds (const Params& p) { return 0.45 * p.glide * p.glide; }
inline double driveGain (const Params& p) { return 1.0 + (double) p.drive * 9.0 * (1.0 + 0.9 * p.power); }
inline double driveMix  (const Params& p) { return std::fmin (1.0, p.drive * 2.5); }

// amplitude envelope shape (0..1) at time t after note-on (no release)
inline double ampShape (const Params& p, double t)
{
    const double atk = 0.0006;
    if (t < atk) return t / atk;
    const double L = tailSeconds (p);
    const double x = std::fmax (0.0, 1.0 - (t - atk) / L);
    return std::pow (x, 1.0 + 0.4 * (1.0 - p.power));   // Serum patch: linear volume ramp; less POWER = a bit shorter feel
}

// pitch offset in semitones at time t after note-on
inline double pitchShape (const Params& p, double t)
{
    const double L = tailSeconds (p);
    return clickSemis (p) * std::exp (-t / clickTau (p))
         + punchSemis (p) * (1.0 + 0.6 * p.power) * std::exp (-t / punchTau (p))
         + dropSemis (p) * std::fmax (0.0, 1.0 - t / L);
}

// asymmetric soft drive, 0 at 0, ~+/-1 at full scale
inline double asym (double x, double g)
{
    const double b = 0.28;
    const double n = std::tanh (g + b) - std::tanh (b);
    return (std::tanh (g * x + b) - std::tanh (b)) / n;
}

// Theoretical waveform for the screen: n points across `seconds`, values -1..1, plus the envelope.
inline void previewShape (const Params& p, double seconds, int n, float* wave, float* env, double baseHz = 46.25)
{
    double ph = 0.0;
    const double dt = seconds / n;
    for (int i = 0; i < n; ++i)
    {
        const double t = i * dt;
        const double f = baseHz * std::pow (2.0, pitchShape (p, t) / 12.0);
        ph += 2.0 * kPi * f * dt;
        const double a = ampShape (p, t);
        const double g = driveGain (p) * (0.55 + 0.45 * a);
        const double s = std::sin (ph);
        const double d = s + driveMix (p) * (asym (s, g) - s) * 0.6;   // the picture keeps the sine readable
        wave[i] = (float) (d * a);
        env[i] = (float) a;
    }
}

// ---- small DSP blocks ----
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void lowpass (double sr, double f, double q)  { calc (sr, f, q, false); }
    void highpass (double sr, double f, double q) { calc (sr, f, q, true); }
    void calc (double sr, double f, double q, bool hp)
    {
        const double w = 2.0 * kPi * f / sr, c = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al;
        if (hp) { b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2; }
        else    { b0 = (1 - c) / 2; b1 = 1 - c;    b2 = (1 - c) / 2; }
        b0 /= a0; b1 /= a0; b2 /= a0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    double operator() (double x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
};

// Linkwitz-Riley 4th order (two Butterworth biquads)
struct LR4
{
    Biquad a, b;
    void setLow (double sr, double f)  { a.lowpass (sr, f, 0.70710678);  b.lowpass (sr, f, 0.70710678); }
    void setHigh (double sr, double f) { a.highpass (sr, f, 0.70710678); b.highpass (sr, f, 0.70710678); }
    double operator() (double x) { return b (a (x)); }
    void reset() { a.reset(); b.reset(); }
};

// 2:1 decimator with a windowed-sinc half-band FIR
struct Decimator2
{
    static constexpr int N = 47;
    std::array<double, N> h {};
    std::array<double, 2 * N> buf {};
    int pos = 0;
    Decimator2()
    {
        const int M = N - 1;
        double sum = 0;
        for (int i = 0; i < N; ++i)
        {
            const double k = i - M / 2.0;
            const double sinc = k == 0 ? 0.5 : std::sin (kPi * 0.5 * k) / (kPi * k);
            const double w = 0.42 - 0.5 * std::cos (2 * kPi * i / M) + 0.08 * std::cos (4 * kPi * i / M);
            h[(size_t) i] = sinc * w;
            sum += h[(size_t) i];
        }
        for (auto& v : h) v /= sum;
    }
    void push (double x) { buf[(size_t) pos] = buf[(size_t) (pos + N)] = x; pos = (pos + 1) % N; }
    double out() const
    {
        double y = 0;
        for (int i = 0; i < N; ++i) y += h[(size_t) i] * buf[(size_t) (pos + i)];
        return y;
    }
    void reset() { buf.fill (0); pos = 0; }
};

// ---- the voice / synth ----
class Engine
{
public:
    static constexpr int OS = 4;

    // read by the UI (written on the audio thread)
    std::atomic<float> vizAmp { 0.0f };      // current amplitude envelope 0..1
    std::atomic<float> vizLevel { 0.0f };    // output peak of the last block
    std::atomic<float> vizSub { 0.0f };      // clean-sub peak of the last block
    std::atomic<float> vizVelocity { 0.0f };
    std::atomic<int> vizHits { 0 };          // increments on every note start
    std::atomic<double> vizNoteTime { 99.0 };  // seconds since the last note start

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        osr = sr * OS;
        subLo.setLow (osr, 90.0);
        dirtHi.setHigh (osr, 90.0);
        hiCut.lowpass (osr, 9000.0, 0.6);
        noiseHp.highpass (osr, 1800.0, 0.7);
        dcBlock.highpass (osr, 18.0, 0.7);
        reset();
    }

    void reset()
    {
        subLo.reset(); dirtHi.reset(); hiCut.reset(); noiseHp.reset(); dcBlock.reset();
        d1.reset(); d2a.reset(); d2b.reset();
        stackSize = 0; gate = false; rel = 0; phase = 0; t = 99; curNote = targetNote = 36;
    }

    void setParams (const Params& np) { p = np; }
    void setPitchBend (double semis) { bend = semis; }

    void noteOn (int note, float velocity)
    {
        const bool overlapping = gate && stackSize > 0;
        push (note);
        targetNote = note;
        vel = velocity;
        if (! overlapping || glideSeconds (p) <= 0.0005)
        {
            curNote = note;
            if (! overlapping) phase = 0.0;   // every fresh hit starts at the zero crossing: same punch every time
        }
        t = 0.0;              // Serum patch has LEGATO off: envelopes retrigger on every note
        gate = true;
        rel = 1.0;
        vizHits.fetch_add (1);
        vizVelocity.store (velocity);
    }

    void noteOff (int note)
    {
        remove (note);
        if (stackSize > 0)
        {
            targetNote = stack[(size_t) (stackSize - 1)];   // last-note priority, glide back without retrigger
            if (glideSeconds (p) <= 0.0005) curNote = targetNote;
        }
        else gate = false;
    }

    void allNotesOff() { stackSize = 0; gate = false; }

    // renders mono into out[0..n)
    void render (float* out, int n)
    {
        float peak = 0, subPeak = 0;
        const double dtO = 1.0 / osr;
        const double glideCoef = glideSeconds (p) > 0.0005 ? std::exp (-dtO / (glideSeconds (p) / 3.0)) : 0.0;
        const double relCoef = std::exp (-dtO / 0.005);    // Serum ENV1 release 15 ms
        const double gD = driveGain (p), mix = driveMix (p);
        const double subGain = 1.0 + 0.5 * p.power;
        const double clipIn = std::pow (10.0, (p.power * 8.0) / 20.0);
        const double outGain = std::pow (10.0, p.output / 20.0) * 0.82;
        const double noiseAmt = (0.10 + 0.35 * p.power) * (0.3 + 0.7 * p.click);

        for (int i = 0; i < n; ++i)
        {
            double o2[2];
            for (int k = 0; k < 2; ++k)
            {
                double o1[2];
                for (int j = 0; j < 2; ++j)
                {
                    curNote = targetNote + (curNote - targetNote) * glideCoef;
                    if (! gate) rel *= relCoef;
                    const double a = ampShape (p, t) * rel * (0.55 + 0.45 * vel);
                    const double hz = 440.0 * std::pow (2.0, (curNote - 69.0 + bend + pitchShape (p, t)) / 12.0);
                    phase += hz * dtO;
                    phase -= std::floor (phase);
                    const double s = std::sin (2.0 * kPi * phase) * a;

                    // noise transient: fast rise, quick decay, high-passed, goes into the drive with the body
                    rng = rng * 1664525u + 1013904223u;
                    const double nz = ((rng >> 8) * (1.0 / 8388608.0) - 1.0);
                    const double ne = t < 0.0015 ? t / 0.0015 : std::exp (-(t - 0.0015) / 0.014);
                    const double noise = noiseHp (nz) * ne * noiseAmt * rel * vel;

                    // drive follows the tail (Serum: LFO 1 -> Dist Drive)
                    const double gNow = gD * (0.5 + 0.5 * ampShape (p, t));
                    const double x = s + noise;
                    const double dirt = x + mix * (asym (x, gNow) - x);

                    // clean sub below 90 Hz + dirt above it
                    const double sub = subLo (s) * subGain;
                    const double hi = dirtHi (dirt);
                    double y = hiCut (sub + hi * (0.8 + 0.4 * p.power));
                    y = dcBlock (y);

                    // output clipper (POWER pushes into it)
                    y = 0.98 * std::tanh (y * clipIn / 0.98);
                    if (std::fabs (sub) > subPeak) subPeak = (float) std::fabs (sub);

                    o1[j] = y;
                    t += dtO;
                }
                d1.push (o1[0]); d1.push (o1[1]);
                o2[k] = d1.out();
            }
            d2a.push (o2[0]); d2a.push (o2[1]);
            const double y = d2a.out() * outGain;
            out[i] = (float) y;
            if (std::fabs (y) > peak) peak = (float) std::fabs (y);
        }
        vizAmp.store ((float) (ampShape (p, t) * rel));
        vizLevel.store (peak);
        vizSub.store (subPeak);
        vizNoteTime.store (t);
    }

private:
    void push (int note)
    {
        remove (note);
        if (stackSize < (int) stack.size()) stack[(size_t) stackSize++] = note;
    }
    void remove (int note)
    {
        int w = 0;
        for (int r = 0; r < stackSize; ++r) if (stack[(size_t) r] != note) stack[(size_t) w++] = stack[(size_t) r];
        stackSize = w;
    }

    Params p;
    double sr = 44100, osr = 176400;
    double phase = 0, t = 99, curNote = 36, targetNote = 36, bend = 0, rel = 0;
    float vel = 1.0f;
    bool gate = false;
    std::array<int, 16> stack {};
    int stackSize = 0;
    uint32_t rng = 12345;
    LR4 subLo, dirtHi;
    Biquad hiCut, noiseHp, dcBlock;
    Decimator2 d1, d2a, d2b;
};

// ---- factory presets (values are knob positions 0..1) ----
struct Preset { const char* name; Params p; };
inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> list = {
        { "THEMATIC 808", { 0.75f, 0.50f, 0.30f, 0.65f, 0.70f, 0.40f, 0.35f, 0.0f } },
        { "DEEP SLIDE",   { 0.45f, 0.30f, 0.15f, 0.55f, 0.85f, 0.20f, 0.70f, 0.0f } },
        { "STREET GUT",   { 0.85f, 0.65f, 0.25f, 0.90f, 0.60f, 0.80f, 0.30f, -2.0f } },
        { "CLEAN SUB",    { 0.30f, 0.20f, 0.10f, 0.35f, 0.75f, 0.00f, 0.40f, 0.0f } },
        { "TRAP PRESS",   { 0.95f, 0.75f, 0.40f, 0.80f, 0.55f, 0.60f, 0.25f, -1.0f } },
        { "BOOM HALL",    { 0.60f, 0.45f, 0.55f, 0.70f, 1.00f, 0.35f, 0.50f, 0.0f } },
    };
    return list;
}
} // namespace bass
