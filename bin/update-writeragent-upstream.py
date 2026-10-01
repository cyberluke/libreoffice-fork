#!/usr/bin/env python3
"""Update the vendored WriterAgent snapshot in officelabs/writeragent/.

Usage:
    python bin/update-writeragent-upstream.py <explicit-sha>
    python bin/update-writeragent-upstream.py <sha> --from-clone <path>
    python bin/update-writeragent-upstream.py <sha> --dry-run

Behavior (deterministic, never silent):
    1. accepts one explicit upstream commit SHA (never floating master);
    2. fetches that exact revision from https://github.com/KeithCu/writeragent
       (or reads it from an existing local clone with --from-clone);
    3. verifies the checked-out tree is exactly that SHA;
    4. syncs the build-required subset into officelabs/writeragent/;
    5. preserves OfficeLabs-only paths (officelabs-build/, vendor/,
       build-tools/, UPSTREAM, INTEGRATION.md, OFFICELABS_PATCHES.md);
    6. records the new SHA in officelabs/writeragent/UPSTREAM (unless
       --dry-run);
    7. reports upstream files that changed and any file that differs from
       pristine upstream inside the vendored tree (a conflict requiring
       review).

This script never commits, never resolves conflicts by itself and never
touches floating master.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

REPO_URL = "https://github.com/KeithCu/writeragent"
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
VENDOR_DIR = os.path.join(ROOT, "officelabs", "writeragent")
UPSTREAM_FILE = os.path.join(VENDOR_DIR, "UPSTREAM")

# Build-required subset. Everything else in the upstream tree (docs/, tests/,
# website/, wiki/, Showcase/, compute_service/, native/, contrib/,
# extension-core/, extension-harper/, .github/) is dev/test/CI surface that
# the OfficeLabs build does not consume. See OFFICELABS_PATCHES.md.
VENDORED_TOP = ("extension", "plugin", "locales", "scripts")
VENDORED_ROOT_FILES = (
    "AGENTS.md", "LICENSE", "Makefile", "pyproject.toml",
    "requirements-vendor.txt", "README.md",
)
SCRIPTS_EXCLUDE = {
    # Large dev/test data files not needed to build the OXT.
    "longdocsample.odt", "lo_test_10k.csv", "lo_test_10k.ods",
}
# Dev-only scripts/ subdirectories not needed to build the OXT (eval
# benchmark data and tooling). Everything else under scripts/ is vendored.
SCRIPTS_EXCLUDE_DIRS = {
    "prompt_optimization",
}
# OfficeLabs-owned paths inside the vendored dir that are never overwritten.
OFFICELABS_PATHS = {
    "officelabs-build", "vendor", "build-tools", "UPSTREAM",
    "INTEGRATION.md", "OFFICELABS_PATCHES.md",
}


def run(cmd, cwd=None, check=True):
    proc = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if check and proc.returncode != 0:
        sys.exit("command failed (%s):\n%s\n%s"
                 % (" ".join(cmd), proc.stdout, proc.stderr))
    return proc.stdout.strip()


def read_upstream_file():
    if not os.path.isfile(UPSTREAM_FILE):
        return None
    info = {}
    with open(UPSTREAM_FILE, encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if "=" in line and not line.startswith("#"):
                key, _, value = line.partition("=")
                info[key.strip()] = value.strip()
    return info


def fetch_upstream(sha, from_clone):
    tmp = tempfile.mkdtemp(prefix="writeragent-upstream-")
    try:
        if from_clone:
            run(["git", "clone", "--no-checkout", from_clone, tmp])
        else:
            run(["git", "init", "-q", tmp])
            run(["git", "remote", "add", "origin", REPO_URL], cwd=tmp)
            run(["git", "fetch", "-q", "--depth", "1", "origin", sha], cwd=tmp)
        run(["git", "checkout", "-q", "--detach", sha], cwd=tmp)
        head = run(["git", "rev-parse", "HEAD"], cwd=tmp)
        if head != sha:
            sys.exit("requested %s but checkout is %s" % (sha, head))
        return tmp
    except SystemExit:
        shutil.rmtree(tmp, ignore_errors=True)
        raise


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def collect_upstream_files(src):
    files = {}
    for top in VENDORED_TOP:
        base = os.path.join(src, top)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames
                           if d not in (".git", "__pycache__")]
            for fn in filenames:
                rel = os.path.relpath(os.path.join(dirpath, fn), src)
                files[rel] = os.path.join(dirpath, fn)
    for name in VENDORED_ROOT_FILES:
        p = os.path.join(src, name)
        if os.path.isfile(p):
            files[name] = p
    return files


def sync(src, dry_run):
    upstream = collect_upstream_files(src)
    current = read_upstream_file() or {}
    old_sha = current.get("commit", "unknown")

    os.makedirs(VENDOR_DIR, exist_ok=True)
    changed = []
    conflicts = []
    for rel, up_path in sorted(upstream.items()):
        rel_parts = rel.split(os.sep)
        if rel_parts[0] == "scripts":
            if os.path.basename(rel) in SCRIPTS_EXCLUDE:
                continue
            if len(rel_parts) > 1 and rel_parts[1] in SCRIPTS_EXCLUDE_DIRS:
                continue
        dst = os.path.join(VENDOR_DIR, rel)
        if os.path.isfile(dst):
            if sha256_of(dst) == sha256_of(up_path):
                continue
            changed.append(rel)
            # Any byte difference between the vendored copy and pristine
            # upstream is a local divergence that must be reviewed (the
            # OfficeLabs overlays live in officelabs-build/ instead).
            conflicts.append(rel)
        else:
            changed.append(rel)
        if not dry_run:
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(up_path, dst)

    overlays_overlap = []
    for name in sorted(os.listdir(os.path.join(VENDOR_DIR, "officelabs-build"))):
        up = os.path.join(src, "extension", name)
        if os.path.isfile(up):
            overlays_overlap.append("officelabs-build/%s (upstream: extension/%s)"
                                    % (name, name))

    if not dry_run:
        with open(UPSTREAM_FILE, "w", encoding="utf-8") as fh:
            fh.write("# WriterAgent upstream pin for the OfficeLabs integration.\n"
                     "#\n"
                     "# OfficeLabs builds WriterAgent from this exact revision. Never track\n"
                     "# floating master for release builds. Update with:\n"
                     "#\n"
                     "#   python bin/update-writeragent-upstream.py <explicit-sha>\n"
                     "#\n"
                     "repository=%s\n"
                     "commit=%s\n" % (REPO_URL, src_head(src)))

    print("old upstream: %s" % old_sha)
    print("new upstream: %s" % src_head(src))
    print("")
    print("upstream files changed: %d" % len(changed))
    for rel in changed:
        print("  + %s" % rel)
    print("")
    print("OfficeLabs-overlaid files changed upstream: %d"
          % len(overlays_overlap))
    for rel in overlays_overlap:
        print("  ! %s" % rel)
    print("")
    if conflicts:
        print("conflicts requiring review (vendored file differs from "
              "pristine upstream): %d" % len(conflicts))
        for rel in conflicts:
            print("  !! %s" % rel)
    else:
        print("conflicts requiring review: none")
    if dry_run:
        print("\n(dry run: no files were modified)")


def src_head(src):
    return run(["git", "rev-parse", "HEAD"], cwd=src)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sha", help="explicit upstream commit SHA")
    parser.add_argument("--from-clone", metavar="PATH",
                        help="sync from an existing local clone instead of "
                             "fetching from GitHub")
    parser.add_argument("--dry-run", action="store_true",
                        help="report only; do not modify the vendored tree")
    args = parser.parse_args()

    if len(args.sha) != 40 or any(c not in "0123456789abcdef"
                                  for c in args.sha):
        sys.exit("not a full commit SHA: %s" % args.sha)

    src = fetch_upstream(args.sha, args.from_clone)
    try:
        sync(src, args.dry_run)
    finally:
        shutil.rmtree(src, ignore_errors=True)


if __name__ == "__main__":
    main()