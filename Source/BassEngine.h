// 808 BASS MAKER - header-only DSP engine (no JUCE), so it can be unit-tested with plain g++.
//
// Built on the principle of Thematic's Serum 808 patch:
//   sine (OCT -2) + three pitch envelopes of different speed (fast CLICK, 1/4-ish PUNCH, slow DROP over the tail)
//   + linear volume tail (LENGTH) + NOISE into the drive + odd-harmonic FM + ASYM distortion whose drive falls with the tail
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
    float glide  = 0.0f;    // legacy (GLIDE knob removed in 0.11): kept at 0
    float output = 0.0f;    // output gain, dB
    float fm     = 0.0f;    // odd-harmonic FM squeak in the low-mids (0..1)
    float noise  = 0.0f;    // noise into the drive (0..1)
    float noiseTone = 0.55f; // dark .. bright
    float noiseTail = 0.35f; // only the attack .. the whole tail
    int   noiseType = 0;     // see noiseNames()
    int   distType  = 0;     // see distNames()
    float pitch     = 0.0f;  // global pitch, semitones (-24..+24). 0 = two octaves below the played note
    bool  adsrOn    = false; // ADSR amp (as in GRAIN FORM). Off = the 808 LENGTH tail
    float attackMs  = 1.0f, decayMs = 450.0f, sustain = 0.7f, releaseMs = 150.0f;
};

constexpr double kBaseOctaveShift = -24.0;   // since 0.13 the plugin plays two octaves lower

inline const char* const* distNames()
{
    static const char* const n[] = { "ASYM", "TUBE", "HARD", "FOLD", "RECTIFY" };
    return n;
}
constexpr int kNumDists = 5;

inline const char* const* noiseNames()
{
    static const char* const n[] = { "WHITE", "PINK", "BROWN", "VINYL", "TAPE", "HUM", "DIRT", "DUST" };
    return n;
}
constexpr int kNumNoises = 8;

// FM squeak: carrier phase th, modulator at 2x -> only odd harmonics (1, 3, 5, 7 ...)
inline double fmIndex (const Params& p, double amp) { return (0.5 + 3.3 * p.fm) * (0.45 + 0.55 * amp); }
inline double fmPart (double th, double index) { return std::sin (th + index * std::sin (2.0 * th)) - std::sin (th); }

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

// ADSR level (no release) at time t
inline double adsrHold (const Params& p, double t)
{
    const double a = std::fmax (0.0001, p.attackMs * 0.001), d = std::fmax (0.001, p.decayMs * 0.001);
    if (t < a) return t / a;
    return p.sustain + (1.0 - p.sustain) * std::exp (-4.6 * (t - a) / d);
}

// the volume shape the screen draws: LENGTH tail, or ADSR held for 65% of the window then released
inline double envShape (const Params& p, double t, double window)
{
    if (! p.adsrOn) return ampShape (p, t);
    const double off = window * 0.65;
    if (t < off) return adsrHold (p, t);
    return adsrHold (p, off) * std::exp (-6.9 * (t - off) / std::fmax (0.005, p.releaseMs * 0.001));
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

// all drive algorithms: 0 at 0, roughly +/-1 at full scale
inline double driveShape (int type, double x, double g)
{
    switch (type)
    {
        default:
        case 0: return asym (x, g);                                                     // ASYM: the Serum patch
        case 1: { const double v = g * x + 0.15 * g * x * x;                            // TUBE: soft, warm, a bit uneven
                  return std::tanh (v) / std::tanh (g + 0.15 * g); }
        case 2: { const double v = g * x * 1.4;                                         // HARD: flat tops with a tiny knee
                  return v / std::pow (1.0 + std::pow (std::fabs (v), 8.0), 0.125); }
        case 3: return std::sin (x * (0.6 + 0.55 * g) * kPi * 0.5);                    // FOLD (as in GRAIN FORM): folds the wave back
        case 4: { const double v = 0.45 * x + 0.9 * std::fabs (x) - 0.35 * std::fabs (x) * std::fabs (x);  // RECTIFY: octave up, nasty
                  return std::tanh (g * v) / std::tanh (g); }
    }
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
        const double s = std::sin (ph) + 0.45 * p.fm * fmPart (ph, fmIndex (p, a));
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


// Built-in noises (generated, no files). Each one is roughly -9 dB RMS.
struct NoiseGen
{
    uint32_t rng = 0x9e3779b9u;
    double b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, brown = 0;
    double crack = 0, crackEnv = 0, held = 0, holdCount = 0, humPh = 0, wob = 0, hissLp = 0, hissHp = 0;
    double white()
    {
        rng = rng * 1664525u + 1013904223u;
        return (rng >> 8) * (1.0 / 8388608.0) - 1.0;
    }
    double next (int type, double osr)
    {
        const double w = white();
        switch (type)
        {
            default:
            case 0: return w * 0.6;
            case 1:   // pink (Paul Kellet)
                b0 = 0.99886 * b0 + w * 0.0555179; b1 = 0.99332 * b1 + w * 0.0750759; b2 = 0.96900 * b2 + w * 0.1538520;
                b3 = 0.86650 * b3 + w * 0.3104856; b4 = 0.55000 * b4 + w * 0.5329522; b5 = -0.7616 * b5 - w * 0.0168980;
                { const double pk = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362; b6 = w * 0.115926; return pk * 0.12; }
            case 2:   // brown: deep rumble
                brown = (brown + 0.02 * w) / 1.0015;
                return std::fmax (-1.0, std::fmin (1.0, brown * 2.2));
            case 3:   // vinyl: soft bed + crackles
                if (white() > 1.0 - 2.0 * 45.0 / osr) { crack = white() > 0 ? 1.0 : -1.0; crackEnv = 0.5 + 0.5 * std::fabs (white()); }
                crackEnv *= std::exp (-1.0 / (0.0007 * osr));
                hissLp += (w - hissLp) * 0.08;
                return hissLp * 0.5 + crack * crackEnv * std::fabs (w) * 1.6;
            case 4:   // tape: band hiss with a slow wobble
                hissHp += (w - hissHp) * 0.06;
                hissLp += ((w - hissHp) - hissLp) * 0.35;
                wob += 0.9 / osr;
                return hissLp * (0.85 + 0.15 * std::sin (2.0 * kPi * wob)) * 1.4;
            case 5:   // hum: mains buzz (odd harmonics of 50 Hz) + a little air
                humPh += 50.0 / osr; humPh -= std::floor (humPh);
                {
                    const double th = 2.0 * kPi * humPh;
                    return 0.42 * std::sin (th) + 0.32 * std::sin (3 * th) + 0.22 * std::sin (5 * th) + 0.14 * std::sin (7 * th)
                         + 0.09 * std::sin (9 * th) + w * 0.06;
                }
            case 6:   // dirt: stepped sample & hold grit
                if ((holdCount -= 1.0) <= 0) { held = w; holdCount = osr / (2200.0 + 1800.0 * std::fabs (white())); }
                return held * 0.55;
            case 7:   // dust: sparse clicks
                if (white() > 1.0 - 2.0 * 70.0 / osr) { crack = white(); crackEnv = 1.0; }
                crackEnv *= std::exp (-1.0 / (0.00025 * osr));
                return crack * crackEnv * 1.8;
        }
    }
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
    std::atomic<int> vizHits { 0 };
    std::atomic<int> vizAdsrStage { 0 };     // 0 idle, 1 attack, 2 decay/sustain, 3 release          // increments on every note start
    std::atomic<double> vizNoteTime { 99.0 };  // seconds since the last note start

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        osr = sr * OS;
        subLo.setLow (osr, 90.0);
        dirtHi.setHigh (osr, 90.0);
        hiCut.lowpass (osr, 9000.0, 0.6);
        noiseHp.highpass (osr, 45.0, 0.7);
        fmHp.highpass (osr, 160.0, 0.7071);
        fmLp1.lowpass (osr, 1600.0, 0.7071);
        fmLp2.lowpass (osr, 1600.0, 0.7071);
        dcBlock.highpass (osr, 18.0, 0.7);
        reset();
    }

    void reset()
    {
        subLo.reset(); dirtHi.reset(); hiCut.reset(); noiseHp.reset(); dcBlock.reset(); fmHp.reset(); fmLp1.reset(); fmLp2.reset(); noiseLp = 0;
        d1.reset(); d2a.reset(); d2b.reset();
        stackSize = 0; gate = false; rel = 0; phase = 0; t = 99; curNote = targetNote = 36; adsrStage = 0; adsrLevel = 0;
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
        adsrStage = 1;   // attack from the current level: no click on retrigger
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
        const double atkInc = dtO / std::fmax (0.0001, p.attackMs * 0.001);
        const double decCoef = std::exp (-4.6 * dtO / std::fmax (0.001, p.decayMs * 0.001));
        const double adsrRelCoef = std::exp (-6.9 * dtO / std::fmax (0.005, p.releaseMs * 0.001));
        const double sus = std::fmin (1.0, std::fmax (0.0, (double) p.sustain));
        const double gD = driveGain (p), mix = driveMix (p);
        const double subGain = 1.0 + 0.5 * p.power;
        const double clipIn = std::pow (10.0, (p.power * 8.0) / 20.0);
        const double outGain = std::pow (10.0, p.output / 20.0) * 0.82;
        const double noiseAmt = p.noise * p.noise * 0.55;
        const double toneHz = 500.0 * std::pow (36.0, (double) p.noiseTone);
        const double toneK = 1.0 - std::exp (-2.0 * kPi * toneHz / osr);
        const double fmGain = p.fm * 1.15;
        const int dType = p.distType < 0 ? 0 : (p.distType >= kNumDists ? kNumDists - 1 : p.distType);
        const int nType = p.noiseType < 0 ? 0 : (p.noiseType >= kNumNoises ? kNumNoises - 1 : p.noiseType);

        for (int i = 0; i < n; ++i)
        {
            double o2[2];
            for (int k = 0; k < 2; ++k)
            {
                double o1[2];
                for (int j = 0; j < 2; ++j)
                {
                    curNote = targetNote + (curNote - targetNote) * glideCoef;
                    double env;
                    if (p.adsrOn)
                    {
                        // ADSR amp (GRAIN FORM): linear attack, exponential decay and release
                        if (! gate && adsrStage != 0) adsrStage = 3;
                        if (adsrStage == 1)      { adsrLevel += atkInc; if (adsrLevel >= 1.0) { adsrLevel = 1.0; adsrStage = 2; } }
                        else if (adsrStage == 2) adsrLevel = sus + (adsrLevel - sus) * decCoef;
                        else if (adsrStage == 3) adsrLevel *= adsrRelCoef;
                        env = adsrLevel;
                        rel = 1.0;
                    }
                    else
                    {
                        if (! gate) rel *= relCoef;
                        env = ampShape (p, t) * rel;
                        adsrLevel = env;
                    }
                    const double a = env * (0.55 + 0.45 * vel);
                    const double hz = 440.0 * std::pow (2.0, (curNote - 69.0 + kBaseOctaveShift + p.pitch + bend + pitchShape (p, t)) / 12.0);
                    phase += hz * dtO;
                    phase -= std::floor (phase);
                    const double th = 2.0 * kPi * phase;
                    const double unit = std::sin (th);
                    const double s = unit * a;
                    const double shape = p.adsrOn ? env : ampShape (p, t);

                    // NOISE goes INTO the drive with the sine: it gets crushed into crunch instead of hissing on top.
                    // It breathes with the wave (louder on the crests) and with the tail (Serum: LFO 5 -> Noise).
                    double noise = 0.0;
                    if (noiseAmt > 0.0)
                    {
                        noiseLp += (gen.next (nType, osr) - noiseLp) * toneK;
                        const double burst = t < 0.003 ? t / 0.003 : std::exp (-(t - 0.003) / 0.035);
                        const double env = std::fmax (burst, p.noiseTail * shape) * rel * vel;
                        noise = noiseHp (noiseLp) * env * (0.3 + 0.7 * std::fabs (unit)) * noiseAmt;
                    }

                    // FM squeak: odd harmonics only, kept in the low-mids
                    double fmx = 0.0;
                    if (fmGain > 0.0)
                        fmx = fmLp2 (fmLp1 (fmHp (fmPart (th, fmIndex (p, shape))))) * a * fmGain;

                    // drive follows the tail (Serum: LFO 1 -> Dist Drive)
                    const double gNow = gD * (0.5 + 0.5 * shape);
                    const double x = s + noise + fmx;
                    const double dirt = x + mix * (driveShape (dType, x, gNow) - x);

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
        peakAcc = std::fmax (peakAcc, peak);
        subAcc = std::fmax (subAcc, subPeak);
        vizAmp.store ((float) (p.adsrOn ? adsrLevel : ampShape (p, t) * rel));
        vizAdsrStage.store (adsrLevel < 0.001 && adsrStage == 3 ? 0 : adsrStage);
        vizNoteTime.store (t);
    }

    // call once per host block: hands the block's peaks to the UI
    void publishViz()
    {
        vizLevel.store (peakAcc);
        vizSub.store (subAcc);
        peakAcc = subAcc = 0.0f;
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
    int adsrStage = 0;        // 0 idle, 1 attack, 2 decay/sustain, 3 release
    double adsrLevel = 0.0;
    std::array<int, 16> stack {};
    int stackSize = 0;
    uint32_t rng = 12345;
    LR4 subLo, dirtHi;
    Biquad hiCut, noiseHp, dcBlock, fmHp, fmLp1, fmLp2;
    NoiseGen gen;
    float peakAcc = 0, subAcc = 0;
    double noiseLp = 0;
    Decimator2 d1, d2a, d2b;
};

// ---- factory presets (values are knob positions 0..1) ----
struct Preset { const char* name; Params p; };
inline const std::vector<Preset>& factoryPresets()
{
    static const std::vector<Preset> list = {
        { "THEMATIC 808", { 0.75f, 0.50f, 0.30f, 0.65f, 0.70f, 0.40f, 0.0f, 0.0f,  0.00f, 0.00f, 0.55f, 0.35f, 0 } },
        { "DEEP SLIDE",   { 0.45f, 0.30f, 0.15f, 0.55f, 0.85f, 0.20f, 0.0f, 0.0f,  0.10f, 0.20f, 0.35f, 0.20f, 2 } },
        { "STREET GUT",   { 0.85f, 0.65f, 0.25f, 0.90f, 0.60f, 0.80f, 0.0f, -2.0f, 0.25f, 0.45f, 0.55f, 0.60f, 1 } },
        { "CLEAN SUB",    { 0.30f, 0.20f, 0.10f, 0.35f, 0.75f, 0.00f, 0.0f, 0.0f,  0.00f, 0.00f, 0.55f, 0.35f, 0 } },
        { "TRAP PRESS",   { 0.95f, 0.75f, 0.40f, 0.80f, 0.55f, 0.60f, 0.0f, -1.0f, 0.15f, 0.30f, 0.70f, 0.25f, 3 } },
        { "BOOM HALL",    { 0.60f, 0.45f, 0.55f, 0.70f, 1.00f, 0.35f, 0.0f, 0.0f,  0.05f, 0.25f, 0.40f, 0.80f, 4 } },
        { "SQUEAK",       { 0.70f, 0.45f, 0.20f, 0.70f, 0.70f, 0.45f, 0.0f, -1.0f, 0.65f, 0.15f, 0.50f, 0.30f, 0 } },
        { "CRUNCH",       { 0.80f, 0.55f, 0.25f, 0.75f, 0.65f, 0.65f, 0.0f, -1.0f, 0.20f, 0.70f, 0.60f, 0.70f, 6 } },
        { "VINYL KNOCK",  { 0.65f, 0.40f, 0.30f, 0.60f, 0.60f, 0.50f, 0.0f, 0.0f,  0.10f, 0.55f, 0.45f, 0.45f, 3 } },
    };
    return list;
}
} // namespace bass
