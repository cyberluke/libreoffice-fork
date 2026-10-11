#!/usr/bin/env python3
"""Writer 2027 command inventory extractor.

Scans Writer 2027 UI configuration for the actual set of `.uno:` commands and
writes a normalized inventory JSON used as the source of coverage truth for the
Writer 2027 icon themes.

The inventory is generated, not guessed: it is produced from:

  * `sw/uiconfig/swriter/ui/`  -- all notebookbar variants, menus, toolbars, dialogs
  * `officecfg/registry/data/org/openoffice/Office/UI/` -- command registry,
    toolbar mode files, Writer-relevant command definitions

Output:
  tools/writer2027-icons/generated/writer-command-inventory.json
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_OUT = REPO_ROOT / "tools" / "writer2027-icons" / "generated" / "writer-command-inventory.json"

# Look recursively under these directories.
SCAN_DIRS = [
    REPO_ROOT / "sw" / "uiconfig" / "swriter" / "ui",
    REPO_ROOT / "officecfg" / "registry" / "data" / "org" / "openoffice" / "Office" / "UI",
]

UNO_RE = re.compile(r"\.uno:([A-Za-z0-9_:.\-]+)")

# Surfaces we know map to notebookbar tabs / dialog groups. Used only for the
# "priority" hint and for the report; not authoritative.
PRIORITY_KEYWORDS = {
    "notebookbar": "notebookbar",
}


def surface_for(path: Path) -> str:
    name = path.name
    low = name.lower()
    if "notebookbar" in low:
        return "notebookbar"
    if name.endswith(".xcu") and name != "ToolbarMode.xcu":
        return "xcu"
    return name


def unbzip_command(cmd: str) -> str:
    """Normalize a raw `.uno:` token to a canonical command id string.

    Keeps the exact casing as found (LibreOffice command ids are case
    sensitive). Returns the crudest normalized key used for de-duplication:
    lowercased.
    """
    return cmd


def scan_file(path: Path, acc):
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except Exception:
        return
    for m in UNO_RE.finditer(text):
        raw = m.group(1)
        # Trim trailing punctuation that sometimes terminates an id in XML.
        cmd = raw.rstrip(".:_")
        if not cmd:
            continue
        lower = cmd.lower()
        entry = acc.get(lower)
        if entry is None:
            entry = {
                "command": ".uno:" + cmd,
                "normalized": lower,
                "locations": [],
                "surfaces": set(),
            }
            acc[lower] = entry
        loc = str(path.relative_to(REPO_ROOT)).replace("\\", "/")
        if loc not in entry["locations"]:
            entry["locations"].append(loc)
        surface = surface_for(path)
        entry["surfaces"].add(surface)


def scan(dirs):
    acc = {}
    for base in dirs:
        if not base.is_dir():
            print(f"warning: scan dir missing: {base}", file=sys.stderr)
            continue
        for path in sorted(base.rglob("*")):
            if path.is_file() and path.suffix.lower() in (".ui", ".xcu"):
                scan_file(path, acc)
    # Order deterministically.
    out = []
    for lower in sorted(acc):
        e = acc[lower]
        e["locations"].sort()
        e["surfaces"] = sorted(e["surfaces"])
        out.append(e)
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("-o", "--output", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--print", action="store_true", help="print to stdout too")
    args = ap.parse_args(argv)

    entries = scan(SCAN_DIRS)
    payload = {
        "generator": "tools/writer2027-icons/inventory_writer_commands.py",
        "count": len(entries),
        "commands": entries,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=2, sort_keys=False) + "\n", encoding="utf-8")
    print(f"Wrote {len(entries)} commands to {args.output}")
    if args.print:
        for e in entries:
            print(e["command"])
    return 0


if __name__ == "__main__":
    sys.exit(main())