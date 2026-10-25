#!/bin/sh
# Build a tiny ASan FreeType (winfonts only) from this tree and load the crafted FON.
set -eu
cd "$(dirname "$0")/.."

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

mkdir -p "$TMP/inc/freetype/config"
cat > "$TMP/inc/freetype/config/ftmodule.h" << 'EOF'
FT_USE_MODULE( FT_Driver_ClassRec, winfnt_driver_class )
EOF

CC=${CC:-gcc}
SAN="-fsanitize=address -fno-omit-frame-pointer -g -O1"
INC="-I$TMP/inc -Iinclude"

$CC $SAN -std=c99 -Wall -DFT2_BUILD_LIBRARY $INC -c src/base/ftsystem.c -o "$TMP/ftsystem.o"
$CC $SAN -std=c99 -Wall -DFT2_BUILD_LIBRARY $INC -c src/base/ftinit.c   -o "$TMP/ftinit.o"
$CC $SAN -std=c99 -Wall -DFT2_BUILD_LIBRARY $INC -c src/base/ftdebug.c  -o "$TMP/ftdebug.o"
$CC $SAN -std=c99 -Wall -DFT2_BUILD_LIBRARY $INC -c src/base/ftbase.c   -o "$TMP/ftbase.o"
$CC $SAN -std=c99 -Wall -DFT2_BUILD_LIBRARY $INC -c src/winfonts/winfnt.c -o "$TMP/winfnt.o"
$CC $SAN -std=c99 -Wall -Iinclude pocs/rsrc_name.c \
    "$TMP/ftsystem.o" "$TMP/ftinit.o" "$TMP/ftdebug.o" "$TMP/ftbase.o" "$TMP/winfnt.o" \
    -o "$TMP/poc"

export ASAN_OPTIONS="${ASAN_OPTIONS:-abort_on_error=1:detect_leaks=0}"
# Force abort so a heap-buffer-overflow is reported as signal 134.
ASAN_OPTIONS="abort_on_error=1:detect_leaks=0" "$TMP/poc"
