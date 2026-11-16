// VBC-1 Bus Compressor — core DSP (framework-independent, header-only).
//
// Models the behaviour of the classic British console VCA stereo bus compressor:
//   * feed-forward VCA topology, stereo-linked detection (max of L/R)
//   * stepped ratio (2/4/10), attack (0.1–30 ms) and release (0.1–1.2 s + Auto)
//   * ratio-dependent soft knee (softer at 2:1, harder at 10:1)
//   * program-dependent "Auto" release (dual time-constant, two-capacitor model)
//   * VCA colour: gain-reduction-dependent, asymmetric (2nd + 3rd harmonic) THD
//   * output stage: gentle symmetric soft saturation
//   * side-chain high-pass filter (12 dB/oct, 20–500 Hz), internal or external key
//
// REAL-TIME SAFETY: no heap allocation, no locks, no system calls, no exceptions, no I/O.
// All state is fixed-size members; every switch (bypass, listen, mix, makeup...) is
// crossfaded/smoothed so automation or button presses never produce clicks.
//
// ZERO LATENCY: everything here is causal and sample-by-sample. No look-ahead,
// no oversampling, no block-based analysis. Output sample n depends only on
// input samples <= n, and the plugin reports 0 samples of latency.
#pragma once

#include <kit/dsp/DspBasics.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace vbc
{

using kit::kPi;
using kit::Smoother;
using kit::SvfHighpass;
using kit::DcBlocker;

//==============================================================================
struct Parameters
{
    float thresholdDb = -10.0f;  // -30 .. +6 dBFS
    float ratio       = 4.0f;    // 2, 4, 10
    float attackMs    = 10.0f;   // 0.1, 0.3, 1, 3, 10, 30
    float releaseMs   = 300.0f;  // 100, 300, 600, 1200; <= 0 means AUTO
    float makeupDb    = 0.0f;    // -10 .. +20 dB
    bool  hpfOn       = true;
    float hpfHz       = 90.0f;   // 20 .. 500 Hz
    float analog      = 0.5f;    // 0 .. 1 (0 = clean / linear)
    float mix         = 1.0f;    // 0 .. 1 (parallel compression)
    bool  externalSc  = false;
    bool  scListen    = false;
    bool  bypass      = false;   // click-free (crossfaded) bypass
};

//==============================================================================
class BusCompressor
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double newSampleRate) noexcept
    {
        fs = newSampleRate;
        for (auto& d : dcBlock) d.setup (fs, 4.0);
        makeupSm.setup (fs, 20.0);
        mixSm.setup (fs, 20.0);
        thresholdSm.setup (fs, 20.0);
        analogSm.setup (fs, 50.0);
        bypassSm.setup (fs, 10.0);
        listenSm.setup (fs, 10.0);
        lastHpfHz = -1.0f;
        updateCoefficients();
        reset();
    }

    void reset() noexcept
    {
        for (auto& h : hpf) h.reset();
        for (auto& d : dcBlock) d.reset();
        envFast = envSlow = 0.0;
        makeupSm.snap (dbToGain (params.makeupDb));
        mixSm.snap (params.mix);
        thresholdSm.snap (params.thresholdDb);
        analogSm.snap (params.analog);
        bypassSm.snap (params.bypass ? 1.0f : 0.0f);
        listenSm.snap (params.scListen ? 1.0f : 0.0f);
        currentGrDb = 0.0f;
    }

    void setParameters (const Parameters& p) noexcept
    {
        params = p;
        makeupSm.target    = dbToGain (p.makeupDb);
        mixSm.target       = std::clamp (p.mix, 0.0f, 1.0f);
        thresholdSm.target = p.thresholdDb;
        analogSm.target    = std::clamp (p.analog, 0.0f, 1.0f);
        bypassSm.target    = p.bypass ? 1.0f : 0.0f;
        listenSm.target    = p.scListen ? 1.0f : 0.0f;
        updateCoefficients();
    }

    // Static gain computer: returns gain reduction in dB (>= 0).
    static double gainReductionDb (double levelDb, double thresholdDb, double ratio, double kneeDb) noexcept
    {
        const double over  = levelDb - thresholdDb;
        const double slope = 1.0 - 1.0 / ratio;
        if (2.0 * over <= -kneeDb) return 0.0;
        if (2.0 * over >=  kneeDb) return slope * over;
        const double t = over + 0.5 * kneeDb;
        return slope * t * t / (2.0 * kneeDb);
    }

    // Knee width depends on ratio, like the hardware (2:1 is noticeably soft).
    static double kneeForRatio (double ratio) noexcept
    {
        if (ratio <= 2.5) return 10.0;
        if (ratio <= 5.0) return 6.0;
        return 3.0;
    }

    /** Process in place.
        io      : numChannels pointers (1 or 2), processed in place
        sc      : external side-chain pointers (may be nullptr / scChannels 0)
    */
    void process (float* const* io, int numChannels, const float* const* sc, int scChannels, int numSamples) noexcept
    {
        numChannels = std::clamp (numChannels, 0, kMaxChannels);
        const bool useExt = params.externalSc && sc != nullptr && scChannels > 0;
        const double ratio = std::max (1.0, (double) params.ratio);
        const double knee  = kneeForRatio (ratio);
        const bool autoRel = params.releaseMs <= 0.0f;

        float blockMaxGr = 0.0f;

        for (int n = 0; n < numSamples; ++n)
        {
            // ---- side-chain / detector ------------------------------------
            float key[kMaxChannels] = { 0.0f, 0.0f };
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float s;
                if (useExt)
                    s = sc[std::min (ch, scChannels - 1)][n];
                else
                    s = io[ch][n];

                // The HPF always runs so its state stays warm: toggling it never glitches.
                const float filtered = hpf[ch].process (s);
                if (params.hpfOn)
                    s = filtered;

                key[ch] = s;
                peak = std::max (peak, std::abs (s)); // stereo link: max of channels
            }

            const double levelDb = 20.0 * std::log10 (std::max ((double) peak, 1.0e-6));
            const double target  = gainReductionDb (levelDb, thresholdSm.next(), ratio, knee);

            // ---- ballistics (log domain, decoupled attack/release) ---------
            if (target > envFast) envFast = target + attackCoef * (envFast - target);
            else                  envFast = target + (autoRel ? autoFastRelCoef : releaseCoef) * (envFast - target);

            double grDb = envFast;
            if (autoRel)
            {
                // Second "capacitor": charges slowly from the fast detector, discharges slowly.
                // Short transients release fast; sustained compression releases slowly.
                if (envFast > envSlow) envSlow = envFast + autoSlowChargeCoef * (envSlow - envFast);
                else                   envSlow = envFast + autoSlowRelCoef   * (envSlow - envFast);
                grDb = std::max (envFast, envSlow);
            }
            else
            {
                envSlow = envFast;
            }

            if (! std::isfinite (envFast)) envFast = envSlow = grDb = 0.0;

            blockMaxGr = std::max (blockMaxGr, (float) grDb);

            const float vcaGain = (float) std::pow (10.0, -grDb / 20.0);
            const float makeup  = makeupSm.next();
            const float mix     = mixSm.next();
            const float analog  = analogSm.next();
            const float bypass  = bypassSm.next();
            const float listen  = listenSm.next();

            // VCA colour grows with gain reduction (real VCAs distort more as they work harder)
            const float vcaDrive = analog * std::min (0.08f + 0.03f * (float) grDb, 0.7f);
            const float outDrive = analog * 0.45f;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float dry = io[ch][n];
                float wet = dry * vcaGain;

                if (vcaDrive > 1.0e-5f)
                {
                    // Only the distortion *difference* passes through the DC blocker, so the
                    // linear signal path stays phase-exact and analog=0 is bit-transparent.
                    const float colour = asymSat (wet, vcaDrive, 0.3f) - wet;
                    wet += dcBlock[ch].process (colour);
                }
                else
                {
                    dcBlock[ch].process (0.0f);
                }

                wet *= makeup;

                if (outDrive > 1.0e-5f)
                    wet = std::tanh (outDrive * wet) / outDrive;

                float out = dry + mix * (wet - dry);
                out = out * (1.0f - listen) + key[ch] * listen; // crossfade to side-chain audition
                out = out * (1.0f - bypass) + dry * bypass;     // crossfade to clean bypass (exact at 1)
                io[ch][n] = out;
            }
        }

        currentGrDb = blockMaxGr;
    }

    float getGainReductionDb() const noexcept { return currentGrDb; }
    static constexpr int getLatencySamples() noexcept { return 0; }

private:
    static float dbToGain (float db) noexcept { return std::pow (10.0f, db / 20.0f); }

    // Asymmetric soft saturation normalised to unity small-signal gain and zero offset.
    static float asymSat (float x, float d, float bias) noexcept
    {
        const float db  = d * bias;
        const float t0  = std::tanh (db);
        const float sl  = d * (1.0f - t0 * t0); // derivative at x = 0
        return (std::tanh (d * x + db) - t0) / sl;
    }

    double coefFor (double ms) const noexcept
    {
        return std::exp (-1.0 / (std::max (0.01, ms) * 0.001 * fs));
    }

    void updateCoefficients() noexcept
    {
        attackCoef          = coefFor (params.attackMs);
        releaseCoef         = coefFor (std::max (1.0f, params.releaseMs));
        autoFastRelCoef     = coefFor (120.0);
        autoSlowChargeCoef  = coefFor (350.0);
        autoSlowRelCoef     = coefFor (1800.0);

        if (std::abs (params.hpfHz - lastHpfHz) > 1.0e-3f)
        {
            for (auto& h : hpf) h.setup (fs, params.hpfHz);
            lastHpfHz = params.hpfHz;
        }
    }

    double fs = 48000.0;
    Parameters params;

    SvfHighpass hpf[kMaxChannels];
    DcBlocker   dcBlock[kMaxChannels];
    Smoother    makeupSm, mixSm, thresholdSm, analogSm, bypassSm, listenSm;
    float       lastHpfHz = -1.0f;

    double envFast = 0.0, envSlow = 0.0;
    double attackCoef = 0.0, releaseCoef = 0.0;
    double autoFastRelCoef = 0.0, autoSlowChargeCoef = 0.0, autoSlowRelCoef = 0.0;

    float currentGrDb = 0.0f;
};

} // namespace vbc
