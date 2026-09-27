#!/bin/sh
# Builds v271-office.oxt from the extension source tree.
# Usage:  ./build_oxt.sh
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="$ROOT/extension"
OUT="$ROOT/v271-office.oxt"

if [ ! -f "$SRC/description.xml" ]; then
    echo "extension/description.xml not found; run from v271/office-extension/" >&2
    exit 1
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cp -R "$SRC"/. "$TMP"/
find "$TMP" -name '.DS_Store' -delete
find "$TMP" -type d -name '__pycache__' -prune -exec rm -rf {} +
find "$TMP" -name '*.pyc' -delete

rm -f "$OUT"
(cd "$TMP" && zip -qr "$OUT" .)

echo "Built $OUT"