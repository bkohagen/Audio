# VBC-1 Bus Compressor

A zero-latency stereo VCA bus compressor (VST3 + AU) modelled on the behaviour of the classic
British console bus compressor, with an adjustable side-chain high-pass filter.
Built with JUCE 8; runs natively on Apple silicon (universal arm64 + x86_64 binary, macOS 11+).
Designed for live use in hosts such as **Live Professor**.

## Features

| Control | Range | Notes |
|---|---|---|
| Threshold | -30 … +6 dBFS | |
| Ratio | 2 / 4 / 10 | stepped like the hardware; knee is softer at 2:1, harder at 10:1 |
| Attack | 0.1 / 0.3 / 1 / 3 / 10 / 30 ms | stepped |
| Release | 0.1 / 0.3 / 0.6 / 1.2 s / **Auto** | Auto = program-dependent dual time-constant release |
| Make-up | -10 … +20 dB | |
| SC HPF | 20 … 500 Hz, on/off | 12 dB/oct Butterworth in the detector path only — stops kick/bass pumping |
| Ext SC | on/off | key from the plug-in's side-chain input instead of the main input |
| Listen | on/off | audition the (filtered) side-chain signal |
| Analog | 0 … 100 % | VCA colour (gain-reduction-dependent 2nd/3rd harmonic THD) + output-stage soft saturation. 0 % = clean/linear |
| Mix | 0 … 100 % | parallel (New York) compression, sample-aligned |

Analog-style behaviours that are modelled:
- Feed-forward VCA, stereo-linked detector (max of L/R) so the stereo image never shifts
- Log-domain RC attack/release ballistics
- Program-dependent Auto release (transients recover fast, sustained compression recovers slowly)
- VCA distortion that rises as the VCA works harder, plus gentle output-stage saturation
- Moving-needle gain-reduction meter

### Zero latency
No look-ahead, no oversampling, no FFT/block processing — every sample is computed from current
and past input only, and the plug-in reports **0 samples** to the host. This is verified by
the unit tests (an impulse comes out at sample 0, and the clean path is bit-transparent).
Because there's no oversampling, the Analog saturation is kept gentle to limit aliasing;
turn Analog down to 0 % for a fully clean compressor.

### Real-time safety (lock-free audio thread)
Live hosts (Live Professor, MainStage, …) glitch or drop out if a plug-in allocates memory or
waits on a lock inside its audio callback. VBC-1 is built and **tested** to avoid that:

- `processBlock()` does no heap allocation, no locking, no logging/I-O and no message-thread
  calls. The DSP state is fixed-size members; parameters are read with atomic loads.
- **No `AudioProcessorValueTreeState` and no `SliderAttachment`/`ButtonAttachment`.** With those,
  host automation (which the VST3 wrapper applies *inside* the audio callback) makes the attachment
  lock and `realloc` the message queue on the audio thread whenever the plug-in window is open.
  The editor instead polls the parameters on a 30 Hz UI timer and sends knob moves to the host
  from the message thread.
- Host bypass is mapped to an internal parameter and **crossfaded over 10 ms** (as are LISTEN,
  Mix and Make-up), so bypassing or automating never clicks. The SC HPF keeps running while
  switched off, so re-enabling it doesn't glitch either.
- CPU load is constant: bypass/listen don't change the amount of work per sample.

`tests/rt_safety_test.cpp` enforces this. It wraps `malloc`/`calloc`/`realloc`/`free`, every
`operator new`/`delete`, and pthread mutex/rwlock/condvar calls at link time, then runs the
plug-in on an audio thread exactly the way JUCE's VST3 wrapper does (host automation of every
parameter, random block sizes 1–2048, stereo/mono, side-chain on/off) while the editor is open and a
simulated user moves knobs. Result:

| Audio-thread phase | Allocations | Locks |
|---|---|---|
| `processBlock` (our code) | **0** | **0** |
| Host applying automation (JUCE VST3 wrapper) | **0** | 1 per changed parameter — JUCE's own per-parameter listener lock |

That remaining lock lives inside JUCE's VST3 wrapper (`AudioProcessorParameter::sendValueChangedMessageToListeners`)
and can't be removed without patching JUCE. It is only ever contended if the *same* parameter is
changed from the GUI at the same instant the host automates it; in the stress test (GUI changing
parameters every 2 ms during heavy automation) it was contended 1–2 times in 24,000 changes.
Before these fixes the same test showed ~1,800 allocations and ~250 contended locks per run.

## Getting the Mac plug-in

### Option A — download the CI build (no Xcode needed)
Every push builds on an Apple-silicon GitHub runner (`.github/workflows/build-macos.yml`),
runs the unit tests, `pluginval` (strictness 8) and Apple's `auval`, then uploads
`VBC1-macOS.zip`. In GitHub: **Actions → Build macOS → latest run → Artifacts**.
Unzip it and run:

```bash
bash install_mac.sh
```

This copies the plug-ins to `~/Library/Audio/Plug-Ins/VST3` and `…/Components`, clears the
download quarantine flag and ad-hoc signs them.

### Option B — build locally
Requires Apple's command-line tools (`xcode-select --install`) and CMake (`brew install cmake`).
The full Xcode app is not needed.

```bash
git clone -b claude/ecstatic-ptolemy-86rla9 https://github.com/bkohagen/Audio.git
cd Audio
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 8
ctest --test-dir build                 # DSP unit tests
bash scripts/install_mac.sh
```

(If you have the full Xcode app you can use `cmake -B build -G Xcode` and
`cmake --build build --config Release` instead.)

## Using it in Live Professor
1. Install as above, then rescan plug-ins in Live Professor's plug-in manager.
2. Insert **VBC-1 Bus Compressor** (VST3, or the AU if you prefer) on your bus/master.
3. Start with: Ratio 4, Attack 10 ms, Release Auto, SC HPF on at ~90–150 Hz, then lower
   Threshold until the needle shows 2–4 dB on peaks.
4. **External side-chain:** route a source to the plug-in's side-chain input in the host (if your
   host/version exposes plug-in side-chain inputs), then enable **EXT SC**. The label under the
   buttons shows whether a side-chain bus is active. If no side-chain is routed, leave EXT SC off —
   the compressor then keys from its own input (through the HPF).

## Project layout
```
Source/BusCompDSP.h        framework-free DSP core (all the compression/analog modelling)
Source/PluginProcessor.*   JUCE wrapper: parameters, buses (main + side-chain), state
Source/PluginEditor.*      GUI and GR meter
tests/dsp_tests.cpp        offline DSP tests (latency, static curve, ballistics, HPF, SC, colour, mix)
```

To build against a local JUCE checkout instead of downloading it: `-DJUCE_SOURCE_DIR=/path/to/JUCE`.

JUCE is used under its GPLv3/commercial licence terms — distribute accordingly.
