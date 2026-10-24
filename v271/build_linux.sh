#!/usr/bin/env bash
#
# NAI Office Linux build driver (autogen.sh + make).
#
# Mirrors v271/build_win.sh but for Linux: no vcvars, no MSYS2 path
# hacks -- a plain POSIX toolchain environment is expected.
#
# Usage:
#   ./v271/build_linux.sh                   # configure (once) + build
#   FORCE_AUTOGEN=1 ./v271/build_linux.sh   # re-run autogen.sh
#   JOBS=8 ./v271/build_linux.sh            # override parallelism
#   AUTOGEN_ONLY=1 ./v271/build_linux.sh    # stop after configure
#   PREP_VENDOR=0 ./v271/build_linux.sh     # skip OXT native-wheel vendoring
#
# Configuration: distro-configs/NAIOfficeLinux.conf (see that file for
# notes on DEB packaging and CPU optimization).
#
# CPU optimization: off by default on Linux; to compile the whole
# build for AVX2 + SSE4.1 + SSE4.2 (x86-64 only), add
# --enable-naioffice-cpu-opt to the distro config and re-run with
# FORCE_AUTOGEN=1.

set -e
cd "$(dirname "$0")/.."

# RAM-aware parallelism: default to the core count, capped like the
# Windows driver (LO's PARALLELISM default is 24; PCH-heavy clang/gcc
# jobs on smaller machines swap below that).
NCORES="$(nproc 2>/dev/null || echo 4)"
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
    echo "== running autogen.sh --with-distro=NAIOfficeLinux =="
    ./autogen.sh --with-distro=NAIOfficeLinux
fi

if [ -n "$AUTOGEN_ONLY" ]; then
    echo "== autogen complete, stopping (AUTOGEN_ONLY) =="
    exit 0
fi

echo "== /usr/bin/make -j${JOBS} $* =="
exec make -j"${JOBS}" "$@"