#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "$0")/.." && pwd)
source_dir=${OPENEXR_SOURCE_ROOT:-$repo_dir}
build_dir=${OPENEXR_ASAN_BUILD:-/tmp/openexr-asan-make}
lib_path="$build_dir/src/lib/OpenEXR:$build_dir/src/lib/IlmThread:$build_dir/src/lib/Iex:$build_dir/src/lib/OpenEXRCore"

g++ -std=c++17 -g -fsanitize=address -fno-omit-frame-pointer \
    -I"$source_dir/src/lib/OpenEXR" -I"$build_dir/cmake" \
    -I"$source_dir/src/lib/Iex" -I"$source_dir/src/lib/IlmThread" \
    -I"$source_dir/src/lib/OpenEXRCore" -I/usr/include/Imath \
    "$repo_dir/pocs/header_roundtrip.cpp" -o "$build_dir/header_roundtrip_asan" \
    -L"$build_dir/src/lib/OpenEXR" -L"$build_dir/src/lib/IlmThread" \
    -L"$build_dir/src/lib/Iex" -L"$build_dir/src/lib/OpenEXRCore" \
    -Wl,-rpath,"$lib_path" \
    -lOpenEXR-4_0_d -lIlmThread-4_0_d -lIex-4_0_d \
    -lOpenEXRCore-4_0_d -lImath-3_1

ASAN_OPTIONS=abort_on_error=1:detect_leaks=0 "$build_dir/header_roundtrip_asan"
