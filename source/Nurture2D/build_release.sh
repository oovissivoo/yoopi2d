#!/bin/bash
# Out-of-source release build script for Nurture2D
# Usage: ./build_release.sh [build_dir]
#
# Qt release build flags used:
#   CONFIG+=release        - enables Qt release mode (O3, NDEBUG, stripped)
#   CONFIG+=NO_TESTS       - skips building the test suite
#   PENCIL2D_RELEASE=1     - enables release-specific defines (no debug output)

set -e

BUILD_DIR="${1:-build-release}"

echo "==> Configuring release build in: $BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Run qmake from the build directory (out-of-source)
cd "$BUILD_DIR"
qmake ../pencil2d.pro \
    CONFIG+=release \
    CONFIG+=NO_TESTS \
    PENCIL2D_RELEASE=1

echo "==> Building..."
make -j"$(nproc)"

echo ""
echo "==> Done. Binary: $BUILD_DIR/app/nurture2d"
