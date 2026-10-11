#!/usr/bin/env python3
"""Writer 2027 icon theme generator.

Turns the hand-curated `icon-map.yaml` plus the pinned upstream Carbon and
Phosphor SVG sources into the two committed LibreOffice themes:

  icon-themes/writer2027_carbon_svg/
  icon-themes/writer2027_phosphor_svg/

Each theme directory gets `cmd/sc_<name>.svg`, `cmd/lc_<name>.svg`,
`cmd/32/<name>.svg` (as the mapping defines), plus a generated `links.txt` for
aliases and a `GENERATED.json` provenance manifest.

The generator is deterministic: same mapping + same pinned sources => identical
bytes. It never downloads anything during a normal run; `--fetch` clones/updates
the pinned source cache (dev-only).

Usage (dev-only fetch + build):
    python build_themes.py --fetch --source-cache .work/icon-sources

Normal (offline, uses existing cache):
    python build_themes.py --source-cache .work/icon-sources
"""

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

try:
    import yaml  # PyYAML
    HAVE_YAML = True
except Exception:
    HAVE_YAML = False

from svg_normalize import normalize_svg_text, WRITER_ICON_FG  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
TOOLS_DIR = Path(__file__).resolve().parent
MAP_FILE = TOOLS_DIR / "icon-map.yaml"
LOCK_FILE = TOOLS_DIR / "icon-sources.lock.json"
GENERATED_DIR = TOOLS_DIR / "generated"

THEMES = {
    "carbon": "writer2027_carbon_svg",
    "phosphor": "writer2027_phosphor_svg",
    "tdesign": "tdesign_svg",
}

CARBON_SIZES = ("32", "24", "20", "16")  # preference order
# Phosphor weights by optical target (spec #23 / updated for naming).
PHOSPHOR_WEIGHT = {16: "bold", 24: "regular", 32: "regular"}


def load_yaml(path):
    if not HAVE_YAML:
        # Minimal fallback is not supported; document requirement.
        raise SystemExit(
            "PyYAML is required to read icon-map.yaml. Install it: pip install pyyaml"
        )
    with open(path, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def load_lock(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def norm_name(uno: str) -> str:
    """``.uno:Bold`` -> ``bold`` (strip prefix, lowercase, no separators)."""
    return uno.replace(".uno:", "").lower()


def find_carbon_icon(svg_root: Path, icon: str):
    """Resolve a carbon icon name across optical sizes.

    icon may be suffixed with a size hint like ``16:text--bold``. Returns
    ``(source_path, native_size)`` or raises.
    """
    if ":" in icon:
        size, name = icon.split(":", 1)
    else:
        size, name = None, icon
    candidates = [size] if size else list(CARBON_SIZES)
    for s in candidates:
        p = svg_root / s / f"{name}.svg"
        if p.is_file():
            return p, int(s)
    raise FileNotFoundError(f"carbon icon not found: {name!r} (sizes {candidates})")


def find_phosphor_icon(assets_root: Path, icon: str, weight: str):
    """Resolve a phosphor icon at a given weight.

    Weight files are named ``<name>-bold.svg`` (bold) or ``<name>.svg``
    (regular). `icon` is the canonical catalog name.
    """
    if weight == "bold":
        p = assets_root / "bold" / f"{icon}-bold.svg"
    else:
        p = assets_root / "regular" / f"{icon}.svg"
    if not p.is_file():
        raise FileNotFoundError(f"phosphor {weight} icon not found: {icon!r}")
    return p


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            h.update(chunk)
    return h.hexdigest()


def write_asset(path: Path, content: str):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content + "\n", encoding="utf-8")


def _custom_source(theme: str, icon: str) -> Path:
    """Resolve a `custom:<name>` icon in the committed custom assets dir."""
    name = icon.split(":", 1)[1]
    p = TOOLS_DIR / "custom" / theme / f"{name}.svg"
    if not p.is_file():
        raise FileNotFoundError(f"custom {theme} icon missing: {name!r} ({p})")
    return p


def generate_carbon(cmd, spec, src_root: Path, output_dir: Path):
    """Generate sc_/lc_/32 SVGs for a single carbon command.

    Uses Carbon's native optical sizes when they exist (spec #57), else falls
    back to the 32 master scaled to the target (spec #17/#18/#19).
    """
    icon = spec.get("carbon")
    if isinstance(icon, dict):
        icon = icon.get("icon", icon)
    if not isinstance(icon, str):
        raise ValueError(f"command {cmd}: carbon spec must be an icon name or dict")

    # target_size -> preferred native source sizes, in order, then 32 fallback.
    plan = {
        16: (("16",), "32"),
        24: (("24",), "32"),
        32: (("32",), "32"),
    }
    for size, (preferred, fallback) in plan.items():
        if icon.startswith("custom:"):
            chosen = _custom_source("carbon", icon)
        else:
            chosen = None
            for pref in preferred:
                cand = src_root / pref / f"{icon}.svg"
                if cand.is_file():
                    chosen = cand
                    break
            if chosen is None:
                chosen = src_root / fallback / f"{icon}.svg"
        text = chosen.read_text(encoding="utf-8")
        content = normalize_svg_text(text, size, size)
        if size == 16:
            rel = output_dir / "cmd" / f"sc_{cmd}.svg"
        elif size == 24:
            rel = output_dir / "cmd" / f"lc_{cmd}.svg"
        else:
            rel = output_dir / "cmd" / "32" / f"{cmd}.svg"
        write_asset(rel, content)
    return None


def generate_phosphor(cmd, spec, assets_root: Path, output_dir: Path):
    icon = spec.get("phosphor")
    if isinstance(icon, dict):
        icon = icon.get("icon", icon)
    if not isinstance(icon, str):
        raise ValueError(f"command {cmd}: phosphor spec must be an icon name or dict")

    for size, prefix in ((16, "sc"), (24, "lc"), (32, "32")):
        weight = PHOSPHOR_WEIGHT[size]
        if icon.startswith("custom:"):
            src = _custom_source("phosphor", icon)
        else:
            src = find_phosphor_icon(assets_root, icon, weight)
        text = src.read_text(encoding="utf-8")
        content = normalize_svg_text(text, size, size)
        if size == 16:
            rel = output_dir / "cmd" / f"sc_{cmd}.svg"
        elif size == 24:
            rel = output_dir / "cmd" / f"lc_{cmd}.svg"
        else:
            rel = output_dir / "cmd" / "32" / f"{cmd}.svg"
        write_asset(rel, content)
    return None


def generate_tdesign(cmd, spec, src_root: Path, output_dir: Path):
    """Generate sc_/lc_/32 SVGs for a single TDesign command.

    TDesign sources are stroke-based 24x24 line icons; the normalizer
    recolors strokes/fills to the Writer 2027 dark foreground.
    """
    icon = spec.get("carbon") or spec.get("phosphor") or spec.get("tdesign")
    if isinstance(icon, dict):
        icon = icon.get("icon", icon)
    if not isinstance(icon, str):
        raise ValueError(f"command {cmd}: tdesign spec must be an icon name or dict")

    for size, prefix in ((16, "sc"), (24, "lc"), (32, "32")):
        if icon.startswith("custom:"):
            src = _custom_source("tdesign", icon)
        else:
            src = src_root / f"{icon}.svg"
            if not src.is_file():
                raise FileNotFoundError(f"tdesign icon not found: {icon!r}")
        text = src.read_text(encoding="utf-8")
        content = normalize_svg_text(text, size, size)
        if size == 16:
            rel = output_dir / "cmd" / f"sc_{cmd}.svg"
        elif size == 24:
            rel = output_dir / "cmd" / f"lc_{cmd}.svg"
        else:
            rel = output_dir / "cmd" / "32" / f"{cmd}.svg"
        write_asset(rel, content)
    return None


def build_theme(theme: str, mapping: dict, cache: Path, lock: dict,
                output_dir: Path):
    """Generate one full theme directory. Returns (counts, coverage_report)."""
    # Resolve the source root that contains the icon assets (per lock `path`).
    src_root = cache / theme / lock[theme]["path"]
    if not src_root.is_dir():
        raise SystemExit(f"missing source dir for {theme}: {src_root}")
    commands = mapping.get("commands", {})
    aliases = mapping.get("aliases", {}).copy()
    output_dir = Path(output_dir)
    (output_dir / "cmd" / "32").mkdir(parents=True, exist_ok=True)

    counts = {"sc": 0, "lc": 0, "32": 0, "custom": 0, "alias": 0}
    missing = []
    coverage = {}

    for uno in sorted(commands):
        spec = commands[uno]
        cmd_name = spec.get("output", norm_name(uno))
        tier = str(spec.get("tier", "x"))
        coverage.setdefault(tier, {"total": 0, "done": 0, "missing": []})

        # alias entry
        target = spec.get("target")
        if target:
            # record alias for links.txt later
            aliases[uno] = target
            continue

        is_custom = (spec.get("status") == "custom")
        try:
            if theme == "carbon":
                generate_carbon(cmd_name, spec, src_root, output_dir)
            elif theme == "phosphor":
                generate_phosphor(cmd_name, spec, src_root, output_dir)
            else:
                generate_tdesign(cmd_name, spec, src_root, output_dir)
        except FileNotFoundError as e:
            missing.append(uno)
            coverage[tier]["missing"].append(uno)
            print(f"  MISSING {theme} {uno}: {e}", file=sys.stderr)
            continue
        counts["sc"] += 1
        counts["lc"] += 1
        counts["32"] += 1
        if is_custom:
            counts["custom"] += 1
        coverage[tier]["total"] += 1
        coverage[tier]["done"] += 1

    # Write links.txt from aliases + target entries among commands.
    links = []
    seen = set()
    for uno in sorted(aliases):
        target = aliases[uno]
        tl = norm_name(target)
        ul = norm_name(uno)
        for pref in ("sc", "lc", "32"):
            key = (pref, uno)
            if key in seen:
                continue
            seen.add(key)
            if pref == "32":
                links.append(f"cmd/32/{ul}.svg cmd/32/{tl}.svg")
            else:
                links.append(f"cmd/{pref}_{ul}.svg cmd/{pref}_{tl}.svg")
    if links:
        (output_dir / "links.txt").write_text("\n".join(links) + "\n", encoding="utf-8")

    counts["alias"] = len(links)
    return counts, coverage


def check_source(lock: dict, cache: Path, fetch: bool):
    """Verify the source cache matches pinned SHAs; optionally fetch."""
    for name in ("carbon", "phosphor", "tdesign"):
        repo = cache / name
        want = lock[name]["commit"]
        if not (repo / ".git").exists():
            if fetch:
                subprocess.run(
                    ["git", "clone", "--filter=blob:none", "--no-checkout",
                     lock[name]["repository"], str(repo)], check=True)
            else:
                raise SystemExit(
                    f"source cache missing {name}. Run with --fetch first.")
        if fetch:
            subprocess.run(["git", "-C", str(repo), "fetch", "origin"], check=True)
        cur = subprocess.run(["git", "-C", str(repo), "rev-parse", "HEAD"],
                             capture_output=True, text=True).stdout.strip()
        if cur != want:
            raise SystemExit(
                f"source {name} HEAD {cur} != pinned {want}. Run --fetch to sync.")
    return cache


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source-cache", type=Path,
                    default=REPO_ROOT / ".work" / "icon-sources")
    ap.add_argument("--fetch", action="store_true",
                    help="dev-only: clone/update pinned source cache")
    ap.add_argument("--themes", nargs="*", default=list(THEMES.keys()),
                    help="which themes to generate (default both)")
    ap.add_argument("--mapping", type=Path, default=MAP_FILE)
    ap.add_argument("--report", action="store_true", help="write coverage .md")
    ap.add_argument("--contact-sheets", action="store_true",
                    help="render contact sheets (needs a built soffice)")
    ap.add_argument("--soffice", type=Path,
                    default=Path("C:/lo-build/instdir/program/soffice.exe"),
                    help="soffice binary for SVG rasterization")
    args = ap.parse_args(argv)

    if args.contact_sheets:
        out_dir = REPO_ROOT / "workdir" / "writer2027-icons"
        return make_contact_sheets(args.soffice, out_dir)

    mapping = load_yaml(args.mapping)
    lock = load_lock(LOCK_FILE)
    cache = check_source(lock, args.source_cache, args.fetch)

    for theme in args.themes:
        if theme not in THEMES:
            raise SystemExit(f"unknown theme: {theme}")
        # tdesign uses its own mapping file derived from icon-map.yaml.
        if theme == "tdesign":
            theme_map = load_yaml(TOOLS_DIR / "icon-map-tdesign.yaml")
            map_sha = sha256_file(TOOLS_DIR / "icon-map-tdesign.yaml")
        else:
            theme_map = mapping
            map_sha = sha256_file(args.mapping)
        out = REPO_ROOT / "icon-themes" / THEMES[theme]
        counts, coverage = build_theme(theme, theme_map, cache, lock, out)

        manifest = {
            "generator": "tools/writer2027-icons/build_themes.py",
            "generator_version": 2,
            "source": theme,
            "source_commit": lock[theme]["commit"],
            "source_license": "Apache-2.0" if theme == "carbon" else "MIT",
            "mapping_sha256": map_sha,
            "icon_count": counts["sc"],
            "counts": counts,
        }
        (out / "GENERATED.json").write_text(
            json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        print(f"[{theme}] {THEMES[theme]}: {counts}")

        if args.report:
            _write_report(GENERATED_DIR / f"coverage-{theme}.md", theme, coverage,
                          mapping)

    return 0


def _write_report(path: Path, theme, coverage, mapping):
    lines = [f"# Writer 2027 {theme.capitalize()} coverage", ""]
    total_done = total_all = 0
    for tier in sorted(coverage):
        t = coverage[tier]
        total_done += t["done"]
        total_all += t["total"]
        pct = 100.0 * t["done"] / t["total"] if t["total"] else 0.0
        lines.append(f"Tier {tier}: {t['done']} / {t['total']}  {pct:.0f}%")
    lines.append(f"ALL: {total_done} / {total_all}  "
                 f"({100.0*total_done/total_all if total_all else 100.0:.0f}%)")
    lines.append("")
    lines.append("Missing commands:")
    for tier in sorted(coverage):
        for m in coverage[tier]["missing"]:
            lines.append(f"- {m}")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _rasterize_batch_with_soffice(soffice: Path, svg_files, out_png_dir: Path,
                                  timeout: int = 900, chunk: int = 80):
    """Rasterize SVGs to PNGs in a few soffice invocations.

    Passes explicit file paths (soffice on Windows cannot load a directory).
    Files are chunked to stay under command-line length limits.
    """
    import subprocess
    out_png_dir.mkdir(parents=True, exist_ok=True)
    src = out_png_dir / "_src"
    src.mkdir(parents=True, exist_ok=True)
    for svg in svg_files:
        shutil.copy2(svg, src / svg.name)
    files = sorted(src.glob("*.svg"))
    for i in range(0, len(files), chunk):
        batch = [str(soffice), "--headless", "--convert-to", "png",
                 "--outdir", str(out_png_dir)]
        batch += [str(f) for f in files[i:i + chunk]]
        subprocess.run(batch, check=True, capture_output=True, timeout=timeout)
    shutil.rmtree(src, ignore_errors=True)


def make_contact_sheets(soffice: Path, out_dir: Path):
    """Render per-theme contact sheets (dev tool, spec #52).

    Produces workdir/writer2027-icons/{carbon,phosphor}-{16,24}.png showing
    every generated icon at its real optical size on a dark background.
    """
    from PIL import Image, ImageDraw, ImageFont  # type: ignore

    out_dir.mkdir(parents=True, exist_ok=True)
    for theme, theme_dir in THEMES.items():
        cmd_dir = REPO_ROOT / "icon-themes" / theme_dir / "cmd"
        for size, prefix in ((16, "sc"), (24, "lc")):
            icons = sorted(cmd_dir.glob(f"{prefix}_*.svg"))
            cols = 16
            rows = (len(icons) + cols - 1) // cols
            cell = size + 24
            sheet = Image.new("RGB", (cols * cell, rows * cell), (30, 32, 36))
            draw = ImageDraw.Draw(sheet)
            try:
                font = ImageFont.truetype("arial.ttf", 10)
            except Exception:
                font = ImageFont.load_default()
            # Rasterize all icons of this size in one soffice run.
            png_dir = out_dir / f"{theme}-{size}-png"
            png_dir.mkdir(parents=True, exist_ok=True)
            try:
                _rasterize_batch_with_soffice(soffice, icons, png_dir)
            except Exception as e:  # noqa: BLE001
                print(f"soffice rasterize failed for {theme} {size}: {e}")
                continue
            for i, svg in enumerate(icons):
                x = (i % cols) * cell + (cell - size) // 2
                y = (i // cols) * cell + 4
                png = png_dir / (svg.stem + ".png")
                try:
                    img = Image.open(png).convert("RGBA")
                    img.thumbnail((size, size), Image.LANCZOS)
                    sheet.paste(img, (x, y), img)
                except Exception as e:  # noqa: BLE001
                    draw.rectangle((x, y, x + size, y + size), outline=(200, 60, 60))
                    draw.text((x, y), "ERR", fill=(200, 60, 60), font=font)
            sheet_path = out_dir / f"{theme}-{size}.png"
            sheet.save(sheet_path)
            print(f"contact sheet: {sheet_path} ({len(icons)} icons)")
    return 0


if __name__ == "__main__":
    sys.exit(main())