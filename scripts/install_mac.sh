#!/bin/bash
# Installs VBC-1 into the user plug-in folders and clears the download quarantine flag.
# Run from the folder containing the .vst3 / .component (the unzipped CI artifact),
# or from the repo root after building (it also looks in build/VBC1_artefacts/Release).
set -euo pipefail
cd "$(dirname "$0")"

find_bundle() {
  for p in "./$1" "../build/VBC1_artefacts/Release/$2/$1" "./build/VBC1_artefacts/Release/$2/$1"; do
    [ -e "$p" ] && { echo "$p"; return; }
  done
}

VST3=$(find_bundle "VBC-1 Bus Compressor.vst3" VST3)
AU=$(find_bundle "VBC-1 Bus Compressor.component" AU)

mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components

if [ -n "${VST3:-}" ]; then
  rm -rf ~/Library/Audio/Plug-Ins/VST3/"VBC-1 Bus Compressor.vst3"
  cp -R "$VST3" ~/Library/Audio/Plug-Ins/VST3/
  xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"VBC-1 Bus Compressor.vst3" 2>/dev/null || true
  codesign --force --deep -s - ~/Library/Audio/Plug-Ins/VST3/"VBC-1 Bus Compressor.vst3"
  echo "Installed VST3 -> ~/Library/Audio/Plug-Ins/VST3"
fi

if [ -n "${AU:-}" ]; then
  rm -rf ~/Library/Audio/Plug-Ins/Components/"VBC-1 Bus Compressor.component"
  cp -R "$AU" ~/Library/Audio/Plug-Ins/Components/
  xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"VBC-1 Bus Compressor.component" 2>/dev/null || true
  codesign --force --deep -s - ~/Library/Audio/Plug-Ins/Components/"VBC-1 Bus Compressor.component"
  killall -9 AudioComponentRegistrar 2>/dev/null || true
  echo "Installed AU   -> ~/Library/Audio/Plug-Ins/Components"
fi

echo "Done. Rescan plug-ins in Live Professor."
