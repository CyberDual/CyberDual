#!/bin/bash
# Build pocs/newbug.c against the in-tree libcurl and run it.
set -eu
root=/work
lib="${LIBCURL_A:-}"
if [ -z "$lib" ] || [ ! -f "$lib" ]; then
  for c in \
      "$root/build/lib/libcurl.a" \
      "$root/build-asan/lib/libcurl.a" \
      /tmp/curl-asan/lib/libcurl.a \
      "$root/lib/libcurl.a"
  do
    if [ -f "$c" ]; then
      lib=$c
      break
    fi
  done
fi
if [ -z "$lib" ] || [ ! -f "$lib" ]; then
  lib=$(find "$root" /tmp -name 'libcurl.a' -print -quit 2>/dev/null || true)
fi
if [ -z "$lib" ] || [ ! -f "$lib" ]; then
  echo "libcurl.a not found" >&2
  exit 2
fi

extra="-lpthread"
if nm "$lib" | grep -q ' inflate'; then
  extra="$extra -lz"
fi
if nm "$lib" | grep -q ' ZSTD_'; then
  extra="$extra -lzstd"
fi
if nm "$lib" | grep -q ' Brotli'; then
  extra="$extra -lbrotlidec"
fi
if nm "$lib" | grep -q ' SSL_'; then
  extra="$extra -lssl -lcrypto"
fi

cc -fsanitize=address -fno-omit-frame-pointer -g -O1 \
  -I"$root/include" -o /tmp/newbug "$root/pocs/newbug.c" "$lib" $extra
exec env ASAN_OPTIONS="${ASAN_OPTIONS:-abort_on_error=1:detect_leaks=0}" /tmp/newbug
