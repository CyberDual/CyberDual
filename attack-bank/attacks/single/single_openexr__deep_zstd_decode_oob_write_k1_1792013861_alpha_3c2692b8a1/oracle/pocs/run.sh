#!/bin/bash
# Build and run pocs/newbug.cpp against an AddressSanitizer build of OpenEXR.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/pocs/newbug.cpp"
BIN="${TMPDIR:-/tmp}/exr_newbug"

search_roots=(
    "${OPENEXR_BUILD:-}"
    "$ROOT/build"
    "$ROOT/build-asan"
    "$ROOT/out"
    "/tmp/exr-parent"
    "/tmp/exr-build"
)

cfg=""
libdir=""
for r in "${search_roots[@]}"; do
    [ -n "$r" ] || continue
    [ -d "$r" ] || continue
    if [ -z "$cfg" ]; then
        hit="$(find "$r" -name OpenEXRConfig.h -print -quit 2>/dev/null || true)"
        [ -n "$hit" ] && cfg="$(dirname "$hit")"
    fi
    if [ -z "$libdir" ]; then
        hit="$(find "$r" -name 'libOpenEXRCore-4_0.so*' -print -quit 2>/dev/null || true)"
        [ -n "$hit" ] && libdir="$(dirname "$hit")"
    fi
done
if [ -z "$libdir" ]; then
    hit="$(find "$ROOT" /tmp -name 'libOpenEXRCore-4_0.so*' -print -quit 2>/dev/null || true)"
    [ -n "$hit" ] && libdir="$(dirname "$hit")"
fi
if [ -z "$cfg" ]; then
    hit="$(find "$ROOT" /tmp -name OpenEXRConfig.h -print -quit 2>/dev/null || true)"
    [ -n "$hit" ] && cfg="$(dirname "$hit")"
fi
if [ -z "$libdir" ] || [ -z "$cfg" ]; then
    echo "OpenEXR ASan build not found (set OPENEXR_BUILD)" >&2
    exit 2
fi

core_libdir="$libdir"
iex_libdir="$libdir"
ilm_libdir="$libdir"
exr_libdir="$libdir"
if [ -n "$libdir" ]; then
    parent="$(dirname "$libdir")"
    [ -d "$parent/OpenEXR" ] && exr_libdir="$parent/OpenEXR"
    [ -d "$parent/Iex" ] && iex_libdir="$parent/Iex"
    [ -d "$parent/IlmThread" ] && ilm_libdir="$parent/IlmThread"
    [ -d "$parent/OpenEXRCore" ] && core_libdir="$parent/OpenEXRCore"
fi

cflags="-fsanitize=address -fno-omit-frame-pointer -g -O1 -pthread"
cflags="$cflags -I$ROOT/src/lib/OpenEXR -I$ROOT/src/lib/OpenEXRCore"
cflags="$cflags -I$ROOT/src/lib/Iex -I$ROOT/src/lib/IlmThread"
if [ -n "$cfg" ]; then
    cflags="$cflags -I$cfg"
fi
if [ -d /usr/include/Imath ]; then
    cflags="$cflags -I/usr/include/Imath"
fi

ldflags="-fsanitize=address -pthread"
ldflags="$ldflags -L$exr_libdir -L$core_libdir -L$ilm_libdir -L$iex_libdir"
ldflags="$ldflags -Wl,-rpath,$exr_libdir:$core_libdir:$ilm_libdir:$iex_libdir"
libs="-lOpenEXR-4_0 -lOpenEXRCore-4_0 -lIlmThread-4_0 -lIex-4_0 -lImath -lzstd -ldeflate"

c++ $cflags -o "$BIN" "$SRC" $ldflags $libs

asan=""
for cand in \
    /usr/lib/gcc/x86_64-linux-gnu/12/libasan.so \
    /usr/lib/x86_64-linux-gnu/libasan.so.8
do
    if [ -f "$cand" ]; then
        asan="$cand"
        break
    fi
done

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=1:abort_on_error=1:detect_leaks=0}"
if [ -n "$asan" ]; then
    export LD_PRELOAD="$asan${LD_PRELOAD:+:$LD_PRELOAD}"
fi
"$BIN"
exit $?
