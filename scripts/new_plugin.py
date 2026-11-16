#!/usr/bin/env python3
"""Create a new plugin in plugins/<TARGET>/ from templates/plugin/.

Example:
    python3 scripts/new_plugin.py --target OPT1 --name "OPT-1 Opto Leveler" --code Opt1 \
        --short "OPT-1" --subtitle "OPTICAL  LEVELING  AMPLIFIER" \
        --latency 0 --vst3-categories "Fx Dynamics" --aax-category AAX_ePlugInCategory_Dynamics

The result builds immediately (a working gain/mix/bypass effect with tests); then replace the
marked processing block, parameters and layout with the real design.
"""
import argparse
import pathlib
import re
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
TEMPLATE = ROOT / "templates" / "plugin"
PLUGINS = ROOT / "plugins"

AAX_CATEGORIES = {
    "AAX_ePlugInCategory_None", "AAX_ePlugInCategory_EQ", "AAX_ePlugInCategory_Dynamics",
    "AAX_ePlugInCategory_PitchShift", "AAX_ePlugInCategory_Reverb", "AAX_ePlugInCategory_Delay",
    "AAX_ePlugInCategory_Modulation", "AAX_ePlugInCategory_Harmonic", "AAX_ePlugInCategory_NoiseReduction",
    "AAX_ePlugInCategory_Dither", "AAX_ePlugInCategory_SoundField", "AAX_ePlugInCategory_HWGenerators",
    "AAX_ePlugInCategory_SWGenerators", "AAX_ePlugInCategory_WrappedPlugin", "AAX_EPlugInCategory_Effect",
}


def existing_codes():
    codes = {}
    for cm in PLUGINS.glob("*/CMakeLists.txt"):
        m = re.search(r"PLUGIN_CODE\s+(\S+)", cm.read_text())
        if m:
            codes[m.group(1)] = cm.parent.name
    return codes


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--target", required=True, help="CMake target / folder name, e.g. OPT1 (letters, digits, _)")
    ap.add_argument("--name", required=True, help='Product name shown in hosts, e.g. "OPT-1 Opto Leveler"')
    ap.add_argument("--code", required=True, help="4-char plugin code: first letter upper-case, rest lower-case/digits, unique")
    ap.add_argument("--short", help="Short name for the editor header (default: target)")
    ap.add_argument("--subtitle", default="", help="Small caps subtitle in the header")
    ap.add_argument("--latency", type=int, default=0, help="Reported latency in samples (0 = zero-latency plugin)")
    ap.add_argument("--vst3-categories", default="Fx", help='e.g. "Fx Dynamics", "Fx EQ", "Fx Reverb"')
    ap.add_argument("--aax-category", default="AAX_ePlugInCategory_None", help="Pro Tools category")
    a = ap.parse_args()

    errors = []
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", a.target):
        errors.append("--target must be letters/digits/underscore and start with a letter")
    if not re.fullmatch(r"[A-Z][a-z0-9]{3}", a.code):
        errors.append("--code must be 4 chars: one upper-case letter then 3 lower-case letters/digits (AU/GarageBand rule)")
    if a.code in existing_codes():
        errors.append(f"--code {a.code} is already used by plugins/{existing_codes()[a.code]}")
    if (PLUGINS / a.target).exists():
        errors.append(f"plugins/{a.target} already exists")
    if a.latency < 0 or a.latency > 192000:
        errors.append("--latency must be between 0 and 192000 samples")
    if a.aax_category not in AAX_CATEGORIES:
        errors.append(f"--aax-category must be one of: {', '.join(sorted(AAX_CATEGORIES))}")
    if '"' in a.name or '"' in (a.subtitle or ""):
        errors.append("names must not contain double quotes")
    if errors:
        print("\n".join("error: " + e for e in errors), file=sys.stderr)
        return 1

    cls = re.sub(r"[^A-Za-z0-9]", "", a.target)
    cls = cls[0].upper() + cls[1:]
    replacements = {
        "__TARGET__": a.target,
        "__PRODUCT_NAME__": a.name,
        "__SHORT_NAME__": a.short or a.target,
        "__SUBTITLE__": a.subtitle.upper(),
        "__CODE__": a.code,
        "__CLASS__": cls,
        "__NS__": a.target.lower(),
        "__LATENCY__": str(a.latency),
        "__BADGE__": "ZERO LATENCY" if a.latency == 0 else "",
        "__VST3_CATEGORIES__": a.vst3_categories,
        "__AAX_CATEGORY__": a.aax_category,
    }

    dest = PLUGINS / a.target
    shutil.copytree(TEMPLATE, dest)
    for f in dest.rglob("*"):
        if f.is_file():
            text = f.read_text()
            for k, v in replacements.items():
                text = text.replace(k, v)
            f.write_text(text)
            leftover = [p for p in re.findall(r"__[A-Z0-9_]+__", text) if p != "__VA_ARGS__"]
            if leftover:
                print(f"warning: unreplaced placeholders in {f}: {sorted(set(leftover))}", file=sys.stderr)

    print(f"Created plugins/{a.target}  ({a.name}, code {a.code}, latency {a.latency} samples)")
    print("Next: put the DSP in Source/PluginDSP.h, parameters in PluginProcessor.*, layout in PluginEditor.*,")
    print(f"      then build and run: ctest -R {a.target}  and  {a.target}_screenshot out.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
