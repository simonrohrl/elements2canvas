#!/bin/bash
# Build script for Skia gradient text test
#
# Prerequisites:
#   - Skia source cloned and built in ../skia directory
#   - Or adjust SKIA_DIR below

set -e

SKIA_DIR="${SKIA_DIR:-$(pwd)/../skia}"
SKIA_BUILD="${SKIA_DIR}/out/Release"

echo "Using Skia from: ${SKIA_DIR}"
echo "Build directory: ${SKIA_BUILD}"

if [ ! -d "${SKIA_DIR}" ]; then
    echo ""
    echo "Skia not found. To set up Skia:"
    echo ""
    echo "  cd $(dirname $0)/.."
    echo "  git clone https://skia.googlesource.com/skia.git"
    echo "  cd skia"
    echo "  python3 tools/git-sync-deps"
    echo "  bin/gn gen out/Release --args='is_official_build=true skia_use_system_libjpeg_turbo=false skia_use_system_libpng=false skia_use_system_zlib=false skia_use_libwebp_encode=false skia_use_libwebp_decode=false'"
    echo "  ninja -C out/Release"
    echo ""
    exit 1
fi

if [ ! -f "${SKIA_BUILD}/libskia.a" ]; then
    echo "Skia not built. Run:"
    echo "  cd ${SKIA_DIR}"
    echo "  ninja -C out/Release"
    exit 1
fi

echo "Compiling gradient_text_test.cpp..."

clang++ -std=c++17 \
    -I "${SKIA_DIR}" \
    -L "${SKIA_BUILD}" \
    -lskia \
    -framework CoreFoundation \
    -framework CoreGraphics \
    -framework CoreText \
    -framework CoreServices \
    -framework ApplicationServices \
    gradient_text_test.cpp \
    -o gradient_text_test

echo "Build successful!"
echo ""
echo "Run with: ./gradient_text_test"
echo "Output will be: gradient_mask_test.png"
