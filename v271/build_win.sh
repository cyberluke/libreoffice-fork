#!/usr/bin/env bash
# V271 LibreOffice Windows build driver (MSYS2 + MSVC 2022).
#
# MUST be launched from a cmd shell that already ran vcvars64.bat, e.g.:
#   cmd /c "call \"C:\Program Files\Microsoft Visual Studio\2022\Community\
#           VC\Auxiliary\Build\vcvars64.bat\" && C:\msys64\usr\bin\bash.exe -lc \
#           /d/_SATIN_AI/LibreOffice/v271/build_win.sh"
#
# vcvars exports INCLUDE/LIB/LIBPATH, which the msys runtime passes through
# to bash unchanged (they are not path variables). The Windows Path itself is
# NOT inherited -- the MSYS2 login profile rebuilds PATH -- so this script
# re-adds the pieces the build needs:
#   - /c/lo-build   : native pkgconf-2.4.3.exe (configure requirement)
#   - git           : LO's get-submodules ./g helper
#   - MSVC bin      : cl.exe/link.exe for recipes that call them by name
#   - wslpath shim  : the top-level Makefile calls `wslpath -u` when MSYSTEM
#                     is set; MSYS2 has cygpath, not wslpath.
#
# Steps: run ./autogen.sh once (reads autogen.input), then make.

set -e
cd "$(dirname "$0")/.."

if [ -z "$INCLUDE" ] || [ -z "$LIB" ]; then
    echo "ERROR: INCLUDE/LIB are empty -- this script must run from a shell" >&2
    echo "       that has vcvars64.bat loaded (see the header comment)." >&2
    exit 1
fi

# vcvars LIB paths contain spaces and parentheses ("Program Files (x86)").
# Convert LIB to 8.3 short form (link.exe handles short paths fine and no
# gbuild recipe embeds $(LIB) into sh). INCLUDE is dropped entirely below.
to_short () {
    local out="" seg short
    local -a segs
    IFS=';' read -r -a segs <<< "$1"
    for seg in "${segs[@]}"; do
        [ -n "$seg" ] || continue
        if short="$(cygpath -d "$seg" 2>/dev/null)" && [ -n "$short" ]; then
            seg="$short"
        fi
        out="${out:+$out;}$seg"
    done
    printf '%s' "$out"
}

# config_host.mk exports LO's own UCRTVERSION (all-caps); vcvars64.bat sets
# UCRTVersion (mixed case). Both land in the environment of MSBuild-based
# external recipes (e.g. lcms2's Projects/VC2019/lcms2_DLL.vcxproj) and .NET's
# case-insensitive env dictionary throws MSB6001. Drop the vcvars duplicate;
# LO recipes that need the value use $(UCRTVERSION) from config_host.mk.
unset UCRTVersion

# LO's gbuild passes ALL include dirs via -I flags (SOLARINC, per-module
# INCLUDE make variable); the vcvars INCLUDE env var must NOT reach make:
# shell/CustomTarget_spsupp_idl.mk embeds $(INCLUDE) raw into an sh -c recipe
# for midl.exe, which breaks on both "Program Files (x86)" parens and the
# ';' separators. gbuild itself unsets INCLUDE for cl recipes. Keep a copy
# for reference only.
export INCLUDE_FULL="$INCLUDE" LIB_FULL="$LIB"
unset INCLUDE
export LIB="$(to_short "$LIB")"

# NOTE: the ATL include dir for the activex module is defined in make
# (solenv/gbuild/platform/com_MSC_class.mk, LOCAL patch) -- an environment
# variable would be rewritten to /c/... form by MSYS2 on the way into make.

# LO's Windows build expects Strawberry Perl (config_host.mk has an empty
# STRAWBERRY_PERL because configure found none). It is used by nmake-based
# externals (openssl's `$(PERL) Configure` under MSYSTEM) and by zip-timestamp
# rewriting. MSYS2's perl is a native Windows exe, so point STRAWBERRY_PERL at
# it (Windows path) instead of installing Strawberry Perl.
if [ -z "$STRAWBERRY_PERL" ] && [ -x /usr/bin/perl.exe ]; then
    export STRAWBERRY_PERL="$(cygpath -m /usr/bin/perl.exe)"
    echo "== STRAWBERRY_PERL not found; using MSYS2 perl: $STRAWBERRY_PERL =="
fi

# MSYS2 sets TMP/TEMP=/tmp (an MSYS path). Native Windows tools (nmake,
# cmd /C, cl.exe) interpret that as C:\tmp, which does not exist -- cl.exe
# then dies at process start with 0xc0000142 (STATUS_DLL_INIT_FAILED,
# CRT temp-file init). Point TMP/TEMP at the real Windows location of /tmp.
if [ "$TMP" = "/tmp" ] || [ "$TEMP" = "/tmp" ]; then
    TMPWIN="$(cygpath -m /tmp)"
    export TMP="$TMPWIN" TEMP="$TMPWIN"
    echo "== TMP/TEMP -> $TMPWIN =="
fi

# Firebird is built in a real, junction-free directory (see the LOCAL patch
# in external/firebird/ExternalProject_firebird.mk): its shared-memory trace
# regions are keyed by path string and the workdir junction gives every
# database attach two path spellings ("Wrong file for memory mapping").
mkdir -p /d/fb_build

export PATH="/c/lo-build:/c/Program Files/Git/cmd:/usr/bin:$PATH"

# ccache (LOCAL enhancement): wrap MSVC with ccache for fast incremental
# rebuilds. LO's configure auto-detects ccache by probing for the old
# "compiler_type = cl" log string, which ccache >= 4.12 renamed to "msvc";
# pre-wrapping CC/CXX with "ccache" makes configure take its documented
# "ccache seems to be included in a pre-defined CC and/or CXX" path instead
# (sets CCACHE_DEPEND_MODE=1, skips the probe). gbuild already emits
# CCACHE_SLOPPINESS/CCACHE_PCH_EXTSUM on every compile line.
if [ -n "$VCToolsInstallDir" ] && command -v ccache >/dev/null 2>&1 \
   && [ -z "$CC" ] && [ -z "$CXX" ] \
   && ! grep -q '^CXX_X64_BINARY=ccache' config_host.mk 2>/dev/null; then
    export CC="ccache cl" CXX="ccache cl"
    export CCACHE_DIR="${CCACHE_DIR:-/c/lo-build/ccache}"
    export CCACHE_MAXSIZE="${CCACHE_MAXSIZE:-30G}"
    export CCACHE_COMPRESS="${CCACHE_COMPRESS:-1}"
    export MSYS_NO_PATHCONV=1
    echo "== ccache enabled: CC=\"$CC\" CXX=\"$CXX\" dir=$CCACHE_DIR =="
fi

if [ -n "$VCToolsInstallDir" ]; then
    MSVCBIN="$(cygpath -u "$VCToolsInstallDir" 2>/dev/null)/bin/Hostx64/x64"
    if [ -x "$MSVCBIN/cl.exe" ]; then
        export PATH="$MSVCBIN:$PATH"
    fi
fi

if [ ! -x /usr/local/bin/wslpath ] && [ -f "$(dirname "$0")/wslpath-shim" ]; then
    mkdir -p /usr/local/bin
    cp "$(dirname "$0")/wslpath-shim" /usr/local/bin/wslpath
    chmod +x /usr/local/bin/wslpath
fi

echo "== env sanity: $(type -p cl.exe) / $(type -p git) / INCLUDE=${#INCLUDE} chars =="

# Configure once (also re-run when ccache was just enabled so config_host.mk
# picks up the ccache-wrapped CC/CXX).
if [ ! -f config_host.mk ] || { [ -n "$CC" ] && ! grep -q '^CXX_X64_BINARY=ccache' config_host.mk; }; then
    echo "== running autogen.sh =="
    ./autogen.sh
else
    echo "== config_host.mk present, skipping autogen =="
fi

# LOCAL fix: LO's configure writes `export PATH=<windows form>` into
# config_host.mk (the PATH it saw, rendered as C:/...;... entries). Exported
# raw from make, that Windows-form PATH breaks every recipe shell: uname/
# mkdir/date lookups fail ("command not found") because MSYS tools cannot
# resolve executables through C:/msys64/usr/bin-style entries. Convert the
# captured PATH to MSYS form (drive letters only: C:/x -> /c/x, ';' -> ':')
# so recipes keep the SDK/MSBuild/MSVC tool dirs (mt.exe, msbuild, rc, midl)
# AND find msys tools. NOTE: the drive pattern must be `[A-Za-z]:/` -- a bare
# `[A-Za-z]:` also matches the trailing letter of /bin: and corrupts every
# separator into a slash (/bin:/c -> /bi/n/c).
if grep -q '^export PATH=[A-Za-z]:/' config_host.mk 2>/dev/null; then
    winpath="$(sed -n 's/^export PATH=//p' config_host.mk | head -1)"
    posixpath="$(printf '%s' "$winpath" | sed -e 's|;|:|g' -e 's|\([A-Za-z]\):/|/\L\1/|g')"
    sed -i "s|^export PATH=.*|export PATH=$posixpath|" config_host.mk
    echo "== config_host.mk PATH converted to MSYS form =="
fi

# AUTOGEN_ONLY=1 stops after configure (for CI-style stepwise builds).
if [ -n "$AUTOGEN_ONLY" ]; then
    echo "== autogen complete, stopping (AUTOGEN_ONLY) =="
    exit 0
fi

# RAM-aware parallelism (LOCAL): config_host.mk sets PARALLELISM=24 by
# default; cl.exe with PCH uses ~1.5GB per job, so -j24 with limited free
# RAM swaps. Cap via PARALLELISM (make reads it with ?= so env wins), still
# overridable with JOBS/PARALLELISM env.
export PARALLELISM="${PARALLELISM:-16}"

# Build. The MSYS2 make is slower than Cygwin's but is the one that works
# in this environment: LO's "fast" MSVC-compiled make (make-4.2.1-msvc.exe)
# is a Cygwin binary and fails under MSYS2 (CreateProcess wslpath/mkdir).
# JOBS env overrides parallelism (default 16: under -j24 the external
# projects' configure steps intermittently failed with cross-contaminated
# cl.exe errors -- a parallel-build artifact that sequential runs never hit).
echo "== /usr/bin/make -j${JOBS:-16} $* =="
exec /usr/bin/make -j"${JOBS:-16}" STRAWBERRY_PERL="$STRAWBERRY_PERL" "$@"