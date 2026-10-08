#!/usr/bin/env python3
"""Writer 2027 visual golden comparator (spec section 26).

Compares an actual Writer 2027 screenshot against an expected golden PNG using a
bounded, antialiasing-tolerant per-channel diff. The image is the contract: a
regression that shifts rows, overlaps a specimen, or moves the popup to the
wrong place shows up here as a changed-pixel ratio or a large per-channel delta,
so "it compiles" can never again be claimed as "it renders".

Design goals (spec 21.3):
  - verify dimensions,
  - compute absolute per-channel difference,
  - ignore tiny antialiasing noise below a channel threshold,
  - calculate the changed-pixel percentage,
  - output a highlighted diff image,
  - fail when the threshold is exceeded.

CLI:

    python solenv/bin/writer2027_visual_compare.py \\
        --expected v271/goldens/writer2027/200/typesystem-default.png \\
        --actual workdir/screenshots/.../typesystem-default.png \\
        --diff workdir/writer2027-diffs/typesystem-default.png \\
        --max-changed-pixels 0.005 \\
        --channel-threshold 8

It prints metrics.json (expected/actual sizes, changedPixelsRatio,
maxChannelDelta, pass) and exits non-zero on failure. Pillow is the only
dependency. Do NOT raise the threshold so high that a giant overlapping "Aa"
can pass (spec 26).
"""

import argparse
import json
import os
import sys

try:
    from PIL import Image, ImageChops, ImageDraw
except ImportError:  # pragma: no cover - clear, actionable error
    sys.stderr.write(
        "writer2027_visual_compare.py requires Pillow: pip install Pillow\n"
    )
    sys.exit(2)


def _load(path):
    if not os.path.exists(path):
        sys.stderr.write("missing image: %s\n" % path)
        sys.exit(2)
    img = Image.open(path).convert("RGB")
    return img


def compare(expected_path, actual_path, diff_path, max_changed, channel_threshold):
    expected = _load(expected_path)
    actual = _load(actual_path)

    ew, eh = expected.size
    aw, ah = actual.size
    if (ew, eh) != (aw, ah):
        metrics = {
            "expectedWidth": ew,
            "expectedHeight": eh,
            "actualWidth": aw,
            "actualHeight": ah,
            "changedPixelsRatio": 1.0,
            "maxChannelDelta": 255,
            "reason": "dimension mismatch",
            "pass": False,
        }
        _write_metrics(metrics_path(expected_path), metrics)
        sys.stderr.write(
            "dimension mismatch: expected %dx%d, actual %dx%d\n" % (ew, eh, aw, ah)
        )
        return False, metrics

    # Absolute per-channel difference; a pixel counts as changed only if at
    # least one channel exceeds the antialiasing-noise threshold.
    diff = ImageChops.difference(expected, actual)
    bands = diff.split()  # R, G, B
    max_channel = 0
    changed = 0
    width, height = actual.size
    # Per-channel max delta over the whole image.
    for band in bands:
        # PIL histogram of the 8-bit band to find the max nonzero delta cheaply.
        hist = band.histogram()
        for v in range(255, 0, -1):
            if hist[v]:
                max_channel = max(max_channel, v)
                break

    px_diff = diff.load()
    # Build a monochrome "changed" mask honoring the channel threshold.
    mask = Image.new("1", actual.size, 0)
    pm = mask.load()
    for y in range(height):
        for x in range(width):
            r, g, b = px_diff[x, y]
            if max(r, g, b) >= channel_threshold:
                pm[x, y] = 1
                changed += 1

    changed_ratio = changed / float(width * height) if width * height else 1.0
    passed = changed_ratio <= max_changed

    if diff_path:
        # Highlight: dim unchanged areas, paint changed pixels bright red so a
        # human can immediately see what moved.
        os.makedirs(os.path.dirname(diff_path) or ".", exist_ok=True)
        view = actual.copy()
        overlay = Image.new("RGB", actual.size, (255, 0, 0))
        # Where the mask is set, keep the actual pixel but outline; elsewhere dim.
        view = Image.blend(view, Image.new("RGB", actual.size, (0, 0, 0)), 0.55)
        view.paste(actual, (0, 0), mask)
        # draw a visible outline on the changed region
        d = ImageDraw.Draw(view)
        # approximate bounding box of changes
        if changed:
            bbox = mask.getbbox()
        else:
            bbox = None
        if bbox:
            d.rectangle(bbox, outline=(255, 0, 0), width=2)
        view.save(diff_path)
        del overlay
        del d

    metrics = {
        "expectedWidth": ew,
        "expectedHeight": eh,
        "actualWidth": aw,
        "actualHeight": ah,
        "changedPixelsRatio": round(changed_ratio, 6),
        "maxChannelDelta": max_channel,
        "channelThreshold": channel_threshold,
        "maxChangedPixels": max_changed,
        "pass": bool(passed),
    }
    _write_metrics(metrics_path(expected_path), metrics)
    return passed, metrics


def metrics_path(expected_path):
    # metrics.json lives next to the diff / run artifacts per spec 21.3.
    base = os.path.splitext(os.path.basename(expected_path))[0]
    return os.path.join("workdir", "writer2027-diffs", base + ".metrics.json")


def _write_metrics(path, metrics):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(metrics, fh, indent=2)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Writer 2027 visual golden comparator"
    )
    parser.add_argument("--expected", required=True, help="golden PNG")
    parser.add_argument("--actual", required=True, help="captured PNG")
    parser.add_argument("--diff", default=None, help="diff highlight PNG")
    parser.add_argument(
        "--max-changed-pixels",
        type=float,
        default=0.005,
        help="max changed-pixel ratio to pass (default 0.005)",
    )
    parser.add_argument(
        "--channel-threshold",
        type=int,
        default=8,
        help="per-channel delta (0-255) below which a pixel is antialiasing "
        "noise and not counted (default 8)",
    )
    args = parser.parse_args(argv)

    passed, metrics = compare(
        args.expected,
        args.actual,
        args.diff,
        args.max_changed_pixels,
        args.channel_threshold,
    )
    sys.stderr.write(json.dumps(metrics, indent=2) + "\n")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())