#!/bin/bash
# Installs every plugin bundle found next to this script (the unzipped CI artifact) or, when run
# from a checkout, every bundle in build/ — VST3, AU and AAX — clears the download quarantine
# flag and ad-hoc signs them (AAX is left alone: it must be PACE-signed for retail Pro Tools).
#
#   bash install_mac.sh            # install everything
#   bash install_mac.sh VBC-1      # install only bundles whose name contains "VBC-1"
set -euo pipefail
cd "$(dirname "$0")"
filter="${1:-}"

VST3_DIR=~/Library/Audio/Plug-Ins/VST3
AU_DIR=~/Library/Audio/Plug-Ins/Components
AAX_DIR="/Library/Application Support/Avid/Audio/Plug-Ins"
mkdir -p "$VST3_DIR" "$AU_DIR"

bundles=()
while IFS= read -r -d '' b; do bundles+=("$b"); done < <(
  { find . -maxdepth 1 \( -name '*.vst3' -o -name '*.component' -o -name '*.aaxplugin' \) -print0
    [ -d ../build ] && find ../build -path '*_artefacts/Release/*' \( -name '*.vst3' -o -name '*.component' -o -name '*.aaxplugin' \) -prune -print0
  } 2>/dev/null)

[ ${#bundles[@]} -gt 0 ] || { echo "No plugin bundles found next to this script or in ../build"; exit 1; }

au_installed=0
for b in "${bundles[@]}"; do
  name=$(basename "$b")
  [[ -z "$filter" || "$name" == *"$filter"* ]] || continue
  case "$name" in
    *.vst3)      dest="$VST3_DIR" ;;
    *.component) dest="$AU_DIR"; au_installed=1 ;;
    *.aaxplugin) dest="$AAX_DIR" ;;
  esac
  if [[ "$name" == *.aaxplugin ]]; then
    echo "Installing AAX needs admin rights (Pro Tools plug-in folder):"
    sudo mkdir -p "$dest" && sudo rm -rf "$dest/$name" && sudo cp -R "$b" "$dest/"
  else
    rm -rf "$dest/$name"
    cp -R "$b" "$dest/"
    xattr -dr com.apple.quarantine "$dest/$name" 2>/dev/null || true
    codesign --force --deep -s - "$dest/$name"
  fi
  echo "Installed $name -> $dest"
done

[ $au_installed -eq 1 ] && { killall -9 AudioComponentRegistrar 2>/dev/null || true; }
echo "Done. Rescan plug-ins in your host (Live Professor, Logic, Pro Tools...)."
