// DSP tests for TPE-542 Tape Emulator. Framework-free; run by ctest (TPE542_dsp_tests).

#include "PluginDSP.h"

#include <cstdio>
#include <vector>

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); } \
                              else { std::printf ("ok:   " __VA_ARGS__); std::printf ("\n"); } } while (0)

constexpr double fs = 48000.0;
using Dsp = tpe542::Processor;

static std::vector<float> sine (double hz, double ampDb, size_t n)
{
    std::vector<float> v (n);
    const double a = std::pow (10.0, ampDb / 20.0);
    for (size_t i = 0; i < n; ++i) v[i] = (float) (a * std::sin (2.0 * kit::kPi * hz * double (i) / fs));
    return v;
}

/** Amplitude (dB) of the component at hz over the second half of v (single-bin DFT). */
static double bin (const std::vector<float>& v, double hz)
{
    const size_t start = v.size() / 2, n = v.size() - start;
    double re = 0, im = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * kit::kPi * double (i) / double (n)); // Hann
        const double ph = 2.0 * kit::kPi * hz * double (start + i) / fs;
        re += w * v[start + i] * std::cos (ph);
        im += w * v[start + i] * std::sin (ph);
    }
    return 20.0 * std::log10 (std::max (1e-12, 4.0 * std::sqrt (re * re + im * im) / double (n)));
}

/** Process a mono-in-stereo sine through fresh DSP with p; returns the left output. */
static std::vector<float> render (const tpe542::Parameters& p, double hz, double ampDb, size_t n = 48000)
{
    Dsp d; d.prepare (fs); d.setParameters (p); d.reset();
    auto l = sine (hz, ampDb, n), r = l;
    float* io[2] = { l.data(), r.data() };
    d.process (io, 2, (int) n);
    return l;
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
        Dsp d; tpe542::Parameters p; d.prepare (fs); d.setParameters (p); d.reset();
        std::vector<float> l (1024, 0.0f), r (1024, 0.0f); l[0] = r[0] = 0.5f;
        run (d, l, r, 0, l.size());
        size_t peakAt = 0; for (size_t i = 0; i < l.size(); ++i) if (std::abs (l[i]) > std::abs (l[peakAt])) peakAt = i;
        CHECK ((int) peakAt == L, "impulse arrives at sample %d == reported latency %d", (int) peakAt, L);
    }

    // 2. Mix 0 % == dry (delayed by the latency), bit exact
    {
        Dsp d; tpe542::Parameters p; p.blend = 0.0f; p.saturation = 10.0f; p.silk = 1; p.texture = 10.0f;
        d.prepare (fs); d.setParameters (p); d.reset();
        auto l = sine (500.0, -6.0, 4800), r = l, ref = l;
        run (d, l, r, 0, l.size());
        double err = 0; for (size_t i = (size_t) L; i < l.size(); ++i) err = std::max (err, (double) std::abs (l[i] - ref[i - (size_t) L]));
        CHECK (err == 0.0, "blend 0%% == dry (max err %.2e)", err);
    }

    // 3. Bypass is click-free and settles to bit-exact (latency-aligned) dry
    {
        Dsp d; tpe542::Parameters p; p.trimDb = 12.0f; p.saturation = 10.0f; p.silk = 2; p.texture = 10.0f;
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
        Dsp d; tpe542::Parameters p; p.trimDb = 12.0f; p.outputDb = 12.0f; p.saturation = 10.0f; p.silk = 1; p.texture = 10.0f;
        d.prepare (fs); d.setParameters (p); d.reset();
        auto l = sine (100.0, 0.0, 9600), r = l;
        run (d, l, r, 0, l.size());
        bool finite = true; for (float v : l) finite &= std::isfinite (v);
        CHECK (finite, "output finite at extreme settings");
    }


    // 5. Saturation 0 + silk off is exactly linear (superposition: 2x input -> 2x output)
    {
        tpe542::Parameters p; p.saturation = 0.0f;
        auto x1 = sine (1000.0, -12.0, 48000), x2 = x1;
        for (auto& v : x2) v *= 2.0f;
        auto y1 = x1, y2 = x2;
        Dsp d1; d1.prepare (fs); d1.setParameters (p); d1.reset();
        Dsp d2; d2.prepare (fs); d2.setParameters (p); d2.reset();
        { float* io[1] = { y1.data() }; d1.process (io, 1, (int) y1.size()); }
        { float* io[1] = { y2.data() }; d2.process (io, 1, (int) y2.size()); }
        double err = 0; for (size_t i = 0; i < y1.size(); ++i) err = std::max (err, (double) std::abs (y2[i] - 2.0f * y1[i]));
        CHECK (err < 1e-5, "saturation 0 + silk off is linear (superposition err %.2e)", err);
        CHECK (bin (y1, 3000.0) - bin (y1, 1000.0) < -100.0, "saturation 0: no 3rd harmonic (%.1f dBc)", bin (y1, 3000.0) - bin (y1, 1000.0));
    }

    // 6. Saturation adds harmonics, more as it is turned up; asymmetry gives 2nd harmonic
    {
        tpe542::Parameters lo; lo.saturation = 3.0f;
        tpe542::Parameters hi; hi.saturation = 9.0f;
        auto a = render (lo, 200.0, -6.0), b = render (hi, 200.0, -6.0);
        const double h3a = bin (a, 600.0) - bin (a, 200.0), h3b = bin (b, 600.0) - bin (b, 200.0);
        const double h2b = bin (b, 400.0) - bin (b, 200.0);
        CHECK (h3b > h3a + 6.0, "3rd harmonic rises with saturation (%.1f -> %.1f dBc)", h3a, h3b);
        CHECK (h3b > -40.0 && h3b < -18.0, "heavy saturation 3rd harmonic in a tape-like range (%.1f dBc)", h3b);
        CHECK (h2b > -50.0 && h2b < h3b, "asymmetric curve produces 2nd harmonic (%.1f dBc)", h2b);
    }

    // 7. Saturation is level-compensated and compresses loud signals
    {
        tpe542::Parameters p; p.saturation = 10.0f;
        auto quiet = render (p, 1000.0, -40.0), loud = render (p, 1000.0, -3.0);
        const double gq = bin (quiet, 1000.0) + 40.0, gl = bin (loud, 1000.0) + 3.0;
        CHECK (std::abs (gq) < 1.0, "small-signal gain ~unity at max saturation (%.2f dB)", gq);
        CHECK (gl < gq - 1.5, "loud signals are compressed (gain %.2f dB vs %.2f dB)", gl, gq);
        Dsp d; d.prepare (fs); d.setParameters (p); d.reset();
        auto l = sine (1000.0, -3.0, 4800), r = l; float* io[2] = { l.data(), r.data() }; d.process (io, 2, 4800);
        CHECK (d.getSaturationDb() > 1.0f, "saturation meter reads %.2f dB", d.getSaturationDb());
    }

    // 8. HF saturates earlier than LF (pre-emphasis around the nonlinearity)
    {
        tpe542::Parameters p; p.saturation = 7.0f;
        auto lf = render (p, 300.0, -10.0), hf = render (p, 5000.0, -10.0);
        const double tl = bin (lf, 900.0) - bin (lf, 300.0), th = bin (hf, 15000.0) - bin (hf, 5000.0);
        CHECK (th > tl, "5 kHz distorts more than 300 Hz at 15 IPS (%.1f vs %.1f dBc)", th, tl);
    }

    // 9. Speed EQ: head bump and top end (low level, saturation 0)
    {
        tpe542::Parameters s15; s15.saturation = 0.0f; s15.speed = 0;
        tpe542::Parameters s30 = s15; s30.speed = 1;
        auto g = [] (const tpe542::Parameters& p, double hz) { auto v = render (p, hz, -30.0, 96000); return bin (v, hz) + 30.0; };
        const double b15 = g (s15, 50.0) - g (s15, 1000.0), b30 = g (s30, 90.0) - g (s30, 1000.0);
        CHECK (b15 > 1.5 && b15 < 3.5, "15 IPS head bump at 50 Hz: +%.2f dB", b15);
        CHECK (b30 > 1.0 && b30 < 2.5, "30 IPS head bump at 90 Hz: +%.2f dB", b30);
        CHECK (g (s15, 50.0) > g (s30, 50.0), "15 IPS has more low-end bump at 50 Hz than 30 IPS");
        const double t15 = g (s15, 16000.0), t30 = g (s30, 16000.0);
        CHECK (t30 > t15 + 1.0, "30 IPS has more top end at 16 kHz (%.2f vs %.2f dB)", t30, t15);
        CHECK (std::abs (g (s15, 1000.0)) < 0.5 && std::abs (g (s30, 1000.0)) < 0.5, "midband is flat at both speeds");
        CHECK (g (s15, 10.0) < -6.0, "subsonic low cut (10 Hz: %.1f dB)", g (s15, 10.0));
    }

    // 10. Silk: RED colours the highs, BLUE colours the lows
    {
        tpe542::Parameters off; off.saturation = 0.0f;
        tpe542::Parameters red = off;  red.silk = 1;  red.texture = 10.0f;
        tpe542::Parameters blue = off; blue.silk = 2; blue.texture = 10.0f;
        auto h = [] (const tpe542::Parameters& p, double hz) { auto v = render (p, hz, -6.0); return bin (v, 2 * hz) - bin (v, hz); };
        CHECK (h (red, 4000.0) > h (blue, 4000.0) + 10.0, "RED adds more HF harmonics (%.1f vs %.1f dBc)", h (red, 4000.0), h (blue, 4000.0));
        CHECK (h (blue, 80.0) > h (red, 80.0) + 10.0, "BLUE adds more LF harmonics (%.1f vs %.1f dBc)", h (blue, 80.0), h (red, 80.0));
        CHECK (h (red, 4000.0) > -50.0 && h (blue, 80.0) > -50.0 && h (red, 4000.0) < -25.0, "silk harmonics subtle but audible at texture 10 (%.1f dBc)", h (red, 4000.0));
        tpe542::Parameters redLow = red; redLow.texture = 2.0f;
        CHECK (h (red, 4000.0) > h (redLow, 4000.0) + 6.0, "TEXTURE scales the silk harmonics");
    }

    // 11. Speed and silk switches are click-free; no DC; mono works
    {
        Dsp d; tpe542::Parameters p; p.saturation = 6.0f;
        d.prepare (fs); d.setParameters (p); d.reset();
        const size_t half = 24000;
        auto l = sine (50.0, -6.0, 2 * half), r = l;
        run (d, l, r, 0, half);
        p.speed = 1; p.silk = 2; p.texture = 10.0f; d.setParameters (p);
        run (d, l, r, half, half);
        float maxStep = 0; for (size_t i = half - 4; i < half + 2000; ++i) maxStep = std::max (maxStep, std::abs (l[i + 1] - l[i]));
        CHECK (maxStep < 0.05f, "speed/silk switch is click-free (max sample step %.4f)", maxStep);

        double mean = 0; for (size_t i = 2 * half - 9600; i < 2 * half; ++i) mean += l[i]; mean /= 9600.0; // last 10 periods
        CHECK (std::abs (mean) < 1e-3, "no DC offset (%.2e)", mean);

        Dsp m; m.prepare (fs); m.setParameters (p); m.reset();
        auto mono = sine (440.0, -6.0, 4800); float* io[1] = { mono.data() }; m.process (io, 1, 4800);
        bool finite = true; for (float v : mono) finite &= std::isfinite (v);
        CHECK (finite && bin (mono, 440.0) > -12.0, "mono processing works");
    }

    std::printf ("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
