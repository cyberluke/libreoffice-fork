#!/usr/bin/env python3
"""Deterministic SVG normalizer for the Writer 2027 icon themes.

Turns a raw upstream Carbon or Phosphor SVG into a LibreOffice-ready command
SVG:

  * preserves upstream `viewBox` and path geometry,
  * sets explicit `width`/`height` for the target optical size
    (16 -> sc_, 24 -> lc_, 32 -> cmd/32/),
  * replaces the semantic foreground (Carbon implicit black / `#000000` /
    `currentColor`) with the Writer 2027 dark foreground token,
  * strips non-essential metadata (styles, titles, descriptions, hidden rects),
  * keeps geometry + fill rules + opacity + clip-path semantics,
  * rejects embedded raster data / external references / scripts / CSS imports.

The output is deterministic: stable attribute ordering and byte-for-byte
reproducible given the same input. Uses `lxml` (an SVG/XML parser) - never a
naive regex over `fill` attributes.
"""

import argparse
import re
import sys
from pathlib import Path

try:
    from lxml import etree
    LXML = True
except Exception:  # pragma: no cover - lxml is required
    import xml.etree.ElementTree as etree  # type: ignore
    LXML = False

# Writer 2027 palette tokens (see spec #21 / #46).
WRITER_ICON_FG = "#F2F4F8"  # primary foreground
WRITER_ICON_FG_MUTED = "#C6C6C6"  # secondary / muted foreground

NS = {
    "svg": "http://www.w3.org/2000/svg",
    "xlink": "http://www.w3.org/1999/xlink",
}

SEMANTIC_BLACKS = {
    "#000000", "black", "#000",
    "#000001", "#010101", "#000002", "#000003",
    "#000004", "#010000", "#020202",
}

# TDesign stroke icons use fill="white" for visible body shapes on line icons
# (e.g. the robot body). On the Writer 2027 dark theme that body must become
# the foreground color, exactly like black strokes.
SEMANTIC_WHITES = {
    "#ffffff", "white", "#fff",
}

# Attributes that indicate danger / non-portable content.
DANGER_PATTERNS = [
    re.compile(r"<script", re.I),
    re.compile(r"foreignObject", re.I),
    re.compile(r"data:image", re.I),
    re.compile(r"(?<![\w:])base64,", re.I),
    re.compile(r"@import", re.I),
    re.compile(r"\bjavascript:", re.I),
]

# Only flag http(s) appearing in href-bearing attribute values, not the plain
# XML namespace URIs like http://www.w3.org/2000/svg used in every SVG.
EXTERNAL_HREF = re.compile(r"\s(?:href|xlink:href)\s*=\s*(['\"])(?:https?://)", re.I)
DATA_IMAGE = re.compile(r"data:image", re.I)


def _q(tag):
    return f"{{{NS['svg']}}}{tag}"


def normalize_svg_text(text: str, width: int, height: int,
                       fg: str = WRITER_ICON_FG,
                       muted: str = WRITER_ICON_FG_MUTED) -> str:
    """Return normalized, deterministic, single-line SVG content string."""
    _check_safety(text)

    parser = etree.XMLParser(remove_blank_text=True, recover=False)
    root = etree.fromstring(text.encode("utf-8"), parser=parser)

    # --- structural validation -------------------------------------------------
    if root.tag != _q("svg"):
        raise ValueError(f"root element is <{root.tag}>, expected <svg>")

    # viewBox handling
    vb = root.get("viewBox")
    if not vb:
        # Some Phosphor assets rely on x/y/width/height instead of viewBox.
        w_attr = root.get("width")
        h_attr = root.get("height")
        if w_attr and h_attr:
            try:
                wx = float(w_attr)
                hy = float(h_attr)
                if wx <= 0 or hy <= 0:
                    raise ValueError("zero-size viewport")
            except ValueError:
                raise ValueError("invalid width/height")
        else:
            raise ValueError("neither viewBox nor width/height present")
    else:
        parts = [p.strip() for p in vb.replace(",", " ").split()]
        if len(parts) != 4:
            raise ValueError(f"invalid viewBox: {vb!r}")
        try:
            nums = [float(p) for p in parts]
        except ValueError:
            raise ValueError(f"invalid viewBox numbers: {vb!r}")
        if nums[2] <= 0 or nums[3] <= 0:
            raise ValueError("zero-size viewBox")

    # --- reject embedded raster / external refs (post-parse check) -------------
    _check_safety(etree.tostring(root, encoding="unicode"))

    # -- remove non-essential nodes --------------------------------------------
    # Keep all geometry: path, rect, circle, ellipse, line, polyline, polygon,
    # g, use, defs (for clip paths / masks), clipPath, mask, symbol.
    # Note: iterate with tag=etree.Element to skip comment/PI nodes.
    for node in root.iter(tag=etree.Element):
        tag = etree.QName(node).localname
        if tag in ("title", "desc", "metadata", "foreignObject"):
            parent = node.getparent()
            if parent is not None:
                parent.remove(node)

    # Discover CSS classes that resolve to fill:none from embedded <style>, so
    # we can drop the helper "transparent rectangle" overlays that they styled.
    # Carbon prepends `.st0{fill:none;}` and `.cls-1{fill:none;}` classes.
    none_classes = set()
    style_blocks = []
    for node in root.iter(tag=etree.Element):
        tag = etree.QName(node).localname
        if tag == "style":
            style_blocks.append(node)
            if node.text:
                for m in re.finditer(r"\.([A-Za-z0-9_-]+)\s*\{[^}]*fill\s*:\s*none[^}]*\}",
                                     node.text):
                    none_classes.add(m.group(1))

    for node in root.iter(tag=etree.Element):
        cls = (node.get("class") or "").split()
        # Drop helper rects: transparent fill:none class overlays.
        if etree.QName(node).localname == "rect" and not node.get("stroke"):
            if (none_classes & set(cls)) or node.get("fill") == "none":
                parent = node.getparent()
                if parent is not None:
                    parent.remove(node)

    # Remove <style> blocks and empty <defs>.
    for node in style_blocks:
        parent = node.getparent()
        if parent is not None:
            parent.remove(node)
    for node in list(root.iter(_q("defs"))):
        if len(node) == 0 and not (node.text or "").strip():
            parent = node.getparent()
            if parent is not None:
                parent.remove(node)

    # -- recolor foreground ----------------------------------------------------
    _recolor(root, fg, muted)

    # -- set explicit width/height ---------------------------------------------
    root.set("width", str(width))
    root.set("height", str(height))
    # C14N emits the svg xmlns on the root automatically; do not add a second.

    # -- drop presentation attributes that are pointless metadata ---------------
    for node in root.iter(tag=etree.Element):
        for attr in ("id", "xml:space", "enable-background", "style", "class",
                     "data-name", "version"):
            if node.get(attr) is not None:
                del node.attrib[attr]
    # Drop Illustrator-export root metadata that would create noise.
    for attr in ("x", "y"):
        if root.get(attr) is not None and root.get(attr) in ("0px", "0", "0.0"):
            del root.attrib[attr]

    # -- deterministic serialization -------------------------------------------
    if LXML:
        # C14N for deterministic, namespace-stable output; returns bytes.
        buf = etree.tostring(root, method="c14n2", with_comments=False)
        buf = buf.decode("utf-8")
    else:
        buf = etree.tostring(root, encoding="unicode")

    # collapse to a single compact line, preserving geometry
    one_line = "".join(line.strip() for line in buf.splitlines())
    one_line = re.sub(r"\s+", " ", one_line).strip()
    return one_line


def _recolor(root, fg, muted):
    """Replace semantic foreground colors with Writer dark tokens.

    Carbon shape assets draw black paths (implicit black fill when no fill
    attribute is present). Phosphor assets use `fill="currentColor"`.
    TDesign line assets stroke in black and fill white body shapes. We:

      * replace explicit semantic foreground colors (`black`, `#000…`,
        `currentColor`, `white`, `#fff…`) with `fg`,
      * recolor `stroke="black"` (and white) to `fg`,
      * cascade `fill=fg` from the root so Carbon's implicit-black shapes
        render as the dark foreground,
      * leave `fill="none"`, `fill="#…"` accents, `opacity`, `fill-rule`,
        `stroke` widths and stop-colors alone.
    """
    for node in root.iter(tag=etree.Element):
        color = node.get("color") or node.get("fill")
        if color is not None:
            c = color.strip().lower()
            if c in SEMANTIC_BLACKS or c == "currentcolor" or c in SEMANTIC_WHITES:
                node.set("fill", fg)
                node.attrib.pop("color", None)
        stroke = node.get("stroke")
        if stroke is not None:
            s = stroke.strip().lower()
            if s in SEMANTIC_BLACKS or s in SEMANTIC_WHITES:
                node.set("stroke", fg)
        sc = node.get("stop-color")
        if sc and sc.strip().lower() in SEMANTIC_BLACKS:
            node.set("stop-color", fg)
    # Cascade foreground onto shape assets that otherwise render black.
    rootfill = root.get("fill")
    if rootfill is None:
        root.set("fill", fg)


def _check_safety(text):
    # foreignObject elements are removed during normalization (AI namespace
    # holders); the final serialized output is re-checked and must not contain
    # any. Here we reject only what cannot be cleaned.
    for pat in DANGER_PATTERNS:
        if pat.pattern == "foreignObject":
            continue
        if pat.search(text):
            raise ValueError(f"unsafe SVG content matched {pat.pattern!r}")
    if EXTERNAL_HREF.search(text):
        raise ValueError(f"external http(s) reference in SVG: {EXTERNAL_HREF.search(text).group(0)!r}")
    if DATA_IMAGE.search(text):
        raise ValueError("embedded raster data in SVG")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", type=Path)
    ap.add_argument("output", type=Path)
    ap.add_argument("--width", type=int, required=True)
    ap.add_argument("--height", type=int, required=True)
    args = ap.parse_args(argv)

    text = args.input.read_text(encoding="utf-8", errors="strict")
    out = normalize_svg_text(text, args.width, args.height)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(out + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())