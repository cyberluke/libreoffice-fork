#!/usr/bin/env bash
#
# NAI Office macOS build driver (Xcode + autogen.sh + make).
#
# Mirrors v271/build_win.sh but for macOS: no vcvars, no MSYS2 path
# hacks -- a plain Xcode/CLT command line environment is expected.
#
# Usage:
#   ./v271/build_mac.sh                    # configure (once) + build
#   FORCE_AUTOGEN=1 ./v271/build_mac.sh    # re-run autogen.sh
#   JOBS=8 ./v271/build_mac.sh             # override parallelism
#   AUTOGEN_ONLY=1 ./v271/build_mac.sh     # stop after configure
#   PREP_VENDOR=0 ./v271/build_mac.sh      # skip OXT native-wheel vendoring
#
# Configuration: distro-configs/NAIOfficeMac.conf (see that file for
# notes on DMG packaging and CPU optimization).
#
# CPU optimization: off by default on macOS (ARM64 hosts cannot use
# x86 SIMD flags). For Intel Macs pass
#   FORCE_AUTOGEN=1 ./autogen.sh --with-distro=NAIOfficeMac \
#       --enable-naioffice-cpu-opt
# or add --enable-naioffice-cpu-opt to the distro config.

set -e
cd "$(dirname "$0")/.."

# RAM-aware parallelism: default to the core count, capped like the
# Windows driver (LO's PARALLELISM default is 24; PCH-heavy clang jobs
# on laptops swap below that).
NCORES="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
export PARALLELISM="${PARALLELISM:-$NCORES}"
export JOBS="${JOBS:-$PARALLELISM}"

# OXT native wheels (cryptography etc.) for this platform. Idempotent;
# fails loudly but non-fatally so the OXT can still be built without
# them (features degrade gracefully). PREP_VENDOR=0 skips this step.
if [ "${PREP_VENDOR:-1}" = "1" ]; then
    ./officelabs/writeragent/officelabs-build/vendor_native.sh "${PYTHON:-python3}" \
        || echo "WARN: vendor_native.sh failed; the OXT will lack platform-native wheels"
fi

# Configure once (FORCE_AUTOGEN=1 re-runs autogen.sh, e.g. after
# editing the distro config).
if [ ! -f config_host.mk ] || [ -n "$FORCE_AUTOGEN" ]; then
    echo "== running autogen.sh --with-distro=NAIOfficeMac =="
    ./autogen.sh --with-distro=NAIOfficeMac
fi

if [ -n "$AUTOGEN_ONLY" ]; then
    echo "== autogen complete, stopping (AUTOGEN_ONLY) =="
    exit 0
fi

echo "== /usr/bin/make -j${JOBS} $* =="
exec make -j"${JOBS}" "$@"