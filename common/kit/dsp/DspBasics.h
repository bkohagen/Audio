// Shared, framework-free DSP building blocks for every plugin in this repo.
//
// Rules every class here follows (and every plugin DSP class should follow):
//   * REAL-TIME SAFE: no heap allocation, locks, system calls, exceptions or I/O.
//   * ZERO LATENCY: causal, sample-by-sample; output n depends only on inputs <= n.
//   * Deterministic: fixed-size state, reset() returns to a known state.
#pragma once

#include <algorithm>
#include <cmath>
#include <iterator>

namespace kit
{

constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db / 20.0f); }
inline float gainToDb (float g, float floorDb = -120.0f) noexcept
{
    return g > 0.0f ? std::max (floorDb, 20.0f * std::log10 (g)) : floorDb;
}

/** Equal-sum crossfade that is EXACT at the ends: amount 0 -> a, amount 1 -> b, bit for bit.
    (Use this instead of a + amount * (b - a), which is not exact at amount = 1.) */
inline float crossfade (float a, float b, float amount) noexcept
{
    return a * (1.0f - amount) + b * amount;
}

//==============================================================================
/** One-pole parameter smoother (per-sample, zero latency).
    Snaps to the target once a step stops making progress: a float one-pole otherwise stalls
    ~1e-5 short of its target forever, so bypass/mix never become bit-exact. */
struct Smoother
{
    void setup (double sampleRate, double timeMs) noexcept
    {
        c = (float) std::exp (-1.0 / (0.001 * std::max (0.01, timeMs) * sampleRate));
    }
    void snap (float v) noexcept { cur = target = v; }
    void setTarget (float v) noexcept { target = v; }
    float next() noexcept
    {
        const float prev = cur;
        cur = target + c * (cur - target);
        if (! (cur < prev || cur > prev)) cur = target; // cur == prev, without -Wfloat-equal
        return cur;
    }
    bool isSettled() const noexcept { return ! (cur < target || cur > target); }

    float c = 0.0f, cur = 0.0f, target = 0.0f;
};

//==============================================================================
/** Topology-preserving-transform state-variable filter (Zavalishin), 12 dB/oct.
    Stable under fast modulation, so cutoff can be automated per block without clicks. */
struct Svf
{
    enum class Mode { lowpass, highpass, bandpass };

    void setup (double sampleRate, double cutoffHz, double q = 0.70710678) noexcept
    {
        cutoffHz = std::clamp (cutoffHz, 1.0, 0.45 * sampleRate);
        g  = std::tan (kPi * cutoffHz / sampleRate);
        k  = 1.0 / std::max (0.05, q);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() noexcept { ic1 = ic2 = 0.0; }

    float process (float x, Mode mode) noexcept
    {
        const double v0 = x;
        const double v3 = v0 - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        switch (mode)
        {
            case Mode::lowpass:  return (float) v2;
            case Mode::bandpass: return (float) v1;
            case Mode::highpass: break;
        }
        return (float) (v0 - k * v1 - v2);
    }

    double g = 0, k = 1.41421356, a1 = 0, a2 = 0, a3 = 0;
    double ic1 = 0, ic2 = 0;
};

/** Butterworth high-pass convenience wrapper (e.g. side-chain HPF). */
struct SvfHighpass
{
    void setup (double sampleRate, double cutoffHz) noexcept { f.setup (sampleRate, cutoffHz); }
    void reset() noexcept { f.reset(); }
    float process (float x) noexcept { return f.process (x, Svf::Mode::highpass); }
    Svf f;
};

//==============================================================================
/** One-pole DC blocker. Tip: run only a distortion *difference* through it
    (wet += dc.process (sat (x) - x)) so the clean path stays phase-exact. */
struct DcBlocker
{
    void setup (double sampleRate, double hz) noexcept { r = std::exp (-2.0 * kPi * hz / sampleRate); }
    void reset() noexcept { x1 = y1 = 0.0; }
    float process (float x) noexcept
    {
        const double y = x - x1 + r * y1;
        x1 = x; y1 = y;
        return (float) y;
    }
    double r = 0.999, x1 = 0, y1 = 0;
};

//==============================================================================
/** Fixed-length delay with compile-time capacity (no allocation).
    Use it when a plugin is ALLOWED latency (look-ahead, oversampling, linear phase): delay the dry
    path (and the bypass path) by exactly the reported latency so Mix and Bypass stay
    time-aligned with the wet signal. MaxSamples must be >= the largest latency you report. */
template <int MaxSamples>
struct FixedDelay
{
    static_assert (MaxSamples > 0, "capacity must be positive");
    void setDelay (int samples) noexcept { delay = std::clamp (samples, 0, MaxSamples); }
    int  getDelay() const noexcept { return delay; }
    void reset() noexcept { std::fill (std::begin (buf), std::end (buf), 0.0f); pos = 0; }
    float process (float x) noexcept
    {
        if (delay == 0) return x;
        buf[pos] = x;
        int read = pos - delay;
        if (read < 0) read += MaxSamples + 1;
        const float y = buf[read];
        if (++pos > MaxSamples) pos = 0;
        return y;
    }
    float buf[MaxSamples + 1] {};
    int pos = 0, delay = 0;
};

/** One-pole coefficient for an RC time constant in ms (attack/release ballistics). */
inline double timeConstantCoef (double ms, double sampleRate) noexcept
{
    return std::exp (-1.0 / (std::max (0.01, ms) * 0.001 * sampleRate));
}

} // namespace kit
