// Offline tests for the VBC-1 DSP core. Build & run: ctest (or ./vbc_dsp_tests)
#include "BusCompDSP.h"

#include <cstdio>
#include <vector>

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); } \
                              else { std::printf ("ok:   " __VA_ARGS__); std::printf ("\n"); } } while (0)

constexpr double fs = 48000.0;

static vbc::Parameters cleanParams()
{
    vbc::Parameters p;
    p.analog = 0.0f; p.hpfOn = false; p.mix = 1.0f; p.makeupDb = 0.0f;
    return p;
}

static double rmsDb (const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0; for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i];
    return 10.0 * std::log10 (s / double (to - from) + 1e-30);
}

static std::vector<float> sine (double hz, double ampDb, size_t n)
{
    std::vector<float> v (n);
    const double a = std::pow (10.0, ampDb / 20.0);
    for (size_t i = 0; i < n; ++i) v[i] = (float) (a * std::sin (2.0 * vbc::kPi * hz * double (i) / fs));
    return v;
}

static void runStereo (vbc::BusCompressor& c, std::vector<float>& l, std::vector<float>& r,
                       const float* const* sc = nullptr, int scCh = 0)
{
    float* io[2] = { l.data(), r.data() };
    c.process (io, 2, sc, scCh, (int) l.size());
}

int main()
{
    // 1. Zero latency + transparency below threshold with analog = 0
    {
        vbc::BusCompressor c; auto p = cleanParams(); p.thresholdDb = 0.0f;
        c.prepare (fs); c.setParameters (p); c.reset();
        std::vector<float> l (512, 0.0f), r (512, 0.0f); l[0] = r[0] = 0.25f;
        runStereo (c, l, r);
        CHECK (l[0] == 0.25f && r[0] == 0.25f, "impulse appears at sample 0 (zero latency)");
        double tail = 0; for (size_t i = 1; i < l.size(); ++i) tail += std::abs (l[i]);
        CHECK (tail == 0.0, "linear path is bit-transparent below threshold (tail=%g)", tail);
        CHECK (vbc::BusCompressor::getLatencySamples() == 0, "reported latency is 0");
    }

    // 2. Static curve: hard region well above knee, 4:1, 20 dB over threshold => 15 dB GR
    {
        const double gr = vbc::BusCompressor::gainReductionDb (0.0, -20.0, 4.0, 6.0);
        CHECK (std::abs (gr - 15.0) < 1e-9, "4:1 @ 20 dB over -> 15 dB GR (got %.3f)", gr);
        const double g2 = vbc::BusCompressor::gainReductionDb (-30.0, -20.0, 4.0, 6.0);
        CHECK (g2 == 0.0, "no GR below knee");
    }

    // 3. Steady-state sine compression roughly matches the static curve (peak detector)
    {
        vbc::BusCompressor c; auto p = cleanParams();
        p.thresholdDb = -20.0f; p.ratio = 10.0f; p.attackMs = 0.1f; p.releaseMs = 1200.0f;
        c.prepare (fs); c.setParameters (p); c.reset();
        auto l = sine (1000.0, -6.0, (size_t) fs), r = l;
        runStereo (c, l, r);
        const double outDb = rmsDb (l, (size_t) (0.5 * fs), l.size());
        const double inDb  = -6.0 - 3.0103; // sine RMS
        const double grMeasured = inDb - outDb;
        const double grExpected = vbc::BusCompressor::gainReductionDb (-6.0, -20.0, 10.0, 3.0);
        CHECK (std::abs (grMeasured - grExpected) < 1.0, "10:1 sine GR %.2f dB ~ expected %.2f dB", grMeasured, grExpected);
    }

    // 4. Attack: with 30 ms attack, GR after 30 ms is ~63% of final; with 0.1 ms it's ~100%
    {
        for (float atk : { 0.1f, 30.0f })
        {
            vbc::BusCompressor c; auto p = cleanParams();
            p.thresholdDb = -30.0f; p.ratio = 10.0f; p.attackMs = atk; p.releaseMs = 1200.0f;
            c.prepare (fs); c.setParameters (p); c.reset();
            const int n = (int) (0.030 * fs);
            std::vector<float> l (n, 0.5f), r (n, 0.5f); // DC step, -6 dBFS
            runStereo (c, l, r);
            const double finalGr = vbc::BusCompressor::gainReductionDb (-6.0206, -30.0, 10.0, 3.0);
            const double frac = c.getGainReductionDb() / finalGr;
            if (atk < 1.0f) CHECK (frac > 0.99, "0.1 ms attack: %.1f%% of final GR after 30 ms", frac * 100);
            else            CHECK (std::abs (frac - 0.632) < 0.03, "30 ms attack: %.1f%% of final GR after 30 ms (~63%%)", frac * 100);
        }
    }

    // 5. Auto release is program dependent: release after a long burst is slower than after a short one
    {
        auto releaseAfterBurst = [] (double burstSec)
        {
            vbc::BusCompressor c; auto p = cleanParams();
            p.thresholdDb = -30.0f; p.ratio = 4.0f; p.attackMs = 1.0f; p.releaseMs = 0.0f; // AUTO
            c.prepare (fs); c.setParameters (p); c.reset();
            std::vector<float> l ((size_t) (burstSec * fs), 0.5f), r = l;
            runStereo (c, l, r);
            const float grStart = c.getGainReductionDb();
            std::vector<float> s ((size_t) (0.3 * fs), 0.0f), s2 = s;
            runStereo (c, s, s2);
            float io0 = 0, io1 = 0; float* io[2] = { &io0, &io1 }; c.process (io, 2, nullptr, 0, 1);
            return c.getGainReductionDb() / grStart; // fraction of GR remaining 300 ms later
        };
        const float shortB = releaseAfterBurst (0.02), longB = releaseAfterBurst (3.0);
        CHECK (longB > shortB * 2.0f, "auto release: %.0f%% GR left after long burst vs %.0f%% after transient", longB * 100, shortB * 100);
    }

    // 6. Side-chain HPF: 40 Hz content with HPF at 200 Hz causes much less GR than unfiltered
    {
        auto grFor = [] (bool hpf)
        {
            vbc::BusCompressor c; auto p = cleanParams();
            p.thresholdDb = -20.0f; p.ratio = 4.0f; p.attackMs = 1.0f; p.releaseMs = 1200.0f;
            p.hpfOn = hpf; p.hpfHz = 200.0f;
            c.prepare (fs); c.setParameters (p); c.reset();
            auto l = sine (40.0, -6.0, (size_t) fs), r = l;
            runStereo (c, l, r);
            return c.getGainReductionDb();
        };
        const float off = grFor (false), on = grFor (true);
        CHECK (off > 8.0f && on < off - 6.0f, "SC HPF 200 Hz on 40 Hz tone: GR %.1f dB (off) -> %.1f dB (on)", off, on);
    }

    // 7. External side-chain: silent main input keyed by loud SC
    {
        vbc::BusCompressor c; auto p = cleanParams();
        p.thresholdDb = -20.0f; p.ratio = 10.0f; p.attackMs = 0.1f; p.externalSc = true;
        c.prepare (fs); c.setParameters (p); c.reset();
        auto key = sine (1000.0, 0.0, 4800);
        std::vector<float> l (4800, 0.01f), r = l;
        const float* sc[2] = { key.data(), key.data() };
        runStereo (c, l, r, sc, 2);
        CHECK (c.getGainReductionDb() > 10.0f, "external key drives GR (%.1f dB) on quiet input", c.getGainReductionDb());
    }

    // 8. Analog colour: adds harmonics, stays finite, small DC
    {
        vbc::BusCompressor c; auto p = cleanParams();
        p.thresholdDb = -20.0f; p.ratio = 4.0f; p.analog = 1.0f; p.makeupDb = 10.0f;
        c.prepare (fs); c.setParameters (p); c.reset();
        auto l = sine (100.0, -1.0, (size_t) fs), r = l;
        runStereo (c, l, r);
        bool finite = true; double dc = 0; float mx = 0;
        for (size_t i = (size_t) (0.5 * fs); i < l.size(); ++i) { finite &= std::isfinite (l[i]); dc += l[i]; mx = std::max (mx, std::abs (l[i])); }
        dc /= (0.5 * fs);
        // 2nd harmonic magnitude via single-bin DFT
        double re = 0, im = 0, re1 = 0, im1 = 0;
        for (size_t i = (size_t) (0.5 * fs); i < l.size(); ++i)
        {
            const double w = 2.0 * vbc::kPi * double (i) / fs;
            re += l[i] * std::cos (200.0 * w); im += l[i] * std::sin (200.0 * w);
            re1 += l[i] * std::cos (100.0 * w); im1 += l[i] * std::sin (100.0 * w);
        }
        const double h2 = 20.0 * std::log10 (std::sqrt (re * re + im * im) / std::sqrt (re1 * re1 + im1 * im1));
        CHECK (finite && mx < 4.0f, "analog stage output finite and bounded (peak %.2f)", mx);
        CHECK (h2 > -80.0 && h2 < -20.0, "analog stage adds 2nd harmonic at %.1f dBc", h2);
        CHECK (std::abs (dc) < 1e-3, "negligible DC offset (%.2e)", dc);
    }

    // 9. Mix = 0 returns the dry signal exactly
    {
        vbc::BusCompressor c; auto p = cleanParams();
        p.thresholdDb = -30.0f; p.ratio = 10.0f; p.mix = 0.0f; p.analog = 1.0f;
        c.prepare (fs); c.setParameters (p); c.reset();
        auto l = sine (500.0, -3.0, 4800), r = l, ref = l;
        runStereo (c, l, r);
        double err = 0; for (size_t i = 0; i < l.size(); ++i) err = std::max (err, (double) std::abs (l[i] - ref[i]));
        CHECK (err < 1e-6, "mix 0%% == dry (max err %.2e)", err);
    }

    // 10. Bypass and Listen switch without clicks (crossfaded), and bypass ends exactly dry
    {
        auto maxStep = [] (const std::vector<float>& v, size_t from, size_t to)
        {
            float m = 0; for (size_t i = from + 1; i < to; ++i) m = std::max (m, std::abs (v[i] - v[i - 1])); return m;
        };
        for (int which = 0; which < 2; ++which)
        {
            vbc::BusCompressor c; auto p = cleanParams();
            p.thresholdDb = -30.0f; p.ratio = 10.0f; p.makeupDb = 12.0f; p.analog = 0.5f; p.hpfOn = true; p.hpfHz = 400.0f;
            c.prepare (fs); c.setParameters (p); c.reset();
            const size_t half = (size_t) (0.25 * fs);
            auto l = sine (50.0, -3.0, 2 * half), r = l, ref = l; // low tone: worst case for HPF'd listen
            float* io1[2] = { l.data(), r.data() };
            c.process (io1, 2, nullptr, 0, (int) half);
            if (which == 0) p.bypass = true; else p.scListen = true;
            c.setParameters (p);
            float* io2[2] = { l.data() + half, r.data() + half };
            c.process (io2, 2, nullptr, 0, (int) half);
            // The 50 Hz sine changes by at most ~0.0047/sample; a hard switch here would jump by ~0.3+
            const float stepAtSwitch = maxStep (l, half - 4, half + 480);
            const char* name = which == 0 ? "bypass" : "listen";
            CHECK (stepAtSwitch < 0.02f, "%s switch is click-free (max sample step %.4f)", name, stepAtSwitch);
            if (which == 0)
            {
                double err = 0; for (size_t i = 2 * half - 1000; i < 2 * half; ++i) err = std::max (err, (double) std::abs (l[i] - ref[i]));
                CHECK (err == 0.0, "bypass settles to bit-exact dry (max err %.2e)", err);
            }
        }
    }

    std::printf ("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
