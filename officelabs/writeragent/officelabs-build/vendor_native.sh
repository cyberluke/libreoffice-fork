#!/usr/bin/env bash
#
# vendor_native.sh — (re)vendor platform-native Python wheels for the
# WriterAgent OXT on the CURRENT build host.
#
# The committed vendor/ tree intentionally holds ONLY pure-Python packages
# (they run unchanged on Windows/macOS/Linux). Compiled wheels are
# platform- and sometimes Python-version-specific (cryptography's Rust
# bindings: .pyd on Windows, .so on Linux, .so/.dylib on macOS), so they are
# installed per host by this script instead of being committed.
#
# Behavior:
#   * Idempotent: if vendor/ already contains a native cryptography backend
#     for THIS platform, it exits immediately (no network, no changes).
#   * Otherwise it runs `pip install --target vendor` for the pinned native
#     wheels (network required only on the first run per platform, or after
#     switching build OS in the same checkout).
#   * It prunes native artifacts of OTHER platforms (a checkout previously
#     vendored on Windows must not leak .pyd/.exe into a macOS bundle).
#
# Invariants kept:
#   * The OXT assembly step (build-wa-oxt.sh / scripts/build_oxt.py) stays
#     fully offline -- vendoring happens HERE, before the bundle step.
#   * vendor/ stays the single source that scripts/build_oxt.py copies into
#     plugin/lib/ at bundle time.
#
# Usage:
#   vendor_native.sh [python]            # default: python3
#   PREP_VENDOR=0 ...                    # skip this step from build drivers
#
# Exit codes: 0 = ok (native wheels present or installed),
#             1 = failure (no native wheels and pip install failed)
set -euo pipefail

PYTHON="${1:-python3}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"          # officelabs/writeragent
VENDOR="$ROOT/vendor"

# Pinned native wheels (abi3 wheels work with any Python >= 3.7; see
# vendor/PINNED.txt for the pure-Python pins).
CRYPTOGRAPHY_PIN="cryptography==49.0.0"

# Per-platform native/foreign extension sets.
case "$(uname -s)" in
    Linux)      NATIVE_EXTS="so" ; FOREIGN_EXTS="pyd exe dll dylib" ;;
    Darwin)     NATIVE_EXTS="so dylib" ; FOREIGN_EXTS="pyd exe dll" ;;
    MINGW*|MSYS*|CYGWIN*)
                NATIVE_EXTS="pyd exe dll" ; FOREIGN_EXTS="so dylib" ;;
    *)
        echo "WARN: vendor_native.sh: unknown platform '$(uname -s)', leaving vendor/ untouched" >&2
        exit 0
        ;;
esac

if [ ! -d "$VENDOR" ]; then
    echo "ERROR: vendor_native.sh: $VENDOR not found -- run from the LibreOffice tree root" >&2
    exit 1
fi

# --- 1. Prune native artifacts of other platforms -------------------------
pruned=0
while IFS= read -r -d '' f; do
    ext="${f##*.}"
    case " $FOREIGN_EXTS " in
        *" $ext "*)
            rm -f "$f"
            pruned=$((pruned + 1))
            ;;
    esac
done < <(find "$VENDOR" -type f \( \
    -name '*.pyd' -o -name '*.exe' -o -name '*.dll' \
    -o -name '*.so' -o -name '*.dylib' \) -print0 2>/dev/null || true)

# Windows console-script wrappers (vendor/bin/*.exe) are useless elsewhere.
if [[ "$(uname -s)" != MINGW* && "$(uname -s)" != MSYS* && "$(uname -s)" != CYGWIN* ]]; then
    if [ -d "$VENDOR/bin" ]; then
        rm -rf "$VENDOR/bin"
        pruned=$((pruned + 1))
    fi
fi
if [ "$pruned" -gt 0 ]; then
    echo "vendor_native.sh: pruned $pruned foreign-platform native artifact(s) from vendor/"
fi

# --- 2. Idempotency: native backend already present? ----------------------
if find "$VENDOR" -type f \( \
        -name "*.pyd" -o -name "*.exe" -o -name "*.dll" \
        -o -name "*.so" -o -name "*.dylib" \) 2>/dev/null | grep -q .; then
    echo "vendor_native.sh: native wheels already vendored for $(uname -s) -- nothing to do"
    exit 0
fi

# --- 3. Vendor the native wheels for this platform ------------------------
echo "vendor_native.sh: vendoring native wheels for $(uname -s) into vendor/ ..."
if ! "$PYTHON" -m pip install --target "$VENDOR" --upgrade --no-compile \
        --only-binary=:all: "$CRYPTOGRAPHY_PIN"; then
    echo "ERROR: vendor_native.sh: pip install failed for $CRYPTOGRAPHY_PIN" >&2
    echo "       The OXT will still build, but without the platform-native" >&2
    echo "       cryptography wheels (features using them degrade gracefully)." >&2
    echo "       Check that '$PYTHON' has pip and network access to PyPI." >&2
    exit 1
fi
echo "vendor_native.sh: done (cryptography vendored for $(uname -s))"