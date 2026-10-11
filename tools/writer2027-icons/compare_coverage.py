#!/usr/bin/env python3
"""Coverage validator for Writer 2027 icon themes.

Checks that every command in the Writer 2027 notebookbar (Tier 0-4) has an
icon in the given theme directory, i.e. no fallback to Colibre for the
Writer-critical surfaces.

Usage:
    python compare_coverage.py \
        --inventory tools/writer2027-icons/generated/writer-command-inventory.json \
        --theme icon-themes/writer2027_carbon_svg \
        --tier-max 4

Expected: missing = 0 for Tier 0-4. Exits non-zero otherwise.
"""

import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--inventory", type=Path, required=True)
    ap.add_argument("--theme", type=Path, required=True)
    ap.add_argument("--tier-max", type=int, default=4,
                    help="only enforce zero-fallback up to this tier")
    ap.add_argument("--notebookbar", type=str, default="notebookbar_writer2027",
                    help="surface file to scope coverage to (default: writer2027 notebookbar)")
    args = ap.parse_args(argv)

    inv = json.loads(args.inventory.read_text(encoding="utf-8"))
    commands = inv["commands"]

    # Scope to the Writer 2027 notebookbar (the active Writer UI surface).
    scoped = [
        c for c in commands
        if any(args.notebookbar in loc for loc in c["locations"])
    ]
    print(f"inventory: {len(commands)} commands, {len(scoped)} in {args.notebookbar}")

    theme = Path(args.theme)
    cmd_dir = theme / "cmd"

    # Load links.txt aliases: "cmd/sc_x.svg cmd/sc_y.svg" means x is aliased
    # to y's asset (LibreOffice resolves these at runtime).
    aliased = set()
    links = theme / "links.txt"
    if links.is_file():
        for line in links.read_text(encoding="utf-8").splitlines():
            parts = line.split()
            if len(parts) == 2:
                aliased.add(Path(parts[0]).name)

    missing = []
    present = 0
    for c in sorted(scoped, key=lambda x: x["normalized"]):
        name = c["normalized"]
        sc = cmd_dir / f"sc_{name}.svg"
        lc = cmd_dir / f"lc_{name}.svg"
        if sc.is_file() and lc.is_file():
            present += 1
        elif f"sc_{name}.svg" in aliased and f"lc_{name}.svg" in aliased:
            present += 1
        else:
            missing.append(c["command"])

    total = len(scoped)
    pct = 100.0 * present / total if total else 100.0
    print(f"coverage: {present}/{total} ({pct:.1f}%)  missing={len(missing)}")
    for m in missing:
        print(f"  MISSING: {m}")

    if missing:
        print(f"FAIL: {len(missing)} Writer {args.notebookbar} commands lack icons "
              f"(zero-fallback gate for Tier 0-{args.tier_max})", file=sys.stderr)
        return 1
    print(f"PASS: zero fallback for {args.notebookbar}")
    return 0


if __name__ == "__main__":
    sys.exit(main())