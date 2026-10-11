#!/usr/bin/env python3
"""Probe icon availability for authoring icon-map.yaml.

Takes a YAML mapping (same shape as icon-map.yaml but can be partial) and
reports, per command, whether each Carbon/Phosphor icon name exists in the
pinned sources, and (for Carbon) at which native optical sizes.

Usage:
    python probe_icons.py --mapping icon-map.yaml
"""

import argparse
import sys
from pathlib import Path

try:
    import yaml
except Exception:
    sys.exit("PyYAML required")

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
CACHE = REPO_ROOT / ".work" / "icon-sources"
CARBON_SIZES = ("32", "24", "20", "16")
PHOSPHOR_REGULAR = None
PHOSPHOR_BOLD = None


def load_sets():
    global PHOSPHOR_REGULAR, PHOSPHOR_BOLD
    carbon_root = CACHE / "carbon" / "packages" / "icons" / "src" / "svg"
    carb32 = {p.stem for p in (carbon_root / "32").glob("*.svg")}
    carb = {}
    for s in CARBON_SIZES:
        carb[s] = {p.stem for p in (carbon_root / s).glob("*.svg")}
    ph_root = CACHE / "phosphor" / "assets"
    PHOSPHOR_REGULAR = {p.stem for p in (ph_root / "regular").glob("*.svg")}
    PHOSPHOR_BOLD = {p.stem for p in (ph_root / "bold").glob("*.svg")}
    return carb, carb32, PHOSPHOR_REGULAR, PHOSPHOR_BOLD


def norm(uno):
    return uno.replace(".uno:", "").lower()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mapping", type=Path, required=True)
    args = ap.parse_args()

    carbon, carb32, ph_reg, ph_bold = load_sets()
    mapping = yaml.safe_load(args.mapping.read_text(encoding="utf-8"))
    commands = mapping.get("commands", {})
    print(f"{'command':38} {'carbon':26} {'c16':4} {'c24':4} {'phosphor':26} {'p-b':4}")
    problems = 0
    for uno in sorted(commands, key=norm):
        spec = commands[uno]
        cname = norm(uno)
        if spec.get("target"):
            continue
        cb = spec.get("carbon")
        cb = cb.get("icon", cb) if isinstance(cb, dict) else cb
        ph = spec.get("phosphor")
        ph = ph.get("icon", ph) if isinstance(ph, dict) else ph
        c_ok = "-"
        c16 = c24 = "-"
        if cb and isinstance(cb, str):
            if cb.startswith("custom:"):
                c_ok = "CST"
            else:
                name = cb.split(":")[-1]
                c16 = "Y" if name in carbon.get("16", set()) else "."
                c24 = "Y" if name in carbon.get("24", set()) else "."
                c_ok = "OK" if name in carb32 else "MISS"
                if c_ok == "MISS":
                    problems += 1
        p_ok = "-"
        p_bold = "-"
        if ph and isinstance(ph, str):
            if ph.startswith("custom:"):
                p_ok = "CST"
            else:
                p_ok = "OK" if ph in ph_reg else "MISS"
                p_bold = "Y" if ph in ph_bold else "."
                if p_ok == "MISS":
                    problems += 1
        mark = "" if (c_ok in ("OK", "CST") and p_ok in ("OK", "CST")) else "  <<<"
        print(f"{uno:38} {str(cb):28} {c16:4} {c24:4} {str(ph):28} {p_bold:4}{mark}")
    print(f"\nproblems (missing names): {problems}")


if __name__ == "__main__":
    main()