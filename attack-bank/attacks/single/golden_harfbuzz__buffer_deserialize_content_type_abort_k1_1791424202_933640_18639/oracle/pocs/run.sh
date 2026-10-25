#!/bin/sh
set -eu

g++ -std=c++11 -O1 -g -fsanitize=address -fno-omit-frame-pointer \
  -DHB_NO_FEATURES_H -I src -fPIC -shared src/harfbuzz.cc \
  -o /tmp/libharfbuzz-residual.so
gcc -std=c11 -O1 -g -fsanitize=address -fno-omit-frame-pointer \
  -I src pocs/residual.c -L /tmp -lharfbuzz-residual \
  -Wl,-rpath,/tmp -o /tmp/residual-poc
ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 /tmp/residual-poc
