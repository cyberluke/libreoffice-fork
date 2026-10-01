#!/usr/bin/env bash
#
# build-wa-oxt.sh — deterministic WriterAgent.oxt build for OfficeLabs.
#
# Builds the bundled OfficeLabs AI extension entirely from the vendored
# WriterAgent snapshot in officelabs/writeragent/. The source tree is never
# mutated: everything runs on a fresh copy under the gbuild workdir.
#
# Invariants (see officelabs/writeragent/INTEGRATION.md):
#   * no network access (no uv, no pip, no GitHub, no curl of .oxt)
#   * no unopkg add / no LibreOffice launch / no $HOME modification
#   * no dependency on a developer .venv or an installed WriterAgent
#   * uses the upstream packaging logic (scripts/build_oxt.py) untouched
#
# Usage:
#   build-wa-oxt.sh <vendored-src> <workdir> <output-oxt> <python>
#
#   <vendored-src>  officelabs/writeragent (read-only input)
#   <workdir>       scratch dir for the source copy + generated files
#   <output-oxt>    absolute path of the produced WriterAgent.oxt
#   <python>        python executable name; the recipe must ensure it
#                   resolves (the LibreOffice-internal Python lives in the
#                   install dirs, which the recipe prepends to PATH)
#
set -euo pipefail

SRC="$1"
WORK="$2"
OUT="$3"
PY="$4"

STAGE="$WORK/src"

rm -rf "$WORK"
mkdir -p "$WORK"

# 1. Fresh copy of the vendored tree. Copying every time keeps the build
#    reproducible: stale generated artifacts can never leak from the source
#    checkout into the OXT.
cp -a "$SRC/." "$STAGE/"

# Remove state that is regenerated below or provided by the build environment.
rm -rf \
    "$STAGE/vendor" \
    "$STAGE/build-tools" \
    "$STAGE/officelabs-build" \
    "$STAGE/build" \
    "$STAGE/plugin/lib"

# Restore the vendored runtime packages (copied into plugin/lib/ by
# scripts/build_oxt.py at bundle time).
cp -a "$SRC/vendor" "$STAGE/vendor"

# 2. OfficeLabs overlays — applied BEFORE manifest generation so that
#    extension/description.xml is generated from the OfficeLabs template.
#    Branding touches only user-visible strings; internal identifiers
#    (org.extension.writeragent.*) are preserved for upstream syncability.
cp -f "$SRC/officelabs-build/description.xml.tpl" \
      "$STAGE/extension/description.xml.tpl"

# Case-sensitive "WriterAgent" -> "OfficeLabs AI". In these files the only
# capital-W "WriterAgent" occurrences are user-visible labels; dispatch URLs
# and node identifiers are lowercase (org.extension.writeragent.*) and are
# left untouched.
sed -i 's/WriterAgent/OfficeLabs AI/g' \
    "$STAGE/extension/Addons.xcu" \
    "$STAGE/extension/registry/org/openoffice/Office/UI/Sidebar.xcu" \
    "$STAGE/extension/Dialogs/EvalDialog.xdl"

# 3. Upstream generation steps (same scripts `make build` uses upstream):
#    module manifest, XCS/XCU registry defaults, description.xml, META-INF,
#    dialog registry, then compiled .mo catalogs.
cd "$STAGE"
export PYTHONPATH="$SRC/build-tools"
"$PY" scripts/generate_manifest.py
"$PY" scripts/compile_translations.py locales

# 4. Upstream OXT assembly: release build (no tests, Debug menu stripped,
#    production code stripped) -> WriterAgent.oxt.
"$PY" scripts/build_oxt.py --no-tests --output "$OUT"

echo "OfficeLabs AI OXT built: $OUT"