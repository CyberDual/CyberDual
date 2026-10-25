#!/bin/bash
set -euo pipefail
root=$(pwd)
paths=()
if [[ -n "${LD_LIBRARY_PATH:-}" ]]; then
    IFS=':' read -r -a paths <<< "$LD_LIBRARY_PATH"
fi
paths+=("$root" "${OPENEXR_BUILD_DIR:-}" "$root/build" "$root/_build" /tmp/exr-build)
lib=""
for p in "${paths[@]}"; do
    [[ -z "$p" || ! -d "$p" ]] && continue
    lib=$(find "$p" -name 'libOpenEXRCore.so' 2>/dev/null | head -1)
    [[ -n "$lib" ]] && break
done
cfg=""
for p in "$root" "${OPENEXR_BUILD_DIR:-}" "$root/build" "$root/_build" /tmp/exr-build "$(dirname "${lib:-}")"; do
    [[ -z "$p" || ! -d "$p" ]] && continue
    cfg=$(find "$p" -name 'OpenEXRConfig.h' 2>/dev/null | head -1)
    [[ -n "$cfg" ]] && break
done
if [[ -z "${lib}" || -z "${cfg}" ]]; then
    echo "OpenEXRCore build not found" >&2
    exit 1
fi
cc -fsanitize=address -fno-omit-frame-pointer -g -O1 \
    -I"$root/src/lib/OpenEXRCore" -I"$(dirname "$cfg")" \
    "$root/pocs/residual.c" \
    -L"$(dirname "$lib")" -Wl,-rpath,"$(dirname "$lib")" \
    -lOpenEXRCore -lz -o /tmp/residual_poc
ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=1:detect_leaks=0}" /tmp/residual_poc
