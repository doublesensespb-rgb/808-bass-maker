// g++ -std=c++17 -O2 -ISource test/engine_test.cpp -o engine_test && ./engine_test
#include "BassEngine.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); ++fails; } } while (0)

static std::vector<float> renderNote (bass::Params p, int note, double seconds, double sr = 48000, double holdFrac = 1.0)
{
    bass::Engine e;
    e.prepare (sr);
    e.setParams (p);
    std::vector<float> out ((size_t) (seconds * sr));
    e.noteOn (note, 1.0f);
    const int block = 256;
    const size_t off = (size_t) (holdFrac * out.size());
    for (size_t i = 0; i < out.size(); i += block)
    {
        if (i >= off && i < off + block) e.noteOff (note);
        e.render (out.data() + i, (int) std::min<size_t> (block, out.size() - i));
    }
    return out;
}

// estimated frequency from zero crossings in [a,b) seconds
static double freqAt (const std::vector<float>& x, double sr, double a, double b)
{
    int z = 0; double first = -1, last = -1;
    for (size_t i = (size_t) (a * sr) + 1; i < (size_t) (b * sr) && i < x.size(); ++i)
        if (x[i - 1] < 0 && x[i] >= 0) { double tt = i / sr; if (first < 0) first = tt; last = tt; ++z; }
    return z > 1 ? (z - 1) / (last - first) : 0;
}

int main()
{
    const double sr = 48000;
    for (const auto& pr : bass::factoryPresets())
    {
        auto x = renderNote (pr.p, 36, 3.0, sr);
        float peak = 0; bool bad = false;
        for (float v : x) { if (! std::isfinite (v)) bad = true; peak = std::fmax (peak, std::fabs (v)); }
        CHECK (! bad, "%s: NaN/inf", pr.name);
        CHECK (peak < 1.0f && peak > 0.3f, "%s: peak %.3f", pr.name, peak);
        std::printf ("%-14s peak %.3f  f(0-10ms) %.0f Hz  f(300-600ms) %.1f Hz\n", pr.name, peak,
                     freqAt (x, sr, 0.0, 0.010), freqAt (x, sr, 0.3, 0.6));
    }

    // pitch settles near the note (C1 = 32.7 Hz) after the envelopes, with DROP off
    bass::Params p; p.drop = 0; p.drive = 0;
    auto x = renderNote (p, 24, 2.0, sr);
    const double f = freqAt (x, sr, 0.6, 1.1);
    CHECK (std::fabs (f - 32.70) < 1.0, "settled pitch %.2f Hz", f);
    // click is much higher than the note
    CHECK (freqAt (x, sr, 0.0, 0.008) > 80, "click too low");

    // release: silence after note off
    auto y = renderNote (bass::Params(), 36, 1.0, sr, 0.3);
    float tail = 0; for (size_t i = (size_t) (0.4 * sr); i < y.size(); ++i) tail = std::fmax (tail, std::fabs (y[i]));
    CHECK (tail < 0.01f, "release tail %.4f", tail);

    // glide: overlapping note moves pitch smoothly (no instant jump)
    {
        bass::Engine e; e.prepare (sr); bass::Params g; g.glide = 0.8f; g.click = 0; g.punch = 0; g.drop = 0; g.drive = 0;
        e.setParams (g);
        std::vector<float> o ((size_t) sr);
        e.noteOn (36, 1); e.render (o.data(), 24000);
        e.noteOn (48, 1); e.render (o.data() + 24000, 24000);
        const double fa = freqAt (o, sr, 0.30, 0.48), fb = freqAt (o, sr, 0.52, 0.56), fc = freqAt (o, sr, 0.85, 0.99);
        std::printf ("glide %.1f -> %.1f -> %.1f Hz\n", fa, fb, fc);
        CHECK (fb > fa * 1.05 && fb < fc * 0.97, "glide not smooth");
    }

    std::printf (fails ? "%d FAILED\n" : "all ok\n", fails);
    return fails ? 1 : 0;
}
