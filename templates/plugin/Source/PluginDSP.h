// __PRODUCT_NAME__ — core DSP (framework-independent, header-only).
//
// REAL-TIME SAFETY: no heap allocation, locks, system calls, exceptions or I/O in process().
// All state is fixed-size members; every switch is crossfaded so nothing clicks.
//
// LATENCY: kLatencySamples is reported to the host (PluginProcessor sets it in prepareToPlay).
//   * Zero-latency plugin: keep it 0. No look-ahead, no oversampling, no FFT/block processing.
//   * Plugin that needs latency (look-ahead, oversampling, linear phase): set it to the exact
//     delay of the wet path. The dry and bypass paths are delayed by the same amount below, so
//     Mix and Bypass stay time-aligned. The DSP tests verify the impulse arrives exactly there.
#pragma once

#include <kit/dsp/DspBasics.h>

namespace __NS__
{

struct Parameters
{
    float inputDb  = 0.0f;   // -24 .. +24 dB
    float outputDb = 0.0f;   // -24 .. +24 dB
    float mix      = 1.0f;   // 0 .. 1
    bool  bypass   = false;  // click-free (crossfaded) bypass
};

class Processor
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kLatencySamples = __LATENCY__;      // see LATENCY note above
    static constexpr int getLatencySamples() noexcept { return kLatencySamples; }

    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        inputSm.setup (fs, 20.0);
        outputSm.setup (fs, 20.0);
        mixSm.setup (fs, 20.0);
        bypassSm.setup (fs, 10.0);
        for (auto& d : dryDelay) d.setDelay (kLatencySamples);
        for (auto& d : wetDelay) d.setDelay (kLatencySamples); // stand-in for your look-ahead / oversampling delay
        reset();
    }

    void reset() noexcept
    {
        inputSm.snap (kit::dbToGain (params.inputDb));
        outputSm.snap (kit::dbToGain (params.outputDb));
        mixSm.snap (params.mix);
        bypassSm.snap (params.bypass ? 1.0f : 0.0f);
        for (auto& d : dryDelay) d.reset();
        for (auto& d : wetDelay) d.reset();
    }

    void setParameters (const Parameters& p) noexcept
    {
        params = p;
        inputSm.setTarget (kit::dbToGain (p.inputDb));
        outputSm.setTarget (kit::dbToGain (p.outputDb));
        mixSm.setTarget (std::clamp (p.mix, 0.0f, 1.0f));
        bypassSm.setTarget (p.bypass ? 1.0f : 0.0f);
    }

    /** Process in place. io: numChannels (1 or 2) channel pointers. */
    void process (float* const* io, int numChannels, int numSamples) noexcept
    {
        numChannels = std::clamp (numChannels, 0, kMaxChannels);

        for (int n = 0; n < numSamples; ++n)
        {
            const float in = inputSm.next(), out = outputSm.next(), mix = mixSm.next(), byp = bypassSm.next();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float x   = io[ch][n];
                const float dry = dryDelay[ch].process (x);   // time-aligned with the wet path

                // ===== YOUR PROCESSING GOES HERE ===========================================
                float wet = wetDelay[ch].process (x * in);
                // ==========================================================================

                wet *= out;
                float y = kit::crossfade (dry, wet, mix);
                y = kit::crossfade (y, dry, byp);             // exact dry when fully bypassed
                io[ch][n] = y;
            }
        }
    }

private:
    double fs = 48000.0;
    Parameters params;
    kit::Smoother inputSm, outputSm, mixSm, bypassSm;
    kit::FixedDelay<(kLatencySamples > 0 ? kLatencySamples : 1)> dryDelay[kMaxChannels], wetDelay[kMaxChannels];
};

} // namespace __NS__
