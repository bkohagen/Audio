// DSP tests for __PRODUCT_NAME__. Framework-free; run by ctest (__TARGET___dsp_tests).
// Keep the latency / bypass / mix tests and add tests for your own processing.
#include "PluginDSP.h"

#include <cstdio>
#include <vector>

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); } \
                              else { std::printf ("ok:   " __VA_ARGS__); std::printf ("\n"); } } while (0)

constexpr double fs = 48000.0;
using Dsp = __NS__::Processor;

static std::vector<float> sine (double hz, double ampDb, size_t n)
{
    std::vector<float> v (n);
    const double a = std::pow (10.0, ampDb / 20.0);
    for (size_t i = 0; i < n; ++i) v[i] = (float) (a * std::sin (2.0 * kit::kPi * hz * double (i) / fs));
    return v;
}

static void run (Dsp& d, std::vector<float>& l, std::vector<float>& r, size_t from, size_t count)
{
    float* io[2] = { l.data() + from, r.data() + from };
    d.process (io, 2, (int) count);
}

int main()
{
    const int L = Dsp::getLatencySamples();

    // 1. Latency: an impulse comes out exactly at the reported latency (0 for zero-latency plugins)
    {
        Dsp d; __NS__::Parameters p; d.prepare (fs); d.setParameters (p); d.reset();
        std::vector<float> l (1024, 0.0f), r (1024, 0.0f); l[0] = r[0] = 0.5f;
        run (d, l, r, 0, l.size());
        size_t peakAt = 0; for (size_t i = 0; i < l.size(); ++i) if (std::abs (l[i]) > std::abs (l[peakAt])) peakAt = i;
        CHECK ((int) peakAt == L, "impulse arrives at sample %d == reported latency %d", (int) peakAt, L);
    }

    // 2. Mix 0 % == dry (delayed by the latency), bit exact
    {
        Dsp d; __NS__::Parameters p; p.mix = 0.0f; p.inputDb = 12.0f;
        d.prepare (fs); d.setParameters (p); d.reset();
        auto l = sine (500.0, -6.0, 4800), r = l, ref = l;
        run (d, l, r, 0, l.size());
        double err = 0; for (size_t i = (size_t) L; i < l.size(); ++i) err = std::max (err, (double) std::abs (l[i] - ref[i - (size_t) L]));
        CHECK (err == 0.0, "mix 0%% == dry (max err %.2e)", err);
    }

    // 3. Bypass is click-free and settles to bit-exact (latency-aligned) dry
    {
        Dsp d; __NS__::Parameters p; p.inputDb = 12.0f;
        d.prepare (fs); d.setParameters (p); d.reset();
        const size_t half = (size_t) (0.25 * fs);
        auto l = sine (50.0, -6.0, 2 * half), r = l, ref = l;
        run (d, l, r, 0, half);
        p.bypass = true; d.setParameters (p);
        run (d, l, r, half, half);
        float maxStep = 0; for (size_t i = half - 4; i < half + 960; ++i) maxStep = std::max (maxStep, std::abs (l[i + 1] - l[i]));
        CHECK (maxStep < 0.05f, "bypass switch is click-free (max sample step %.4f)", maxStep);
        double err = 0; for (size_t i = 2 * half - 1000; i < 2 * half; ++i) err = std::max (err, (double) std::abs (l[i] - ref[i - (size_t) L]));
        CHECK (err == 0.0, "bypass settles to bit-exact dry (max err %.2e)", err);
    }

    // 4. Output stays finite with extreme settings
    {
        Dsp d; __NS__::Parameters p; p.inputDb = 24.0f; p.outputDb = 24.0f;
        d.prepare (fs); d.setParameters (p); d.reset();
        auto l = sine (100.0, 0.0, 9600), r = l;
        run (d, l, r, 0, l.size());
        bool finite = true; for (float v : l) finite &= std::isfinite (v);
        CHECK (finite, "output finite at extreme settings");
    }

    std::printf ("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
