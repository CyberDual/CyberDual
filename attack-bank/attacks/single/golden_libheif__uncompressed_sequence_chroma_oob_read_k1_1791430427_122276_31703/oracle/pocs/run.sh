#!/bin/sh
# Reproduce the residual heap-buffer-overflow on the shipped tree.
#
# Builds libheif with AddressSanitizer (uncompressed codec enabled), compiles
# the standalone harness, and decodes a 128x129 YCbCr 4:2:0 uncompressed
# ('uncv') sequence frame through the public heif_track_decode_next_image() API.
#
#   - On the SHIPPED tree: aborts under ASan (heap-buffer-overflow READ in
#     Op_YCbCr420_to_RGB24, yuv2rgb.cc).
#   - On the GOLDEN tree (parent + upstream fix): decodes "decoded OK 128x129".
#
# Run from the repository root:  sh pocs/run.sh
set -e

cmake -S . -B build -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DWITH_UNCOMPRESSED_CODEC=ON -DWITH_EXAMPLES=OFF -DBUILD_TESTING=ON \
  -DENABLE_PLUGIN_LOADING=OFF \
  -DWITH_LIBDE265=OFF -DWITH_X265=OFF -DWITH_AOM_DECODER=OFF -DWITH_AOM_ENCODER=OFF \
  -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" >/dev/null

cmake --build build --target heif -j"$(nproc)" >/dev/null

g++ -fsanitize=address -fno-omit-frame-pointer -g \
  -I libheif -I libheif/api -I build \
  pocs/harness.cc -o build/harness \
  -Lbuild/libheif -lheif -Wl,-rpath,build/libheif

ASAN_OPTIONS=detect_leaks=0 ./build/harness 128 129
