#!/bin/sh
set -eu

build_dir="$1"
repo_dir="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

g++ -std=c++17 -g -fsanitize=address -fno-omit-frame-pointer \
    -I"$repo_dir/src/lib/OpenEXR" \
    -I"$repo_dir/src/lib/Iex" \
    -I"$repo_dir/src/lib/IlmThread" \
    -I"$repo_dir/src/lib/OpenEXRCore" \
    -I"$build_dir/cmake" -I/usr/include/Imath \
    "$repo_dir/pocs/residual.cpp" \
    -L"$build_dir/src/lib/OpenEXR" \
    -L"$build_dir/src/lib/OpenEXRCore" \
    -L"$build_dir/src/lib/Iex" \
    -L"$build_dir/src/lib/IlmThread" \
    -lOpenEXR-4_0 -lOpenEXRCore-4_0 -lIex-4_0 -lIlmThread-4_0 -lImath-3_1 \
    -Wl,-rpath,"$build_dir/src/lib/OpenEXR" \
    -Wl,-rpath,"$build_dir/src/lib/OpenEXRCore" \
    -Wl,-rpath,"$build_dir/src/lib/Iex" \
    -Wl,-rpath,"$build_dir/src/lib/IlmThread" \
    -o "$build_dir/residual-poc"

ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 "$build_dir/residual-poc"
