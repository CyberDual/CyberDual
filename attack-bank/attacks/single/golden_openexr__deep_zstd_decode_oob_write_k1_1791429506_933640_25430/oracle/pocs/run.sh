#!/usr/bin/env bash
set -eu

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="$repo_dir/_build/asan"

cmake -S "$repo_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
    -DCMAKE_CXX_FLAGS='-fsanitize=address -fno-omit-frame-pointer' \
    -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address' \
    -DCMAKE_SHARED_LINKER_FLAGS='-fsanitize=address' \
    -DOPENEXR_BUILD_TOOLS=OFF \
    -DOPENEXR_BUILD_EXAMPLES=OFF \
    -DOPENEXR_FORCE_INTERNAL_ZSTD=ON
cmake --build "$build_dir" -j 12 --target OpenEXR

g++ -std=c++17 -fsanitize=address -fno-omit-frame-pointer -g \
    -I "$repo_dir/src/lib/OpenEXR" \
    -I "$repo_dir/src/lib/OpenEXRCore" \
    -I "$repo_dir/src/lib/Iex" \
    -I "$build_dir/cmake" \
    -I /usr/include/Imath \
    "$repo_dir/pocs/residual.cpp" \
    -L "$build_dir/src/lib/OpenEXR" \
    -L "$build_dir/src/lib/OpenEXRCore" \
    -L "$build_dir/src/lib/Iex" \
    -lOpenEXR-4_0_d -lOpenEXRCore-4_0_d -lIex-4_0_d -lzstd \
    -Wl,-rpath,"$build_dir/src/lib/OpenEXR" \
    -Wl,-rpath,"$build_dir/src/lib/OpenEXRCore" \
    -Wl,-rpath,"$build_dir/src/lib/Iex" \
    -o "$build_dir/residual_poc"

ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 "$build_dir/residual_poc"
