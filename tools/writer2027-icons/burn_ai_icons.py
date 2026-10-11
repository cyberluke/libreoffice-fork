#!/usr/bin/env python3
"""Burn the TDesign AI icon set into every bundled LibreOffice icon theme.

Requirement: all AI actions/buttons must feature the TDesign exclusive AI
icons even when another icon pack is selected in LibreOffice.

LibreOffice resolves command icons from the active theme's `cmd/` directory
(images_<theme>.zip). By placing the AI command icons into EVERY bundled
theme's cmd/ (SVG for *_svg themes, PNG for raster themes), any selected pack
shows the same TDesign AI icons. The default-theme archive inherits them via
the base theme, covering the runtime fallback path.

AI command set (sc_/lc_/32 per command):
  ai, ai-1, ai-article, ai-book-open, ai-chart-bar, ai-coordinate-system,
  ai-cut, ai-edit, ai-edit-1, ai-education, ai-git-branch, ai-image,
  ai-image-1, ai-layout, ai-music, ai-screenshot, ai-search, ai-terminal,
  ai-terminal-1, ai-textformat-italic, ai-tool, ai-video,
  robot, robot-1, robot-2, aideck

PNG rasterization uses the built LibreOffice (soffice) for pixel-perfect
rendering of the normalized SVGs.
"""
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "writer2027-icons"))
from svg_normalize import normalize_svg_text  # noqa: E402

REPO = Path(__file__).resolve().parent.parent.parent
TDESIGN_SRC = REPO / ".work" / "icon-sources" / "tdesign" / "svg"
SOFFICE = Path("C:/lo-build/instdir/program/soffice.exe")

AI_ICONS = [
    "ai", "ai-1", "ai-article", "ai-book-open", "ai-chart-bar",
    "ai-coordinate-system", "ai-cut", "ai-edit", "ai-edit-1", "ai-education",
    "ai-git-branch", "ai-image", "ai-image-1", "ai-layout", "ai-music",
    "ai-screenshot", "ai-search", "ai-terminal", "ai-terminal-1",
    "ai-textformat-italic", "ai-tool", "ai-video",
    "robot", "robot-1", "robot-2",
    "aideck",  # sidebar deck icon (uses TDesign 'ai' geometry)
]

DECK_SOURCE = "ai"

BUNDLED_THEMES = [
    "breeze", "breeze_dark", "breeze_dark_svg", "breeze_svg",
    "colibre", "colibre_svg", "colibre_dark", "colibre_dark_svg",
    "elementary", "elementary_svg",
    "karasa_jaga", "karasa_jaga_svg",
    "sifr", "sifr_svg", "sifr_dark", "sifr_dark_svg",
    "sukapura", "sukapura_dark", "sukapura_dark_svg", "sukapura_svg",
    "writer2027_carbon_svg", "writer2027_phosphor_svg",
    "tdesign_svg",
]

PNG_THEMES = [t for t in BUNDLED_THEMES if not t.endswith("_svg")]
SVG_THEMES = [t for t in BUNDLED_THEMES if t.endswith("_svg")]

SIZES = ((16, "sc_"), (24, "lc_"), (32, ""))


def rendered_svg(name: str, size: int) -> str:
    src_name = DECK_SOURCE if name == "aideck" else name
    text = (TDESIGN_SRC / f"{src_name}.svg").read_text(encoding="utf-8")
    return normalize_svg_text(text, size, size)


def rasterize_batch(soffice: Path, svg_dir: Path, out_dir: Path, chunk: int = 100,
                    timeout: int = 600):
    """Convert all SVGs in svg_dir to PNGs in out_dir via soffice."""
    out_dir.mkdir(parents=True, exist_ok=True)
    files = sorted(svg_dir.glob("*.svg"))
    for i in range(0, len(files), chunk):
        batch = [str(soffice), "--headless", "--convert-to", "png",
                 "--outdir", str(out_dir)]
        batch += [str(f) for f in files[i:i + chunk]]
        subprocess.run(batch, check=True, capture_output=True, timeout=timeout)


def main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--themes-dir", type=Path, default=REPO / "icon-themes",
                    help="directory containing the theme dirs (default: repo icon-themes)")
    ap.add_argument("--tdesign-src", type=Path, default=TDESIGN_SRC)
    args = ap.parse_args(argv)
    themes_root = args.themes_dir
    tdesign_src = args.tdesign_src

    if not tdesign_src.is_dir():
        sys.exit(f"tdesign source missing: {tdesign_src}")
    missing = [n for n in AI_ICONS if n != "aideck"
               and not (tdesign_src / f"{n}.svg").is_file()]
    if missing:
        sys.exit(f"missing tdesign icons: {missing}")

    # 1) Generate normalized SVGs into a scratch dir (per size).
    scratch = REPO / ".work" / "ai-burn"
    scratch.mkdir(parents=True, exist_ok=True)
    for size, _ in SIZES:
        d = scratch / str(size)
        d.mkdir(exist_ok=True)
        for name in AI_ICONS:
            src_name = DECK_SOURCE if name == "aideck" else name
            text = (tdesign_src / f"{src_name}.svg").read_text(encoding="utf-8")
            (d / f"{name}.svg").write_text(
                normalize_svg_text(text, size, size) + "\n", encoding="utf-8")

    # 2) Rasterize each size once into a PNG pool (skip if already present).
    png_pool = scratch / "png"
    for size, _ in SIZES:
        out_png_dir = png_pool / str(size)
        existing = len(list(out_png_dir.glob("*.png"))) if out_png_dir.is_dir() else 0
        if existing >= len(AI_ICONS):
            print(f"png pool {size} already has {existing} files; skipping rasterize")
            continue
        rasterize_batch(SOFFICE, scratch / str(size), out_png_dir)

    total = 0
    # 3) SVG themes get the normalized SVG.
    for theme in SVG_THEMES:
        cmd_dir = themes_root / theme / "cmd"
        if not cmd_dir.is_dir():
            print(f"skip missing theme dir: {theme}")
            continue
        for name in AI_ICONS:
            for size, prefix in SIZES:
                sub = "32" if size == 32 else ""
                rel = cmd_dir / sub / f"{prefix}{name}.svg"
                rel.parent.mkdir(parents=True, exist_ok=True)
                src_name = DECK_SOURCE if name == "aideck" else name
                text = (tdesign_src / f"{src_name}.svg").read_text(encoding="utf-8")
                rel.write_text(
                    normalize_svg_text(text, size, size) + "\n", encoding="utf-8")
                total += 1
    # 4) PNG themes get the rasterized PNG.
    for theme in PNG_THEMES:
        cmd_dir = themes_root / theme / "cmd"
        if not cmd_dir.is_dir():
            print(f"skip missing theme dir: {theme}")
            continue
        for name in AI_ICONS:
            for size, prefix in SIZES:
                sub = "32" if size == 32 else ""
                png = png_pool / str(size) / f"{name}.png"
                if not png.is_file():
                    print(f"WARN: no png for {name}@{size}")
                    continue
                rel = cmd_dir / sub / f"{prefix}{name}.png"
                rel.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(png, rel)
                total += 1

    print(f"burned AI icon set into {len(BUNDLED_THEMES)} themes ({total} files)")
    print(f"  SVG themes ({len(SVG_THEMES)}): {SVG_THEMES}")
    print(f"  PNG themes ({len(PNG_THEMES)}): {PNG_THEMES}")


if __name__ == "__main__":
    main()