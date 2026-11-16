# Audio plugin monorepo (JUCE 8 / CMake)

- Plugins live in `plugins/<TARGET>/`; shared code in `common/kit/`; build logic in
  `cmake/KitPlugin.cmake`. Reference implementation: `plugins/VBC1/`.
- For any new plugin or plugin change, use the **`audio-plugin` skill**
  (`.claude/skills/audio-plugin/SKILL.md`). It starts by asking whether the plugin must be zero
  latency, and whether Pro Tools (AAX) is needed.
- Hard rules: nothing in `processBlock` may allocate, lock, log or touch the GUI; no
  `AudioProcessorValueTreeState` or Slider/ButtonAttachment (use `kit::ParamBinding`); every switch
  is crossfaded; never change a released plugin's code, manufacturer code or parameter IDs.
- Build & test (Linux): `cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build -j8 && ctest --test-dir build --output-on-failure`
- Render a plugin's UI: `build/plugins/<TARGET>/<TARGET>_screenshot_artefacts/RelWithDebInfo/<TARGET>_screenshot out.png`
- macOS bundles come from CI (`AudioPlugins-macOS` artifact); install with `bash install_mac.sh`.
