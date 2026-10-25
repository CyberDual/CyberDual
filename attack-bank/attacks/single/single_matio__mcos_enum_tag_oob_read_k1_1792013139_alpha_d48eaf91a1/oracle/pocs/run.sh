#!/bin/bash
set -euo pipefail
root=/work
lib=$(find "$root" /tmp -name 'libmatio.so' -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -1 | cut -d' ' -f2-)
if [ -z "${lib}" ]; then
    echo "libmatio.so not found" >&2
    exit 2
fi
libdir=$(dirname "$lib")
pub=$(find "$root" /tmp -name 'matio_pubconf.h' -printf '%T@ %h\n' 2>/dev/null | sort -nr | head -1 | cut -d' ' -f2-)
if [ -z "${pub}" ]; then
    echo "matio_pubconf.h not found" >&2
    exit 2
fi
cc -fsanitize=address -fno-omit-frame-pointer -g -O1 -I"$root/src" -I"$pub" -o /tmp/poc_newbug "$root/pocs/newbug.c" -L"$libdir" -lmatio -Wl,-rpath,"$libdir"
ASAN_OPTIONS="${ASAN_OPTIONS:-abort_on_error=1:halt_on_error=1}" /tmp/poc_newbug
