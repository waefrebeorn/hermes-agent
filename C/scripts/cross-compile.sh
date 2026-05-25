#!/bin/bash
# cross-compile.sh — Cross-compile Hermes C for target platforms
# Usage: ./cross-compile.sh [target]
# Targets: linux-x86_64 (default), linux-aarch64, linux-armv7, windows-x86_64
#
# Requires cross-compilation toolchains:
#   sudo apt-get install gcc-aarch64-linux-gnu gcc-arm-linux-gnueabihf mingw-w64

set -euo pipefail
CDIR="$(cd "$(dirname "$0")" && pwd)"

# Default target
TARGET="${1:-linux-x86_64}"

case "$TARGET" in
    linux-x86_64)
        CC="gcc"
        AR="ar"
        CFLAGS="-O2 -g -Wall -Wextra -Wpedantic"
        LDFLAGS="-lssl -lcrypto -ldl -lpthread -lm"
        SUFFIX=""
        ;;
    linux-aarch64)
        CC="aarch64-linux-gnu-gcc"
        AR="aarch64-linux-gnu-ar"
        CFLAGS="-O2 -g -Wall -Wextra -Wpedantic"
        LDFLAGS="-lssl -lcrypto -ldl -lpthread -lm"
        SUFFIX="-aarch64"
        ;;
    linux-armv7)
        CC="arm-linux-gnueabihf-gcc"
        AR="arm-linux-gnueabihf-ar"
        CFLAGS="-O2 -g -Wall -Wextra -Wpedantic -march=armv7-a"
        LDFLAGS="-lssl -lcrypto -ldl -lpthread -lm"
        SUFFIX="-armv7"
        ;;
    windows-x86_64)
        CC="x86_64-w64-mingw32-gcc"
        AR="x86_64-w64-mingw32-ar"
        CFLAGS="-O2 -g -Wall -Wextra -Wpedantic -D_WIN32_WINNT=0x0A00"
        LDFLAGS="-lssl -lcrypto -lpthread -lm -lws2_32"
        SUFFIX=".exe"
        ;;
    *)
        echo "Usage: $0 [target]"
        echo "Targets: linux-x86_64, linux-aarch64, linux-armv7, windows-x86_64"
        exit 1
        ;;
esac

echo "=== Cross-compiling Hermes C for $TARGET ==="
echo "CC: $CC"
echo "CFLAGS: $CFLAGS"
echo "LDFLAGS: $LDFLAGS"
echo ""

# Check toolchain
if ! command -v "$CC" &>/dev/null; then
    echo "ERROR: $CC not found. Install the cross-compilation toolchain."
    echo "  linux-aarch64: sudo apt-get install gcc-aarch64-linux-gnu"
    echo "  linux-armv7:   sudo apt-get install gcc-arm-linux-gnueabihf"
    echo "  windows:       sudo apt-get install mingw-w64"
    exit 1
fi

# Build standalone libraries
echo "--- Building libraries ---"
for lib in lib/libjson lib/libhttp lib/libtemplate lib/libyaml lib/libcrypto \
           lib/libdotenv lib/libcron lib/libproc lib/libtui lib/libdb \
           lib/libplugin lib/libskin lib/libwebsocket lib/libprotobuf lib/libmcp; do
    if [ -f "$CDIR/$lib/Makefile" ]; then
        make -C "$CDIR/$lib" CC="$CC" AR="$AR" CFLAGS="$CFLAGS" 2>&1 | tail -1 || true
    fi
done

# Build hermes binary
echo "--- Building hermes$SUFFIX ---"
export CC AR CFLAGS LDFLAGS
make -C "$CDIR" hermes 2>&1 | tail -5

# Rename binary with target suffix
if [ -n "$SUFFIX" ]; then
    mv "$CDIR/hermes" "$CDIR/hermes$SUFFIX"
fi

echo ""
echo "=== Done: $TARGET build ==="
ls -lh "$CDIR/hermes$SUFFIX" 2>/dev/null || ls -lh "$CDIR/hermes"
