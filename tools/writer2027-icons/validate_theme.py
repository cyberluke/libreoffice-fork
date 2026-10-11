#!/usr/bin/env python3
"""Structural/safety/size validator for a generated Writer 2027 theme.

Validates every committed SVG in an icon theme directory:

  * SVG safety: no <script>, foreignObject, external href, http(s), data:image,
    embedded bitmap, @import (spec #49).
  * SVG structure: svg root, viewBox, width, height, at least one drawable
    shape/path; no zero-size viewBox, no NaN coordinates, valid XML (#50).
  * Size contract: sc_*.svg -> 16x16, lc_*.svg -> 24x24, cmd/32/*.svg -> 32x32
    (#51).

Usage:
    python validate_theme.py --theme icon-themes/writer2027_carbon_svg
"""

import argparse
import math
import re
import sys
from pathlib import Path

try:
    from lxml import etree
except Exception:
    sys.exit("lxml required")

SVG_NS = "{http://www.w3.org/2000/svg}"
DRAWABLE = {"path", "rect", "circle", "ellipse", "line", "polyline", "polygon",
            "use", "text", "g"}
DANGER = [
    (re.compile(r"<script", re.I), "script element"),
    (re.compile(r"foreignObject", re.I), "foreignObject"),
    (re.compile(r"\s(?:href|xlink:href)\s*=\s*(['\"])(?:https?://)", re.I), "external href"),
    (re.compile(r"data:image", re.I), "embedded raster"),
    (re.compile(r"@import", re.I), "css import"),
    (re.compile(r"\bjavascript:", re.I), "javascript"),
]


def validate_file(path: Path, expected_w: int, expected_h: int):
    text = path.read_text(encoding="utf-8", errors="replace")
    for pat, name in DANGER:
        if pat.search(text):
            return f"unsafe: {name}"
    try:
        root = etree.fromstring(text.encode("utf-8"))
    except Exception as e:
        return f"invalid XML: {e}"
    if etree.QName(root).localname != "svg":
        return "root not <svg>"
    vb = root.get("viewBox")
    if not vb:
        return "no viewBox"
    parts = [p.strip() for p in vb.replace(",", " ").split()]
    if len(parts) != 4:
        return f"bad viewBox {vb!r}"
    try:
        nums = [float(p) for p in parts]
    except ValueError:
        return f"NaN viewBox {vb!r}"
    if nums[2] <= 0 or nums[3] <= 0:
        return "zero-size viewBox"
    if any(math.isnan(x) or math.isinf(x) for x in nums):
        return "non-finite viewBox"
    w = root.get("width")
    h = root.get("height")
    if w != str(expected_w) or h != str(expected_h):
        return f"size {w}x{h}, expected {expected_w}x{expected_h}"
    drawable = False
    for el in root.iter():
        if etree.QName(el).localname in DRAWABLE:
            drawable = True
            break
    if not drawable:
        return "no drawable content"
    return None


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--theme", type=Path, required=True)
    args = ap.parse_args(argv)

    cmd = Path(args.theme) / "cmd"
    if not cmd.is_dir():
        sys.exit(f"not a theme dir: {args.theme}")
    files = list(cmd.glob("sc_*.svg")) + list(cmd.glob("lc_*.svg")) + \
            list((cmd / "32").glob("*.svg"))
    failures = []
    checked = 0
    for f in sorted(files):
        name = f.name
        if name.startswith("sc_"):
            want = (16, 16)
        elif name.startswith("lc_"):
            want = (24, 24)
        else:
            want = (32, 32)
        err = validate_file(f, *want)
        checked += 1
        if err:
            failures.append((str(f.relative_to(args.theme)), err))
    print(f"validated {checked} SVGs; failures={len(failures)}")
    for f, err in failures[:50]:
        print(f"  FAIL {f}: {err}")
    if failures:
        print("FAIL", file=sys.stderr)
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())