#!/usr/bin/env python3
"""Lookup actual Carbon and Phosphor icon names by substring.

For authoring icon-map.yaml: given a concept keyword, print every real
upstream icon name containing it (Carbon native sizes, Phosphor regular/bold).

Usage:
    python lookup_icons.py cart --phosphor  # carbon names containing 'cart'
    python lookup_icons.py arrow --carbon
"""
import argparse
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
CACHE = REPO / ".work" / "icon-sources"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("substr")
    ap.add_argument("--carbon", action="store_true")
    ap.add_argument("--phosphor", action="store_true")
    args = ap.parse_args()
    s = args.substr.lower()

    if args.carbon or not args.phosphor:
        root = CACHE / "carbon" / "packages" / "icons" / "src" / "svg"
        names = sorted({p.stem for p in (root / "32").glob("*.svg")})
        hits = [n for n in names if s in n]
        print(f"=== CARBON ({len(hits)}) ===")
        for n in hits:
            sizes = []
            for sz in ("16", "24", "20"):
                if (root / sz / f"{n}.svg").is_file():
                    sizes.append(sz)
            tag = " " + "".join(f"[{x}]" for x in sizes) if sizes else ""
            print(f"  {n}{tag}")
    if args.phosphor or args.carbon != True:
        pass
    if args.phosphor:
        root = CACHE / "phosphor" / "assets"
        reg = sorted({p.stem for p in (root / "regular").glob("*.svg")})
        bold = {p.stem for p in (root / "bold").glob("*.svg")}
        hits = [n for n in reg if s in n]
        print(f"=== PHOSPHOR ({len(hits)}) ===")
        for n in hits:
            tag = " [b]" if n in bold else ""
            print(f"  {n}{tag}")


if __name__ == "__main__":
    main()