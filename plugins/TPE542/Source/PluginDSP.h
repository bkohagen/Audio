// TPE-542 Tape Emulator — core DSP (framework-independent, header-only).
//
// Models the behaviour of a 500-series transformer-coupled tape-emulation module:
//   * TRIM            input level into the "tape" (-12 .. +12 dB)
//   * SATURATION      drive into an asymmetric soft magnetic-saturation curve. The curve is
//                     level-compensated (unity small-signal gain), so turning it up adds
//                     compression and harmonics rather than volume. 0 = exactly linear.
//                     High frequencies saturate earlier (record pre-emphasis / playback
//                     de-emphasis around the nonlinearity), as on real tape.
//   * SPEED 15 / 30   tape-speed EQ: 15 IPS = bigger, lower head bump and softer top end with
//                     more HF saturation; 30 IPS = tighter, higher bump and an extended top.
//   * SILK OFF/RED/BLUE + TEXTURE   output-stage harmonic colour:
//                     RED  adds harmonics and a little lift in the upper mids / highs,
//                     BLUE adds harmonics and weight in the low end.
//   * BLEND           parallel mix of the trimmed (pre-tape) signal and the tape signal
//   * OUTPUT          output level (-12 .. +12 dB)
//
// REAL-TIME SAFETY: no heap allocation, locks, system calls, exceptions or I/O in process().
// All state is fixed-size members; every switch (speed, silk, blend, bypass) is crossfaded and
// both speed paths always run, so CPU is constant and switching never clicks.
//
// ZERO LATENCY: causal, sample-by-sample, no oversampling and no look-ahead. Because there is no
// oversampling, the saturation is a smooth (C1) low-order curve and the drive range is limited
// to keep aliasing low. Only the distortion *difference* goes through the de-emphasis and DC
// blocker, so the linear part of the tape path is untouched.
#pragma once

#include <kit/dsp/DspBasics.h>

#include <algorithm>
#include <cmath>

namespace tpe542
{

struct Parameters
{
    float trimDb     = 0.0f;   // -12 .. +12 dB
    float saturation = 5.0f;   // 0 .. 10
    int   speed      = 0;      // 0 = 15 IPS, 1 = 30 IPS
    int   silk       = 0;      // 0 = off, 1 = red, 2 = blue
    float texture    = 5.0f;   // 0 .. 10
    float blend      = 1.0f;   // 0 .. 1 (0 = dry, 1 = tape)
    float outputDb   = 0.0f;   // -12 .. +12 dB
    bool  bypass     = false;  // click-free (crossfaded) bypass
};

//==============================================================================
/** RBJ biquad, transposed direct form II, double precision. Coefficients are only computed in
    prepare(), never per block. */
struct Biquad
{
    void reset() noexcept { z1 = z2 = 0.0; }

    float process (float xf) noexcept
    {
        const double x = xf;
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }

    void setIdentity() noexcept { b0 = 1; b1 = b2 = a1 = a2 = 0; }

    void setPeak (double fs, double hz, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = omega (fs, hz);
        const double alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        set (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
    }

    void setHighShelf (double fs, double hz, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = omega (fs, hz);
        const double c = std::cos (w), alpha = std::sin (w) / 2.0 * std::sqrt (2.0); // slope S = 1
        const double sa = 2.0 * std::sqrt (A) * alpha;
        set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
             (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }

    void setHighpass (double fs, double hz, double q) noexcept
    {
        const double w = omega (fs, hz), alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha);
    }

    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

private:
    static double omega (double fs, double hz) noexcept
    {
        return 2.0 * kit::kPi * std::clamp (hz, 1.0, 0.45 * fs) / fs;
    }
    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }
};

/** Smooth saturating curve (rational tanh approximation), C1 at the clip point |v| = 3. */
inline float softClip (float v) noexcept
{
    if (v >=  3.0f) return  1.0f;
    if (v <= -3.0f) return -1.0f;
    const float v2 = v * v;
    return v * (27.0f + v2) / (27.0f + 9.0f * v2);
}

//==============================================================================
class Processor
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kLatencySamples = 0;
    static constexpr int getLatencySamples() noexcept { return kLatencySamples; }

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        trimSm.setup (fs, 20.0);
        outputSm.setup (fs, 20.0);
        blendSm.setup (fs, 20.0);
        bypassSm.setup (fs, 10.0);
        satAmtSm.setup (fs, 30.0);
        driveSm.setup (fs, 30.0);
        speedSm.setup (fs, 30.0);
        redSm.setup (fs, 30.0);
        blueSm.setup (fs, 30.0);

        for (int ch = 0; ch < kMaxChannels; ++ch)
        {
            for (int s = 0; s < 2; ++s)
            {
                auto& t = tape[s][ch];
                const bool slow = (s == 0);   // 15 IPS
                // record pre-emphasis / playback de-emphasis around the nonlinearity only
                t.preEmph .setHighShelf (fs, slow ? 3000.0 : 6000.0, slow ?  6.0 :  4.0);
                t.deEmph  .setHighShelf (fs, slow ? 3000.0 : 6000.0, slow ? -6.0 : -4.0);
                // playback / speed EQ (linear)
                t.lowCut  .setHighpass  (fs, slow ? 20.0 : 25.0, 0.6);
                t.bump    .setPeak      (fs, slow ? 50.0 : 90.0, slow ? 1.0 : 1.1, slow ? 2.5 : 1.8);
                t.dip     .setPeak      (fs, slow ? 115.0 : 200.0, 1.2, slow ? -1.0 : -0.7);
                t.top     .setHighShelf (fs, slow ? 10000.0 : 16000.0, slow ? -2.0 : 0.3);
                t.dc.setup (fs, 5.0);
            }
            redBand[ch] .f.setup (fs, 2200.0);
            blueBand[ch].setup (fs, 220.0);
            silkDc[ch].setup (fs, 5.0);
        }
        reset();
    }

    void reset() noexcept
    {
        trimSm.snap (kit::dbToGain (params.trimDb));
        outputSm.snap (kit::dbToGain (params.outputDb));
        blendSm.snap (params.blend);
        bypassSm.snap (params.bypass ? 1.0f : 0.0f);
        satAmtSm.snap (satAmount (params.saturation));
        driveSm.snap (driveGain (params.saturation));
        speedSm.snap (params.speed == 1 ? 1.0f : 0.0f);
        redSm.snap (silkAmount (params, 1));
        blueSm.snap (silkAmount (params, 2));
        for (auto& chain : tape)
            for (auto& t : chain) t.reset();
        for (auto& f : redBand) f.reset();
        for (auto& f : blueBand) f.reset();
        for (auto& d : silkDc) d.reset();
        saturationDb = 0.0f;
    }

    void setParameters (const Parameters& p) noexcept
    {
        params = p;
        trimSm.setTarget (kit::dbToGain (std::clamp (p.trimDb, -12.0f, 12.0f)));
        outputSm.setTarget (kit::dbToGain (std::clamp (p.outputDb, -12.0f, 12.0f)));
        blendSm.setTarget (std::clamp (p.blend, 0.0f, 1.0f));
        bypassSm.setTarget (p.bypass ? 1.0f : 0.0f);
        satAmtSm.setTarget (satAmount (p.saturation));
        driveSm.setTarget (driveGain (p.saturation));
        speedSm.setTarget (p.speed == 1 ? 1.0f : 0.0f);
        redSm.setTarget (silkAmount (p, 1));
        blueSm.setTarget (silkAmount (p, 2));
    }

    /** Process in place. io: numChannels (1 or 2) channel pointers. */
    void process (float* const* io, int numChannels, int numSamples) noexcept
    {
        numChannels = std::clamp (numChannels, 0, kMaxChannels);
        float peakLinear = 0.0f, peakTape = 0.0f;

        for (int n = 0; n < numSamples; ++n)
        {
            const float trim = trimSm.next(), out = outputSm.next(), blend = blendSm.next(), byp = bypassSm.next();
            const float amt = satAmtSm.next(), drive = driveSm.next(), speed = speedSm.next();
            const float red = redSm.next(), blue = blueSm.next();
            const float bias = 0.12f * amt;                    // asymmetry -> even harmonics
            const float biasOut = softClip (bias);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float x = io[ch][n];
                const float u = x * trim;

                // ---- both tape speeds always run (constant CPU, click-free speed switch) ----
                float y[2];
                for (int s = 0; s < 2; ++s)
                {
                    auto& t = tape[s][ch];
                    const float e   = t.preEmph.process (u);
                    const float sat = (softClip (e * drive + bias) - biasOut) / drive;
                    const float diff = t.dc.process (amt * (sat - e));
                    float v = u + t.deEmph.process (diff);       // linear part untouched
                    v = t.lowCut.process (v);
                    v = t.bump.process (v);
                    v = t.dip.process (v);
                    y[s] = t.top.process (v);

                    if (s == (speed < 0.5f ? 0 : 1))
                    {
                        peakLinear = std::max (peakLinear, std::abs (e));
                        peakTape   = std::max (peakTape, std::abs (e + amt * (sat - e)));
                    }
                }
                float wet = kit::crossfade (y[0], y[1], speed);

                // ---- SILK: output-stage harmonic colour (bands always run) ----
                const float hi = redBand[ch].process (wet);
                const float lo = blueBand[ch].process (wet, kit::Svf::Mode::lowpass);
                const float hiC = softClip (2.0f * hi), loC = softClip (2.0f * lo);
                const float harmHi = 0.45f * hiC * hiC + 0.20f * hiC * hiC * hiC;
                const float harmLo = 0.45f * loC * loC + 0.20f * loC * loC * loC;
                const float colour = red * (0.12f * hi + 0.12f * harmHi) + blue * (0.12f * lo + 0.12f * harmLo);
                wet += silkDc[ch].process (colour);

                float yOut = kit::crossfade (u, wet, blend) * out;
                yOut = kit::crossfade (yOut, x, byp);           // exact dry when fully bypassed
                io[ch][n] = yOut;
            }
        }

        saturationDb = (peakTape > 1.0e-6f && peakLinear > peakTape)
                           ? 20.0f * std::log10 (peakLinear / peakTape) : 0.0f;
    }

    /** Peak "tape compression" of the last block in dB (how much the saturation is squashing). */
    float getSaturationDb() const noexcept { return saturationDb; }

    // mapping helpers (public for tests)
    static float satAmount (float s) noexcept { return std::clamp (s / 2.0f, 0.0f, 1.0f); }
    static float driveGain (float s) noexcept { return kit::dbToGain (-8.0f + 1.6f * std::clamp (s, 0.0f, 10.0f)); }
    static float silkAmount (const Parameters& p, int which) noexcept
    {
        return p.silk == which ? std::clamp (p.texture, 0.0f, 10.0f) * 0.1f : 0.0f;
    }

private:
    struct TapePath
    {
        Biquad preEmph, deEmph, lowCut, bump, dip, top;
        kit::DcBlocker dc;
        void reset() noexcept
        {
            preEmph.reset(); deEmph.reset(); lowCut.reset(); bump.reset(); dip.reset(); top.reset(); dc.reset();
        }
    };

    double fs = 48000.0;
    Parameters params;
    kit::Smoother trimSm, outputSm, blendSm, bypassSm, satAmtSm, driveSm, speedSm, redSm, blueSm;
    TapePath tape[2][kMaxChannels];
    kit::SvfHighpass redBand[kMaxChannels];
    kit::Svf blueBand[kMaxChannels];
    kit::DcBlocker silkDc[kMaxChannels];
    float saturationDb = 0.0f;
};

} // namespace tpe542
