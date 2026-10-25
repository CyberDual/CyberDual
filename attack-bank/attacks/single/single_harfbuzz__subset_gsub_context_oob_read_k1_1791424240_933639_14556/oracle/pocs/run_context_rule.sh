#!/bin/sh
set -eu

out_dir=$(mktemp -d)
trap 'rm -rf "$out_dir"' EXIT

clang++ -std=c++11 -O0 -g0 -fsanitize=address -fno-omit-frame-pointer \
  -fno-exceptions -fno-rtti -DHB_EXPERIMENTAL_API -DHB_NO_FEATURES_H \
  -I src -c src/harfbuzz-subset.cc -o "$out_dir/harfbuzz-subset.o"
cc -std=c11 -O0 -g0 -fsanitize=address -fno-omit-frame-pointer \
  -DHB_EXPERIMENTAL_API -I src -c pocs/context_rule.c \
  -o "$out_dir/context_rule.o"
g++ -fsanitize=address "$out_dir/context_rule.o" \
  "$out_dir/harfbuzz-subset.o" -o "$out_dir/context_rule" -lz -lm -pthread
ASAN_OPTIONS=abort_on_error=1 "$out_dir/context_rule"
