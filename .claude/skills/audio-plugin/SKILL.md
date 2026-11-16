---
name: audio-plugin
description: Build a new audio plugin (VST3 / AU, and AAX for Pro Tools) in this repo, or change an existing one — compressors, limiters, EQs, saturators, gates, delays, channel strips, etc. Use whenever the user asks to make, design, clone-the-behaviour-of, redesign or fix a plugin in plugins/, or change the shared kit in common/. Always starts by asking whether the plugin must be zero latency.
---

# Building audio plugins in this repo

This repo is a JUCE 8 / CMake monorepo. Every plugin lives in `plugins/<TARGET>/` and is built,
tested, validated and packaged by the same machinery. Read `README.md` for the layout. The
reference implementation is `plugins/VBC1/` (zero-latency VCA bus compressor) — copy its
patterns when in doubt.

## Step 0 — ask first (before writing any code)

Ask with **one** AskUserQuestion call (skip a question only if the user already answered it):

1. **"Does this plugin need to be zero latency?"**
   - *Yes — zero latency (Recommended for live use: Live Professor, MainStage, tracking/monitoring)*
   - *No — latency is OK (mixing/mastering: allows oversampling, look-ahead, linear-phase, FFT)*
2. **"Which hosts/formats?"** (multi-select) — *VST3 + AU on Mac (always built)*, *Pro Tools (AAX)*,
   *Windows build too*.
3. If not already clear: **what it should do / sound like** (e.g. "1176-style FET compressor",
   "Pultec-style EQ"), and the **product name**.

Record the answers; they decide the rules in the sections below. If the answer is "latency is
OK", also settle the exact latency (in samples, constant at all sample rates or per-rate) before
designing the DSP.

## Step 1 — scaffold

```bash
python3 scripts/new_plugin.py --target <TARGET> --name "<Product Name>" --code <Code> \
    --short "<HEADER NAME>" --subtitle "<what it is>" --latency <0 or N> \
    --vst3-categories "Fx <Dynamics|EQ|Distortion|Delay|Reverb|...>" --aax-category <AAX_ePlugInCategory_...>
```
- `--code`: 4 chars, first upper-case then 3 lower-case/digits (AU/GarageBand rule), unique in
  the repo (the script checks). **Never change a plugin's code, the manufacturer code (`Vbca`),
  `BUNDLE_ID`, or any parameter ID after release** — hosts (Live Professor, Pro Tools, Logic) key
  saved sessions and automation on them.
- The scaffold builds and passes its tests immediately: gain → (your DSP) → output, mix,
  crossfaded bypass, IN switch, output-peak readout.
- Names/branding must be original. Emulating a classic circuit's *behaviour* is fine; do not use
  another company's product names, logos, trade dress or marketing text in the UI or docs.

## Step 2 — DSP (`Source/PluginDSP.h`, framework-free)

Put all processing in a header-only class with `prepare / reset / setParameters / process`, no
JUCE includes, so it can be unit-tested directly. Use `common/kit/dsp/DspBasics.h`:
`Smoother` (snaps to target — never write your own one-pole smoother for switches), `Svf`
(TPT, safe to modulate), `SvfHighpass`, `DcBlocker`, `FixedDelay<N>`, `crossfade()` (exact at the
ends), `dbToGain`, `timeConstantCoef`.

### Real-time rules (hard requirements — the RT test enforces the first three)
- No heap allocation, no locks/mutexes, no `std::function` creation, no logging, no file/network
  I/O, no exceptions, no `MessageManager`/GUI calls inside `process()` / `processBlock()`.
- Fixed-size state only (`float buf[N]`, `std::array`). Size everything in `prepare()` at the
  latest — and prefer compile-time capacity so even `prepare()` doesn't allocate per block size.
- Parameters are read once per block via atomic loads (`param->get()`) into a plain struct.
- **Every** switch or discontinuous parameter is smoothed/crossfaded (bypass, mix, listen, mode
  changes, makeup). Bypass must settle to *bit-exact* dry — use `kit::crossfade(y, dry, b)`, not
  `y + b * (dry - y)`.
- Keep filters/detectors running even when their feature is switched off, so re-enabling
  doesn't glitch. Keep CPU constant (bypass does the same work).
- Handle any block size 1..N (hosts send blocks bigger than `prepareToPlay` announced) and
  mono or stereo.

### If ZERO LATENCY was chosen
- `kLatencySamples = 0`. No look-ahead, no oversampling, no FFT/partitioned convolution, no
  linear-phase filters, no block-based analysis. Everything causal, sample-by-sample.
- Saturation without oversampling aliases: keep nonlinearities gentle/low-order, scale drive with
  a "character" control, run only the distortion *difference* through a DC blocker so the clean
  path stays bit-transparent, and offer a 0 % setting that is exactly linear.
- Tests must show an impulse comes out at sample 0 and the clean path is bit-transparent.

### If LATENCY IS ALLOWED
- Report the exact wet-path delay: `kLatencySamples` → `setLatencySamples()` in
  `prepareToPlay()`. It must be **constant while playing** (Pro Tools and Live Professor compensate
  using it; changing it mid-stream causes drift/clicks). If it depends on sample rate or
  oversampling factor, compute it in `prepareToPlay()` and call `setLatencySamples()` there only.
- Delay the **dry and bypass paths by the same amount** (`kit::FixedDelay`) — the template already
  does this — otherwise Mix and Bypass comb-filter / jump in time.
- Oversampling: `juce::dsp::Oversampling` is fine (allocate in `prepareToPlay`, use
  `getLatencyInSamples()`), or a fixed-size polyphase halfband of your own.
- Show a "LATENCY n ms" badge instead of "ZERO LATENCY" (the scaffold leaves the badge empty).
- Tests must show the impulse arrives exactly at the reported latency and that Mix 0 % / Bypass
  equal the latency-aligned dry signal bit for bit (the template's tests do both).

## Step 3 — parameters & processor (`Source/PluginProcessor.*`)
- Plain `AudioParameterFloat/Choice/Bool` added with `addParameter`, IDs in `ParamIDs`, version
  hint `1`. **No `AudioProcessorValueTreeState`, no `SliderAttachment`/`ButtonAttachment`** — with
  the editor open, host automation (applied inside the audio callback) makes them lock and
  allocate on the audio thread. Measured on VBC-1: ~1,800 allocations and ~250 contended locks per
  test run before the fix.
- Stepped hardware controls → `AudioParameterChoice`. Continuous → `NormalisableRange` with a
  sensible skew (`setSkewForCentre`), interval and `withStringFromValueFunction` text + units.
- `getBypassParameter()` returns your bypass param so host bypass is crossfaded by your DSP.
- Side-chain: add `.withInput ("Sidechain", stereo, true)`, accept disabled/mono/stereo for it in
  `isBusesLayoutSupported`, read it as channels after the main bus (see VBC1's `processBlock`).
- State: `kit::saveParameterState / loadParameterState` (plain values by ID).
- Meters to the GUI: `std::atomic<float>` written with `memory_order_relaxed`.

## Step 4 — editor (`Source/PluginEditor.*`)
- Derive from `kit::PluginEditorBase (processor, baseW, baseH)`; bind with `bindKnob` /
  `bindButton` (polling `kit::ParamBinding`, never attachments); position in `layout()` in base
  coordinates; draw cards in `paintContent()`; update meters/status in `onTimer()` (30 Hz). Call
  `finishSetup()` last. The window is resizable 75–150 % automatically.
- Components: `kit::KnobCell` (name, value readout, hardware scale labels; `stepped=true` for
  choice params), `kit::GainReductionMeter` (0 dB on the left), `juce::ToggleButton` (LED pill, or
  `getProperties().set ("style", "in")` for the illuminated IN switch, bound with `invert=true` to
  bypass), `juce::Label`.
- Design system (`common/kit/ui/Theme.h`): dark cards, blue `accent` for main controls, amber
  `warm` for character/engage, red `hot` for warnings. Header via `drawBackgroundAndHeader`.
  Layout grid used so far: cells 156×172, cards with 16–20 px gutters; VBC-1 uses a 720×800
  portrait layout (2×4 knob grid left, meter/character/side-chain right).
- **Always render and look at the UI before pushing**: build `<TARGET>_screenshot`, run
  `<TARGET>_screenshot out.png`, Read the PNG, fix overlaps/clipping, repeat.

## Step 5 — tests
Write DSP tests in `tests/dsp_tests.cpp` (framework-free, `CHECK` macro). Always keep the
template's latency / mix-0 % / bypass (click-free + bit-exact) / finite-output tests, and add
behaviour tests for the design, e.g.:
- dynamics: static curve vs. theory, attack/release time constants (63 % after τ), program-
  dependent release, side-chain filter/key effect;
- EQ/filters: magnitude at a few frequencies vs. the target curve, stability at extreme settings;
- saturation: harmonic levels (single-bin DFT), DC offset, 0 % = bit-transparent;
- everything: no NaN/Inf, mono and stereo.

Then build and run everything (Linux container):
```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo     # add -DJUCE_SOURCE_DIR=... if JUCE is checked out locally
cmake --build build -j8
ctest --test-dir build --output-on-failure            # all plugins: DSP + RT-safety tests
```
`<TARGET>_rt_safety_test` must report `processBlock: allocs=0 frees=0 locks=0` in every scenario
(it exercises every supported bus layout, random block sizes, automation of every parameter, with
the editor open). Investigate any failure with the stack traces it prints — never weaken the test.

## Step 6 — deliver
- Commit on the working branch and push. CI (`.github/workflows/build-macos.yml`) builds every
  plugin on `macos-latest` (universal arm64 + x86_64), runs tests, `pluginval` strictness 8 on
  each VST3, `auval` on each AU, and uploads **`AudioPlugins-macOS`** (all bundles +
  `install_mac.sh`). Don't pin `macos-14`-style images: retired images make jobs cancel instantly
  with no logs.
- Check the run (GitHub MCP `actions_list`), then give the user the run URL → Artifacts →
  unzip → `bash install_mac.sh [name-filter]` → rescan in the host.
- Add the plugin to the table in the top-level `README.md` and write `plugins/<TARGET>/README.md`
  (features, controls table, latency, identity codes). Save the screenshot to
  `plugins/<TARGET>/docs/screenshot.png`.

## Pro Tools (AAX)
- Pro Tools only loads **AAX**. `kit_add_plugin` adds the AAX format automatically when
  `KIT_AAX_SDK_PATH` (or env `AAX_SDK_PATH`) points at the Avid AAX SDK (folder containing
  `Interfaces/AAX.h`); macOS and Windows only. Set `AAX_CATEGORY` per plugin.
- The SDK is free from Avid's developer portal (requires an Avid developer account). Its licence
  doesn't allow committing it to this public repo. For CI, the user stores a private download URL
  of the SDK zip in the repository secret `AAX_SDK_URL`; the workflow then builds AAX too.
- **Signing:** unsigned AAX plugins only load in the *Pro Tools Developer* build. Retail Pro Tools
  requires the plugin to be signed with PACE's tools (PACE developer account + iLok). This is a
  manual/credentialed step — tell the user, don't fake it. `install_mac.sh` copies `.aaxplugin`
  to `/Library/Application Support/Avid/Audio/Plug-Ins` (needs sudo) and does not ad-hoc sign it.
- Pro Tools specifics: delay compensation uses the reported latency (must be exact and constant);
  host bypass maps to `getBypassParameter()`; side-chain shows up as the key input when the plugin
  has a side-chain bus; Pro Tools also runs plugins offline (AudioSuite) — `processBlock` must work
  with `isNonRealtime()` true and any block size. Pro Tools offers mono, stereo and mono→stereo
  instances; the template accepts in == out (mono/mono, stereo/stereo) — add mono→stereo in
  `isBusesLayoutSupported` if the design needs it.
- Windows Pro Tools users need a Windows build: add a `windows-latest` job (VST3 + AAX) if asked.

## Pitfalls already paid for (don't repeat)
- APVTS/attachments allocate and lock on the audio thread under automation → use `kit::ParamBinding`.
- A float one-pole smoother stalls ~1e-5 short of its target → `kit::Smoother` snaps.
- `out + 1.0f * (dry - out)` is not exactly `dry` → `kit::crossfade`.
- `-G Xcode` needs the full Xcode app; command-line tools need the default generator.
- `macos-14` runner label was retired → `macos-latest`.
- JUCE `Slider` text doesn't refresh if the bound value equals its initial 0 → `ParamBinding` calls
  `updateText()`.
- Running downloaded binaries (e.g. pluginval) in the cloud container may be blocked — validate in CI.
- GitHub artifact downloads require being signed in, even for public repos.

## Done checklist
- [ ] Step 0 answers recorded (zero latency? formats?) and reflected in the design
- [ ] DSP tests pass, including latency, mix-0 %, bypass click-free + bit-exact
- [ ] `<TARGET>_rt_safety_test`: processBlock 0 allocs / 0 locks in every scenario
- [ ] Screenshot rendered and checked; saved to `plugins/<TARGET>/docs/screenshot.png`
- [ ] READMEs updated; identity codes unique and recorded
- [ ] Pushed; CI green (pluginval + auval); user given the artifact link and install steps
- [ ] If AAX: user told about the SDK secret and PACE signing status
